#include <apps/app_common.h>

#include <wallpaper.h>

#ifndef UI_UPSIDE_DOWN
#define UI_UPSIDE_DOWN 0
#endif

static BackCallback active_back_callback = nullptr;

lv_obj_t *createAppScreen(const char *title, BackCallback backCallback) {
    active_back_callback = backCallback;

    lv_obj_t *screen = lv_obj_create(NULL);
    styleWallpaperScreen(screen);
    addWallpaper(screen);

    lv_obj_t *back_hint = lv_label_create(screen);
    lv_label_set_text(back_hint, "1 Back");
    lv_obj_set_style_text_color(back_hint, lv_color_hex(0xC7D0D9), 0);
    lv_obj_set_style_text_font(back_hint, &lv_font_montserrat_10, 0);
#if UI_UPSIDE_DOWN
    lv_obj_align(back_hint, LV_ALIGN_TOP_RIGHT, -4, 2);
#else
    lv_obj_align(back_hint, LV_ALIGN_TOP_LEFT, 4, 2);
#endif

    lv_obj_t *heading = lv_label_create(screen);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_color(heading, lv_color_white(), 0);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_16, 0);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 6);

    return screen;
}

void updateAppBackButton(bool button1Pressed, bool *lastButton1Pressed) {
    if (button1Pressed && !*lastButton1Pressed && active_back_callback != nullptr) {
        active_back_callback();
    }
    *lastButton1Pressed = button1Pressed;
}
