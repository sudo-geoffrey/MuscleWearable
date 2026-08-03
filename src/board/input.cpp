#include <board/input.h>
#include <Arduino.h>
#include <board/pins.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>

static Adafruit_BNO08x bno;
static MotionSample motion_sample;
static bool bno_active;
static uint16_t active_motion_hz;

static uint32_t reportIntervalUs(uint16_t hz) {
    if (hz == 0) {
        hz = 1;
    }
    return 1000000UL / hz;
}

static bool configureMotionReports(uint16_t hz, bool force) {
    if (!bno_active) {
        return false;
    }
    if (!force && hz == active_motion_hz) {
        return true;
    }

    uint32_t interval_us = reportIntervalUs(hz);
    bool ok = true;
    ok = bno.enableReport(SH2_GAME_ROTATION_VECTOR, interval_us) && ok;
    ok = bno.enableReport(SH2_ACCELEROMETER, interval_us) && ok;
    ok = bno.enableReport(SH2_GYROSCOPE_CALIBRATED, interval_us) && ok;
    if (ok) {
        active_motion_hz = hz;
    }
    return ok;
}

bool setMotionReportRateHz(uint16_t hz) {
    return configureMotionReports(hz, false);
}

void initInputs() {
    Serial.print("    Activating Button Input ");
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);
    Serial.println("✔");
    Serial.print("    Activating EMG Sensing ");
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT_VOLT, ADC_11db);
    analogSetPinAttenuation(PIN_GRAPH_SIGNAL, ADC_11db);
    Serial.println("✔");

    Serial.print("    Starting I2C ");

    Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);
    Wire.setClock(400000);
    Serial.println("✔");

    Serial.print("    Checking for BNO ");
    if (!bno.begin_I2C(0x4B, &Wire)) {
        Serial.println("IIC ERROR DETECTED: Debugging");
        Serial.println("Scanning I2C...");

        int devices = 0;

        for (uint8_t address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            uint8_t error = Wire.endTransmission();

            if (error == 0) {
                Serial.print("Found device at 0x");

                if (address < 16) {
                    Serial.print("0");
                }

                Serial.println(address, HEX);
                devices++;
            }
        }

        if (devices == 0) {
            Serial.println("No I2C devices found.");
        }
        bno_active = false;
        motion_sample.bno_active = false;
        return;
    }
    
    bno_active = true;
    motion_sample.bno_active = true;
    Serial.println("✔");
    Serial.print("    Activating Motion Reports ");
    Serial.println(setMotionReportRateHz(50) ? "✔" : "ERROR");

}

bool isButton1Pressed() {
    return digitalRead(PIN_BUTTON_1) == LOW;
}

bool isButton2Pressed() {
    return digitalRead(PIN_BUTTON_2) == LOW;
}

bool isSelectButtonPressed() {
#if UI_UPSIDE_DOWN
    return isButton2Pressed();
#else
    return isButton1Pressed();
#endif
}

bool isRotateButtonPressed() {
#if UI_UPSIDE_DOWN
    return isButton1Pressed();
#else
    return isButton2Pressed();
#endif
}

uint32_t getBatteryVoltageMv() {
    return analogReadMilliVolts(PIN_BAT_VOLT) * 2;
}

bool isBatteryConnected(uint32_t batteryMv) {
    return batteryMv <= 4300;
}

uint32_t getGraphSignalMv() {
    return analogReadMilliVolts(PIN_GRAPH_SIGNAL);
}

void updateMotionSample() {
    if (!bno_active) {
        motion_sample.bno_active = false;
        return;
    }

    if (bno.wasReset()) {
        configureMotionReports(active_motion_hz == 0 ? 50 : active_motion_hz, true);
    }

    sh2_SensorValue_t sensor_value;
    uint32_t now_ms = millis();
    int drained = 0;
    while (drained < 8 && bno.getSensorEvent(&sensor_value)) {
        drained++;
        motion_sample.bno_active = true;
        switch (sensor_value.sensorId) {
            case SH2_ACCELEROMETER:
                motion_sample.accel_x = sensor_value.un.accelerometer.x;
                motion_sample.accel_y = sensor_value.un.accelerometer.y;
                motion_sample.accel_z = sensor_value.un.accelerometer.z;
                motion_sample.accel_ms = now_ms;
                motion_sample.accel_valid = true;
                break;
            case SH2_GYROSCOPE_CALIBRATED:
                motion_sample.gyro_x = sensor_value.un.gyroscope.x;
                motion_sample.gyro_y = sensor_value.un.gyroscope.y;
                motion_sample.gyro_z = sensor_value.un.gyroscope.z;
                motion_sample.gyro_ms = now_ms;
                motion_sample.gyro_valid = true;
                break;
            case SH2_GAME_ROTATION_VECTOR:
                motion_sample.quat_i = sensor_value.un.gameRotationVector.i;
                motion_sample.quat_j = sensor_value.un.gameRotationVector.j;
                motion_sample.quat_k = sensor_value.un.gameRotationVector.k;
                motion_sample.quat_real = sensor_value.un.gameRotationVector.real;
                motion_sample.quat_ms = now_ms;
                motion_sample.quat_valid = true;
                break;
        }
    }
}

const MotionSample &getMotionSample() {
    return motion_sample;
}

bool isMotionSampleFresh(uint32_t maxAgeMs) {
    uint32_t now_ms = millis();
    return motion_sample.bno_active &&
           motion_sample.accel_valid &&
           motion_sample.gyro_valid &&
           motion_sample.quat_valid &&
           now_ms - motion_sample.accel_ms <= maxAgeMs &&
           now_ms - motion_sample.gyro_ms <= maxAgeMs &&
           now_ms - motion_sample.quat_ms <= maxAgeMs;
}
