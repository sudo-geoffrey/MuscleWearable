#include <ui.h>

#include <Arduino.h>
#include <apps/calibrate_app.h>
#include <apps/detect_app.h>
#include <apps/graph_app.h>
#include <apps/record_app.h>
#include <debug_ui.h>
#include <board/input.h>
#include <lvgl.h>
#include <wallpaper.h>

lv_obj_t *my_screen;

#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

#ifndef UI_UPSIDE_DOWN
#define UI_UPSIDE_DOWN 0
#endif

enum ActiveScreen {
    SCREEN_HOME,
    SCREEN_GRAPH,
    SCREEN_DETECT,
    SCREEN_RECORD,
    SCREEN_CALIBRATE,
};

static const char *MENU_TITLES[] = {"Graph", "Detect", "Record", "Calibrate"};
static const char *MENU_DETAILS[] = {
    "Live EMG + gyro",
    "Gestures + reps",
    "Save sessions",
    "Zero baseline"
};
static const uint32_t MENU_COLORS[] = {0x00E5FF, 0x7CFF8A, 0xFFE066, 0xFF8A65};
static const int MENU_COUNT = 4;

static lv_obj_t *menu_tiles[MENU_COUNT];
static lv_obj_t *menu_title_labels[MENU_COUNT];
static lv_obj_t *menu_detail_labels[MENU_COUNT];
static lv_obj_t *battery_label;
static int selected_index;
static bool last_select_pressed;
static bool last_rotate_pressed;
static uint32_t last_battery_update_ms;
static ActiveScreen active_screen = SCREEN_HOME;

static void initHintLabels(lv_obj_t *screen) {
    lv_obj_t *select_hint = lv_label_create(screen);
    lv_label_set_text(select_hint, "1 Select");
    lv_obj_set_style_text_color(select_hint, lv_color_hex(0xC7D0D9), 0);
    lv_obj_set_style_text_font(select_hint, &lv_font_montserrat_10, 0);
#if UI_UPSIDE_DOWN
    lv_obj_align(select_hint, LV_ALIGN_TOP_RIGHT, -4, 2);
#else
    lv_obj_align(select_hint, LV_ALIGN_TOP_LEFT, 4, 2);
#endif

    lv_obj_t *rotate_hint = lv_label_create(screen);
    lv_label_set_text(rotate_hint, "2 Rotate");
    lv_obj_set_style_text_color(rotate_hint, lv_color_hex(0xC7D0D9), 0);
    lv_obj_set_style_text_font(rotate_hint, &lv_font_montserrat_10, 0);
#if UI_UPSIDE_DOWN
    lv_obj_align(rotate_hint, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
#else
    lv_obj_align(rotate_hint, LV_ALIGN_BOTTOM_LEFT, 4, -2);
#endif
}

static void updateMenuSelection() {
    for (int i = 0; i < MENU_COUNT; i++) {
        bool selected = i == selected_index;
        uint32_t border = selected ? MENU_COLORS[i] : 0x2B3845;
        uint32_t bg = selected ? 0x24313D : 0x17212B;

        lv_obj_set_style_bg_color(menu_tiles[i], lv_color_hex(bg), 0);
        lv_obj_set_style_bg_opa(menu_tiles[i], selected ? LV_OPA_90 : LV_OPA_70, 0);
        lv_obj_set_style_border_color(menu_tiles[i], lv_color_hex(border), 0);
        lv_obj_set_style_border_width(menu_tiles[i], selected ? 3 : 1, 0);
        lv_obj_set_style_text_color(menu_title_labels[i], lv_color_hex(MENU_COLORS[i]), 0);
        lv_obj_set_style_text_color(menu_detail_labels[i], lv_color_hex(selected ? 0xFFFFFF : 0xC7D0D9), 0);
    }
}

static uint8_t getBatteryPercent(uint32_t battery_mv) {
    const uint32_t empty_mv = 3300;
    const uint32_t full_mv = 4200;

    if (battery_mv <= empty_mv) {
        return 0;
    }
    if (battery_mv >= full_mv) {
        return 100;
    }

    return static_cast<uint8_t>(((battery_mv - empty_mv) * 100) / (full_mv - empty_mv));
}

static void updateBatteryLabel(bool force = false) {
    if (battery_label == NULL) {
        return;
    }

    uint32_t now_ms = millis();
    if (!force && now_ms - last_battery_update_ms < 1000) {
        return;
    }
    last_battery_update_ms = now_ms;

    uint32_t battery_mv = getBatteryVoltageMv();
    if (!isBatteryConnected(battery_mv)) {
        lv_label_set_text(battery_label, "USB");
        return;
    }

    lv_label_set_text_fmt(battery_label, "%u%%", getBatteryPercent(battery_mv));
}

static void createMenuTile(int index, int x, int y) {
    lv_obj_t *tile = lv_obj_create(my_screen);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tile, 126, 50);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_style_radius(tile, 6, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x17212B), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_70, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(0x2B3845), 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_pad_all(tile, 6, 0);

    lv_obj_t *title = lv_label_create(tile);
    lv_label_set_text(title, MENU_TITLES[index]);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *detail = lv_label_create(tile);
    lv_label_set_text(detail, MENU_DETAILS[index]);
    lv_obj_set_width(detail, 110);
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_10, 0);
    lv_obj_align(detail, LV_ALIGN_TOP_LEFT, 0, 23);

    menu_tiles[index] = tile;
    menu_title_labels[index] = title;
    menu_detail_labels[index] = detail;
}

static void initHomeMenuUI() {
    active_screen = SCREEN_HOME;
    my_screen = lv_obj_create(NULL);
    styleWallpaperScreen(my_screen);
    addWallpaper(my_screen);
    initHintLabels(my_screen);

    lv_obj_t *title = lv_label_create(my_screen);
    lv_label_set_text(title, "GeoffBit");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    battery_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0x7CFF8A), 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_12, 0);
#if UI_UPSIDE_DOWN
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, 6, 6);
#else
    lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -6, 6);
#endif

    createMenuTile(0, 54, 35);
    createMenuTile(1, 188, 35);
    createMenuTile(2, 54, 92);
    createMenuTile(3, 188, 92);

    selected_index = 0;
    last_select_pressed = isSelectButtonPressed();
    last_rotate_pressed = isRotateButtonPressed();
    last_battery_update_ms = 0;
    updateMenuSelection();
    updateBatteryLabel(true);
    lv_scr_load(my_screen);
}

static void openSelectedApp() {
    switch (selected_index) {
        case 0:
            active_screen = SCREEN_GRAPH;
            initGraphApp();
            break;
        case 1:
            active_screen = SCREEN_DETECT;
            initDetectApp();
            break;
        case 2:
            active_screen = SCREEN_RECORD;
            initRecordApp();
            break;
        case 3:
            active_screen = SCREEN_CALIBRATE;
            initCalibrateApp();
            break;
    }
}

void initUI() {
    if (DEBUG_MODE) {
        initDebugUI();
        return;
    }

    initHomeMenuUI();
}

void updateUI() {
    if (DEBUG_MODE) {
        updateDebugUI();
        return;
    }

    if (active_screen == SCREEN_GRAPH) {
        updateGraphApp();
        return;
    }
    if (active_screen == SCREEN_DETECT) {
        updateDetectApp();
        return;
    }
    if (active_screen == SCREEN_RECORD) {
        updateRecordApp();
        return;
    }
    if (active_screen == SCREEN_CALIBRATE) {
        updateCalibrateApp();
        return;
    }

    bool select_pressed = isSelectButtonPressed();
    bool rotate_pressed = isRotateButtonPressed();
    updateBatteryLabel();

    if (rotate_pressed && !last_rotate_pressed) {
        selected_index = (selected_index + 1) % MENU_COUNT;
        updateMenuSelection();
    }

    if (select_pressed && !last_select_pressed) {
        openSelectedApp();
    }

    last_select_pressed = select_pressed;
    last_rotate_pressed = rotate_pressed;
}
