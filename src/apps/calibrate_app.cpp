#include <apps/calibrate_app.h>

#include <apps/app_common.h>
#include <board/input.h>
#include <ui.h>

static bool last_back_pressed;

void initCalibrateApp() {
    last_back_pressed = isSelectButtonPressed();
    my_screen = createAppScreen("Calibrate", initUI);

    // Put code here.

    lv_scr_load(my_screen);
}

void updateCalibrateApp() {
    updateAppBackButton(isSelectButtonPressed(), &last_back_pressed);
}
