#include <debug_ui.h>

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <board/input.h>
#include <lvgl.h>
#include <board/pins.h>
#include <ui.h>
#include <wallpaper.h>

#ifndef UI_UPSIDE_DOWN
#define UI_UPSIDE_DOWN 0
#endif

static lv_obj_t *debug_buttons;
static lv_obj_t *debug_voltage;
static lv_obj_t *debug_signal;
static lv_obj_t *debug_battery;
static lv_obj_t *debug_misc;
static uint32_t last_debug_update_ms;

static void initDebugHintLabels(lv_obj_t *screen) {
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

void initDebugUI() {
    my_screen = lv_obj_create(NULL);
    styleWallpaperScreen(my_screen);
    addWallpaper(my_screen);

    initDebugHintLabels(my_screen);

    lv_obj_t *debug_title = lv_label_create(my_screen);
    lv_label_set_text(debug_title, "DEBUG");
    lv_obj_set_style_text_color(debug_title, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(debug_title, &lv_font_montserrat_14, 0);
    lv_obj_align(debug_title, LV_ALIGN_TOP_MID, 0, 4);

    debug_buttons = lv_label_create(my_screen);
    lv_obj_set_style_text_color(debug_buttons, lv_color_white(), 0);
    lv_obj_set_style_text_font(debug_buttons, &lv_font_montserrat_12, 0);
    lv_obj_align(debug_buttons, LV_ALIGN_TOP_LEFT, 82, 24);

    debug_voltage = lv_label_create(my_screen);
    lv_obj_set_style_text_color(debug_voltage, lv_color_hex(0xFFE066), 0);
    lv_obj_set_style_text_font(debug_voltage, &lv_font_montserrat_12, 0);
    lv_obj_align(debug_voltage, LV_ALIGN_TOP_LEFT, 82, 60);

    debug_signal = lv_label_create(my_screen);
    lv_obj_set_style_text_color(debug_signal, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(debug_signal, &lv_font_montserrat_12, 0);
    lv_obj_align(debug_signal, LV_ALIGN_TOP_LEFT, 82, 80);

    debug_battery = lv_label_create(my_screen);
    lv_obj_set_style_text_color(debug_battery, lv_color_hex(0x7CFF8A), 0);
    lv_obj_set_style_text_font(debug_battery, &lv_font_montserrat_12, 0);
    lv_obj_align(debug_battery, LV_ALIGN_TOP_LEFT, 82, 100);

    debug_misc = lv_label_create(my_screen);
    lv_obj_set_style_text_color(debug_misc, lv_color_hex(0xC7D0D9), 0);
    lv_obj_set_style_text_font(debug_misc, &lv_font_montserrat_10, 0);
    lv_obj_align(debug_misc, LV_ALIGN_TOP_LEFT, 82, 122);

    lv_scr_load(my_screen);
    updateDebugUI();
}

void updateDebugUI() {
    if (debug_buttons == NULL) {
        return;
    }

    uint32_t now_ms = millis();
    if (now_ms - last_debug_update_ms < 250) {
        return;
    }
    last_debug_update_ms = now_ms;

    bool button1 = isButton1Pressed();
    bool button2 = isButton2Pressed();
    uint32_t battery_mv = getBatteryVoltageMv();
    uint32_t signal_mv = getGraphSignalMv();
    bool battery_connected = isBatteryConnected(battery_mv);
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t min_free_heap = ESP.getMinFreeHeap();
    uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int signal_raw = analogRead(PIN_GRAPH_SIGNAL);

    lv_label_set_text_fmt(
        debug_buttons,
        "B1 GPIO%d: %-5s r:%d\nB2 GPIO%d: %-5s r:%d",
        PIN_BUTTON_1,
        button1 ? "PRESS" : "open",
        digitalRead(PIN_BUTTON_1),
        PIN_BUTTON_2,
        button2 ? "PRESS" : "open",
        digitalRead(PIN_BUTTON_2)
    );

    lv_label_set_text_fmt(debug_voltage, "VBAT: %lu mV", battery_mv);
    lv_label_set_text_fmt(debug_signal, "GPIO%d: %lu mV  raw:%d", PIN_GRAPH_SIGNAL, signal_mv, signal_raw);

    lv_label_set_text_fmt(
        debug_battery,
        "BAT: %s",
        battery_connected ? "connected" : "USB/no batt"
    );

    lv_label_set_text_fmt(
        debug_misc,
        "UP:%lu.%03lus  LVGL:%lu\nHEAP:%lu  MIN:%lu\nPSRAM:%lu",
        now_ms / 1000,
        now_ms % 1000,
        lv_tick_get(),
        free_heap,
        min_free_heap,
        psram_free
    );

    Serial.printf(
        "DBG GPIO%d signal: %lu mV raw:%d t:%lu ms\n",
        PIN_GRAPH_SIGNAL,
        signal_mv,
        signal_raw,
        now_ms
    );
}
