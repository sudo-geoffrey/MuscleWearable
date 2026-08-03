#ifndef APP_COMMON_H
#define APP_COMMON_H

#include <lvgl.h>

typedef void (*BackCallback)();

lv_obj_t *createAppScreen(const char *title, BackCallback backCallback);
void updateAppBackButton(bool button1Pressed, bool *lastButton1Pressed);

#endif
