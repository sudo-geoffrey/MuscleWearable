#include <wallpaper.h>

#include <generated_wallpaper.h>

void styleWallpaperScreen(lv_obj_t *screen) {
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0E141B), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
}

void addWallpaper(lv_obj_t *screen) {
#if APP_WALLPAPER_ENABLED
    lv_obj_t *wallpaper = lv_img_create(screen);
    lv_img_set_src(wallpaper, &generated_wallpaper);
    lv_obj_align(wallpaper, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_background(wallpaper);
#else
    (void)screen;
#endif
}
