#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include <board/pins.h>
#include <board/setup.h>
#include <board/input.h>


void setup() {

  Serial.begin(115200);
  delay(500);
  Serial.print("Activating Display ");
  setupDisplay();
  Serial.println("✔");
  Serial.println("Activating Inputs:");
  initInputs();
  Serial.print("Activating UI ");
  initUI();
  Serial.println("✔");
  Serial.println("\nSuccess!");
}

void loop() {
  updateMotionSample();
  lv_timer_handler();
  updateUI();
  
  delay(5);
}
