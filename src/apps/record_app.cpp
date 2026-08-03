#include <apps/record_app.h>

#include <Arduino.h>
#include <SPIFFS.h>
#include <apps/app_common.h>
#include <board/input.h>
#include <ui.h>

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

enum RecordSink {
    SINK_SERIAL,
    SINK_FLASH,
    SINK_BLE,
    SINK_COUNT,
};

enum RecordField {
    FIELD_SINK,
    FIELD_RATE,
    FIELD_LABEL,
    FIELD_ACTION,
    FIELD_COUNT,
};

struct LabelOption {
    const char *name;
    uint8_t value;
};

struct __attribute__((packed)) RecordHeader {
    char magic[4];
    uint8_t version;
    uint16_t hz;
    uint16_t row_size;
};

struct __attribute__((packed)) RecordRow {
    uint32_t time_ms;
    uint16_t emg_mv;
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
    uint8_t label;
};

static const char *SINK_NAMES[] = {"Serial", "Flash", "BLE"};
static const uint16_t SAMPLE_RATES[] = {25, 50, 100, 200};
static const int SAMPLE_RATE_COUNT = sizeof(SAMPLE_RATES) / sizeof(SAMPLE_RATES[0]);
static const LabelOption LABELS[] = {
    {"Rest", 0},
    {"Bicep curls", 1},
};
static const int LABEL_COUNT = sizeof(LABELS) / sizeof(LABELS[0]);
static const char *RECORD_PATH = "/record.bin";
static const char *BLE_DEVICE_NAME = "MuscleWearable";
static const char *BLE_UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *BLE_UART_TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static const uint32_t LONG_PRESS_MS = 800;

static bool last_select_pressed;
static bool last_rotate_pressed;
static bool select_press_active;
static bool recording;
static bool editing;
static uint32_t select_press_started_ms;
static uint32_t recording_started_ms;
static uint32_t last_sample_ms;
static uint32_t records_written;
static uint32_t last_ui_update_ms;
static RecordSink selected_sink;
static RecordField selected_field;
static int selected_rate_index;
static int selected_label_index;
static File record_file;
static bool fs_ready;
static bool fs_formatted_this_boot;
static bool fs_mount_attempted;
static bool fs_mount_failed;
static bool ble_ready;
static bool ble_connected;
static bool ble_advertising;
static BLEServer *ble_server;
static BLECharacteristic *ble_tx;
static lv_obj_t *status_label;
static lv_obj_t *sink_label;
static lv_obj_t *rate_label;
static lv_obj_t *label_label;
static lv_obj_t *estimate_label;
static lv_obj_t *warning_label;

class RecordBleCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override {
        ble_connected = true;
    }

    void onDisconnect(BLEServer *server) override {
        ble_connected = false;
        BLEDevice::startAdvertising();
        ble_advertising = true;
    }
};

static void initRecordBle() {
    if (ble_ready) {
        BLEDevice::startAdvertising();
        ble_advertising = true;
        return;
    }

    Serial.println("Record BLE: starting MuscleWearable advertising");
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setPower(ESP_PWR_LVL_P9);
    ble_server = BLEDevice::createServer();
    ble_server->setCallbacks(new RecordBleCallbacks());

    BLEService *service = ble_server->createService(BLE_UART_SERVICE_UUID);
    ble_tx = service->createCharacteristic(
        BLE_UART_TX_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    ble_tx->addDescriptor(new BLE2902());
    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    BLEAdvertisementData adv_data;
    adv_data.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    adv_data.setCompleteServices(BLEUUID(BLE_UART_SERVICE_UUID));
    advertising->setAdvertisementData(adv_data);

    BLEAdvertisementData scan_response;
    scan_response.setName(BLE_DEVICE_NAME);
    advertising->setScanResponseData(scan_response);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    advertising->setMinInterval(0x20);
    advertising->setMaxInterval(0x40);
    advertising->start();
    ble_ready = true;
    ble_advertising = true;
}

static bool ensureFileSystem() {
    if (fs_ready) {
        return true;
    }

    if (fs_mount_attempted && fs_mount_failed) {
        return false;
    }

    fs_mount_attempted = true;
    Serial.println("Record flash: mounting SPIFFS from internal flash");
    fs_ready = SPIFFS.begin(true);
    fs_mount_failed = !fs_ready;
    fs_formatted_this_boot = fs_ready;
    if (fs_ready) {
        Serial.printf(
            "Record flash: ready, total=%lu used=%lu free=%lu\n",
            static_cast<unsigned long>(SPIFFS.totalBytes()),
            static_cast<unsigned long>(SPIFFS.usedBytes()),
            static_cast<unsigned long>(SPIFFS.totalBytes() - SPIFFS.usedBytes())
        );
    } else {
        Serial.println("Record flash: SPIFFS unavailable. Re-upload after full flash erase if this persists.");
    }
    return fs_ready;
}

static uint32_t estimateFlashSeconds() {
    if (!ensureFileSystem()) {
        return 0;
    }

    size_t free_bytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    uint32_t bytes_per_second = sizeof(RecordRow) * SAMPLE_RATES[selected_rate_index];
    if (bytes_per_second == 0) {
        return 0;
    }
    return free_bytes / bytes_per_second;
}

static void writeBinaryBytes(const uint8_t *data, size_t len) {
    switch (selected_sink) {
        case SINK_SERIAL:
            Serial.write(data, len);
            break;
        case SINK_FLASH:
            if (record_file) {
                record_file.write(data, len);
            }
            break;
        case SINK_BLE:
            if (ble_tx != nullptr && ble_connected) {
                size_t offset = 0;
                while (offset < len) {
                    size_t chunk = len - offset;
                    if (chunk > 20) {
                        chunk = 20;
                    }
                    ble_tx->setValue((uint8_t *)(data + offset), chunk);
                    ble_tx->notify();
                    offset += chunk;
                    delay(2);
                }
            }
            break;
        default:
            break;
    }
}

static bool startRecordingSink() {
    RecordHeader header = {{'M', 'W', 'R', '1'}, 1, SAMPLE_RATES[selected_rate_index], sizeof(RecordRow)};

    if (selected_sink == SINK_FLASH) {
        if (!ensureFileSystem()) {
            return false;
        }
        record_file = SPIFFS.open(RECORD_PATH, FILE_APPEND);
        if (!record_file) {
            return false;
        }
    }

    if (selected_sink == SINK_BLE) {
        initRecordBle();
        if (!ble_connected) {
            return false;
        }
    }

    setMotionReportRateHz(SAMPLE_RATES[selected_rate_index]);
    writeBinaryBytes(reinterpret_cast<const uint8_t *>(&header), sizeof(header));
    return true;
}

static void stopRecordingSink() {
    if (record_file) {
        record_file.flush();
        record_file.close();
    }
    setMotionReportRateHz(50);
}

static void startRecording() {
    selected_label_index = 0;
    if (!startRecordingSink()) {
        lv_label_set_text(warning_label, "WARN output not ready");
        return;
    }

    recording = true;
    editing = false;
    recording_started_ms = millis();
    last_sample_ms = 0;
    records_written = 0;
}

static void stopRecording() {
    recording = false;
    stopRecordingSink();
}

static void writeRecordSample() {
    const MotionSample &motion = getMotionSample();
    RecordRow row = {
        millis() - recording_started_ms,
        static_cast<uint16_t>(min<uint32_t>(getGraphSignalMv(), 3300)),
        motion.accel_x,
        motion.accel_y,
        motion.accel_z,
        motion.gyro_x,
        motion.gyro_y,
        motion.gyro_z,
        motion.quat_i,
        motion.quat_j,
        motion.quat_k,
        motion.quat_real,
        LABELS[selected_label_index].value
    };

    writeBinaryBytes(reinterpret_cast<const uint8_t *>(&row), sizeof(row));
    records_written++;
}

static void updateRecordLabels() {
    const char *sink_marker = selected_field == FIELD_SINK ? (editing ? "*" : ">") : " ";
    const char *rate_marker = selected_field == FIELD_RATE ? (editing ? "*" : ">") : " ";
    const char *label_marker = selected_field == FIELD_LABEL ? (editing ? "*" : ">") : " ";
    const char *action_marker = selected_field == FIELD_ACTION ? ">" : " ";

    lv_label_set_text_fmt(sink_label, "%sOut: %s", sink_marker, SINK_NAMES[selected_sink]);
    lv_label_set_text_fmt(rate_label, "%sHz: %u", rate_marker, SAMPLE_RATES[selected_rate_index]);
    lv_label_set_text_fmt(label_label, "%sLabel: %s", label_marker, LABELS[selected_label_index].name);
    lv_label_set_text_fmt(status_label, "%s%s  %lu rec", action_marker, recording ? "Stop" : "Start", records_written);

    uint32_t seconds = estimateFlashSeconds();
    if (seconds == 0) {
        lv_label_set_text(estimate_label, fs_mount_failed ? "Flash: mount failed" : "Flash: unavailable");
    } else {
        lv_label_set_text_fmt(
            estimate_label,
            "Flash @%uHz: %luh %lum",
            SAMPLE_RATES[selected_rate_index],
            seconds / 3600,
            (seconds % 3600) / 60
        );
    }

    uint32_t emg_mv = getGraphSignalMv();
    if (!isMotionSampleFresh(500)) {
        lv_label_set_text(warning_label, "WARN IMU stale/missing");
    } else if (emg_mv >= 3290) {
        lv_label_set_text(warning_label, "WARN EMG saturated");
    } else if (fs_formatted_this_boot) {
        lv_label_set_text(warning_label, "Flash ready");
        fs_formatted_this_boot = false;
    } else if (selected_sink == SINK_BLE && ble_connected) {
        lv_label_set_text(warning_label, "BLE phone connected");
    } else if (selected_sink == SINK_BLE && ble_advertising) {
        lv_label_set_text(warning_label, "BLE advertising");
    } else {
        lv_label_set_text(warning_label, "");
    }
}

static void rotateSelection() {
    if (recording) {
        selected_label_index = (selected_label_index + 1) % LABEL_COUNT;
        return;
    }

    if (!editing) {
        selected_field = static_cast<RecordField>((selected_field + 1) % FIELD_COUNT);
        return;
    }

    switch (selected_field) {
        case FIELD_SINK:
            selected_sink = static_cast<RecordSink>((selected_sink + 1) % SINK_COUNT);
            if (selected_sink == SINK_BLE) {
                initRecordBle();
            }
            break;
        case FIELD_RATE:
            selected_rate_index = (selected_rate_index + 1) % SAMPLE_RATE_COUNT;
            break;
        case FIELD_LABEL:
            selected_label_index = (selected_label_index + 1) % LABEL_COUNT;
            break;
        default:
            break;
    }
}

static void selectCurrentField() {
    if (recording) {
        stopRecording();
        return;
    }

    if (selected_field == FIELD_ACTION) {
        startRecording();
        return;
    }

    editing = !editing;
    if (editing && selected_field == FIELD_LABEL) {
        selected_label_index = 0;
    }
}

void initRecordApp() {
    last_select_pressed = isSelectButtonPressed();
    last_rotate_pressed = isRotateButtonPressed();
    select_press_active = false;
    recording = false;
    editing = false;
    selected_sink = SINK_SERIAL;
    selected_field = FIELD_SINK;
    selected_rate_index = 1;
    selected_label_index = 0;
    records_written = 0;
    last_ui_update_ms = 0;

    my_screen = createAppScreen("Record", initUI);

    sink_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(sink_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(sink_label, &lv_font_montserrat_12, 0);
    lv_obj_align(sink_label, LV_ALIGN_TOP_LEFT, 52, 30);

    rate_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(rate_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(rate_label, &lv_font_montserrat_12, 0);
    lv_obj_align(rate_label, LV_ALIGN_TOP_LEFT, 52, 52);

    label_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(label_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_label, &lv_font_montserrat_12, 0);
    lv_obj_align(label_label, LV_ALIGN_TOP_LEFT, 52, 74);

    status_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 52, 98);

    estimate_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(estimate_label, lv_color_hex(0xC7D0D9), 0);
    lv_obj_set_style_text_font(estimate_label, &lv_font_montserrat_10, 0);
    lv_obj_align(estimate_label, LV_ALIGN_TOP_LEFT, 52, 124);

    warning_label = lv_label_create(my_screen);
    lv_obj_set_style_text_color(warning_label, lv_color_hex(0xFF8A65), 0);
    lv_obj_set_style_text_font(warning_label, &lv_font_montserrat_10, 0);
    lv_obj_align(warning_label, LV_ALIGN_TOP_LEFT, 52, 140);

    updateRecordLabels();
    lv_scr_load(my_screen);
}

void updateRecordApp() {
    bool rotate_pressed = isRotateButtonPressed();
    if (rotate_pressed && !last_rotate_pressed) {
        rotateSelection();
        updateRecordLabels();
    }
    last_rotate_pressed = rotate_pressed;

    bool select_pressed = isSelectButtonPressed();
    if (select_pressed && !last_select_pressed) {
        select_press_active = true;
        select_press_started_ms = millis();
    }
    if (!select_pressed && last_select_pressed && select_press_active) {
        uint32_t held_ms = millis() - select_press_started_ms;
        select_press_active = false;
        if (held_ms >= LONG_PRESS_MS && !recording) {
            initUI();
            return;
        }
        selectCurrentField();
        updateRecordLabels();
    }
    last_select_pressed = select_pressed;

    if (recording) {
        uint32_t now_ms = millis();
        uint32_t interval_ms = 1000UL / SAMPLE_RATES[selected_rate_index];
        if (now_ms - last_sample_ms >= interval_ms) {
            last_sample_ms = now_ms;
            writeRecordSample();
        }
    }

    if (millis() - last_ui_update_ms >= 500) {
        last_ui_update_ms = millis();
        updateRecordLabels();
    }
}
