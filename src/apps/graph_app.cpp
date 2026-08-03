#include <apps/graph_app.h>

#include <Arduino.h>
#include <apps/app_common.h>
#include <board/input.h>
#include <lvgl.h>
#include <ui.h>

enum GraphMode {
    GRAPH_EMG,
    GRAPH_ACCEL,
    GRAPH_GYRO,
    GRAPH_QUAT,
    GRAPH_MODE_COUNT,
};

static const char *MODE_NAMES[] = {"EMG", "Accel", "Gyro", "Quat"};
static const int CHART_POINTS = 250;
static const int MAX_SERIES = 4;
static const uint32_t SAMPLE_INTERVAL_MS = 20;
static const uint32_t LONG_PRESS_MS = 800;

static bool last_back_pressed;
static bool last_run_pressed;
static bool graph_running;
static bool back_press_active;
static uint32_t back_press_started_ms;
static uint32_t last_sample_ms;
static int write_index;
static int sample_count;
static int32_t min_sample_value;
static int32_t max_sample_value;
static GraphMode graph_mode;
static lv_obj_t *chart;
static lv_obj_t *mode_label;
static lv_obj_t *range_label;
static lv_obj_t *run_hint_label;
static lv_chart_series_t *series[MAX_SERIES];
static lv_coord_t samples[MAX_SERIES][CHART_POINTS];

static int activeSeriesCount() {
    switch (graph_mode) {
        case GRAPH_EMG:
            return 1;
        case GRAPH_ACCEL:
        case GRAPH_GYRO:
            return 3;
        case GRAPH_QUAT:
            return 4;
        default:
            return 1;
    }
}

static void clearGraphData() {
    write_index = 0;
    sample_count = 0;
    min_sample_value = 0;
    max_sample_value = 3300;

    for (int s = 0; s < MAX_SERIES; s++) {
        for (int i = 0; i < CHART_POINTS; i++) {
            samples[s][i] = LV_CHART_POINT_NONE;
            lv_chart_set_value_by_id(chart, series[s], i, LV_CHART_POINT_NONE);
        }
    }
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min_sample_value, max_sample_value);
    lv_chart_refresh(chart);
}

static void updateSeriesColors() {
    if (graph_mode == GRAPH_EMG) {
        lv_chart_set_series_color(chart, series[0], lv_color_hex(0x00E5FF));
    } else {
        lv_chart_set_series_color(chart, series[0], lv_color_hex(0xFF4D4D));
    }
    lv_chart_set_series_color(chart, series[1], lv_color_hex(0x7CFF8A));
    lv_chart_set_series_color(chart, series[2], lv_color_hex(0x4D8DFF));
    lv_chart_set_series_color(chart, series[3], lv_color_white());
}

static void updateModeLabel() {
    const char *legend = "";
    switch (graph_mode) {
        case GRAPH_ACCEL:
        case GRAPH_GYRO:
            legend = " X/Y/Z";
            break;
        case GRAPH_QUAT:
            legend = " i/j/k/r";
            break;
        default:
            break;
    }
    lv_label_set_text_fmt(mode_label, "%s%s", MODE_NAMES[graph_mode], legend);
}

static void updateGraphRange() {
    int count = activeSeriesCount();
    int32_t min_value = INT32_MAX;
    int32_t max_value = INT32_MIN;

    for (int s = 0; s < count; s++) {
        for (int i = 0; i < sample_count; i++) {
            if (samples[s][i] == LV_CHART_POINT_NONE) {
                continue;
            }
            int32_t sample = samples[s][i];
            if (sample < min_value) {
                min_value = sample;
            }
            if (sample > max_value) {
                max_value = sample;
            }
        }
    }

    if (min_value == INT32_MAX || max_value == INT32_MIN) {
        min_value = graph_mode == GRAPH_EMG ? 0 : -1000;
        max_value = graph_mode == GRAPH_EMG ? 3300 : 1000;
    }

    int32_t span = max_value - min_value;
    int32_t padding = span / 8;
    if (padding < 50) {
        padding = 50;
    }

    min_sample_value = min_value - padding;
    max_sample_value = max_value + padding;
    if (max_sample_value - min_sample_value < 100) {
        max_sample_value = min_sample_value + 100;
    }

    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, min_sample_value, max_sample_value);
    if (graph_mode == GRAPH_EMG) {
        lv_label_set_text_fmt(range_label, "%.2f-%.2fV", min_sample_value / 1000.0f, max_sample_value / 1000.0f);
    } else {
        lv_label_set_text_fmt(range_label, "%.2f..%.2f", min_sample_value / 1000.0f, max_sample_value / 1000.0f);
    }
}

static void setSampleValues(const int32_t *values, int value_count) {
    for (int s = 0; s < MAX_SERIES; s++) {
        lv_coord_t value = s < value_count ? static_cast<lv_coord_t>(values[s]) : LV_CHART_POINT_NONE;
        samples[s][write_index] = value;
        lv_chart_set_value_by_id(chart, series[s], write_index, value);
    }

    write_index = (write_index + 1) % CHART_POINTS;
    if (sample_count < CHART_POINTS) {
        sample_count++;
    }

    updateGraphRange();
    lv_chart_refresh(chart);
}

static void addGraphSample() {
    const MotionSample &motion = getMotionSample();
    int32_t values[MAX_SERIES] = {0, 0, 0, 0};
    int count = 1;

    switch (graph_mode) {
        case GRAPH_EMG:
            values[0] = static_cast<int32_t>(getGraphSignalMv());
            if (values[0] > 3300) {
                values[0] = 3300;
            }
            break;
        case GRAPH_ACCEL:
            values[0] = static_cast<int32_t>(motion.accel_x * 1000.0f);
            values[1] = static_cast<int32_t>(motion.accel_y * 1000.0f);
            values[2] = static_cast<int32_t>(motion.accel_z * 1000.0f);
            count = 3;
            break;
        case GRAPH_GYRO:
            values[0] = static_cast<int32_t>(motion.gyro_x * 1000.0f);
            values[1] = static_cast<int32_t>(motion.gyro_y * 1000.0f);
            values[2] = static_cast<int32_t>(motion.gyro_z * 1000.0f);
            count = 3;
            break;
        case GRAPH_QUAT:
            values[0] = static_cast<int32_t>(motion.quat_i * 1000.0f);
            values[1] = static_cast<int32_t>(motion.quat_j * 1000.0f);
            values[2] = static_cast<int32_t>(motion.quat_k * 1000.0f);
            values[3] = static_cast<int32_t>(motion.quat_real * 1000.0f);
            count = 4;
            break;
        default:
            break;
    }

    setSampleValues(values, count);
}

static void updateRunHint() {
    lv_label_set_text(run_hint_label, graph_running ? "2 Stop" : "2 Start");
}

static void cycleGraphMode() {
    graph_mode = static_cast<GraphMode>((graph_mode + 1) % GRAPH_MODE_COUNT);
    updateSeriesColors();
    clearGraphData();
    updateModeLabel();
}

void initGraphApp() {
    last_back_pressed = isSelectButtonPressed();
    last_run_pressed = isRotateButtonPressed();
    graph_running = true;
    back_press_active = false;
    back_press_started_ms = 0;
    last_sample_ms = 0;
    graph_mode = GRAPH_EMG;

    my_screen = createAppScreen("Graph", initUI);

    run_hint_label = lv_label_create(my_screen);
    lv_label_set_text(run_hint_label, "2 Stop");
    lv_obj_set_style_text_color(run_hint_label, lv_color_hex(0xC7D0D9), 0);
    lv_obj_set_style_text_font(run_hint_label, &lv_font_montserrat_10, 0);
#if UI_UPSIDE_DOWN
    lv_obj_align(run_hint_label, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
#else
    lv_obj_align(run_hint_label, LV_ALIGN_BOTTOM_LEFT, 4, -2);
#endif

    mode_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(mode_label, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_16, 0);
    lv_obj_align(mode_label, LV_ALIGN_TOP_RIGHT, -8, 30);

    range_label = lv_label_create(my_screen);
    lv_label_set_text(range_label, "0.00-3.30V");
    lv_obj_set_style_text_color(range_label, lv_color_hex(0xC7D0D9), 0);
    lv_obj_set_style_text_font(range_label, &lv_font_montserrat_10, 0);
    lv_obj_align(range_label, LV_ALIGN_TOP_RIGHT, -8, 52);

    chart = lv_chart_create(my_screen);
    lv_obj_set_size(chart, 304, 94);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_80, 0);
    lv_obj_set_style_border_color(chart, lv_color_hex(0x2B3845), 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 4, 0);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x2B3845), LV_PART_MAIN);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINTS);
    lv_chart_set_div_line_count(chart, 5, 6);

    series[0] = lv_chart_add_series(chart, lv_color_hex(0x00E5FF), LV_CHART_AXIS_PRIMARY_Y);
    series[1] = lv_chart_add_series(chart, lv_color_hex(0x7CFF8A), LV_CHART_AXIS_PRIMARY_Y);
    series[2] = lv_chart_add_series(chart, lv_color_hex(0x4D8DFF), LV_CHART_AXIS_PRIMARY_Y);
    series[3] = lv_chart_add_series(chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);

    updateSeriesColors();
    clearGraphData();
    updateModeLabel();

    lv_scr_load(my_screen);
}

void updateGraphApp() {
    bool run_pressed = isRotateButtonPressed();
    if (run_pressed && !last_run_pressed) {
        graph_running = !graph_running;
        updateRunHint();
    }
    last_run_pressed = run_pressed;

    bool back_pressed = isSelectButtonPressed();
    if (back_pressed && !last_back_pressed) {
        back_press_active = true;
        back_press_started_ms = millis();
    }
    if (!back_pressed && last_back_pressed && back_press_active) {
        uint32_t held_ms = millis() - back_press_started_ms;
        back_press_active = false;
        if (held_ms >= LONG_PRESS_MS) {
            initUI();
            return;
        }
        cycleGraphMode();
    }
    last_back_pressed = back_pressed;

    uint32_t now_ms = millis();
    if (graph_running && now_ms - last_sample_ms >= SAMPLE_INTERVAL_MS) {
        last_sample_ms = now_ms;
        addGraphSample();
    }
}
