#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>

struct MotionSample {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float quat_i;
    float quat_j;
    float quat_k;
    float quat_real;
    uint32_t accel_ms;
    uint32_t gyro_ms;
    uint32_t quat_ms;
    bool bno_active;
    bool accel_valid;
    bool gyro_valid;
    bool quat_valid;
};

void initInputs();
bool isButton1Pressed();
bool isButton2Pressed();
bool isSelectButtonPressed();
bool isRotateButtonPressed();
uint32_t getBatteryVoltageMv();
bool isBatteryConnected(uint32_t batteryMv);
uint32_t getGraphSignalMv();
void updateMotionSample();
const MotionSample &getMotionSample();
bool isMotionSampleFresh(uint32_t maxAgeMs);
bool setMotionReportRateHz(uint16_t hz);

#endif
