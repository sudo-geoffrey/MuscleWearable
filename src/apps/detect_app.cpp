#include <apps/detect_app.h>

#include <apps/app_common.h>
#include <board/input.h>
#include <ui.h>

static bool last_back_pressed;

void initDetectApp() {
    last_back_pressed = isSelectButtonPressed();
    my_screen = createAppScreen("Detect", initUI);

    // Put code here.

    lv_scr_load(my_screen);
}

void updateDetectApp() {
    updateAppBackButton(isSelectButtonPressed(), &last_back_pressed);
}
