#include <Arduino.h>
#include "config.h"
#include "display_manager.h"
#include "radar_system.h"
#include "camera_controller.h"
#include "light_sensor.h"
#include "avoidance_sensor.h"
#include "climate_sensor.h"
#include "ble_manager.h"
#include "rfid_manager.h"
#include "door_controller.h"
#include "touch_button.h"
#include "mqtt_manager.h"
#include "security_controller.h"
#include "power_manager.h"
#include "alarm_leds.h"

// ============================================================================
// Global instances
// ============================================================================
DisplayManager      displayMgr;
RadarSystem         radar;
CameraController    camera;
LightSensor         lightSensor;
AvoidanceSensor     avoidance;
ClimateSensor       climate;
BLEManager          bleMgr;
RFIDManager         rfidMgr;
DoorController      door;
TouchButton         touchBtn;
MQTTManager         mqttMgr;
SecurityController  security;
PowerManager        power;
AlarmLEDs           alarmLeds;

SystemConfig        sysConfig;
unsigned long       alarmScreenUntilMs = 0;

// Forward declarations
void updateDisplay();
void publishPeriodicData();

// ============================================================================
// MQTT Message Handler - routes incoming commands to subsystems
// Called from Core 1 when processing the incoming command queue
// ============================================================================
void onMqttMessage(const char* topic, JsonDocument& doc) {

    // --- Radar configuration ---
    if (strcmp(topic, TOPIC_CONFIG_RADAR) == 0) {
        if (doc.containsKey("min_angle")) {
            int minA = doc["min_angle"];
            int maxA = doc["max_angle"] | radar.getMaxAngle();
            radar.setAngleRange(minA, maxA);
        }
        if (doc.containsKey("threshold")) {
            radar.setDetectionThreshold(doc["threshold"]);
        }
    }
    // --- Light configuration ---
    else if (strcmp(topic, TOPIC_CONFIG_LIGHT) == 0) {
        if (doc.containsKey("auto")) {
            lightSensor.setAutoMode(doc["auto"]);
        }
    }
    // --- Detection thresholds ---
    else if (strcmp(topic, TOPIC_CONFIG_DETECTION) == 0) {
        if (doc.containsKey("threshold")) {
            radar.setDetectionThreshold(doc["threshold"]);
        }
        if (doc.containsKey("alert_distance")) {
            radar.setAlertDistance(doc["alert_distance"]);
        }
    }
    // --- WiFi configuration ---
    else if (strcmp(topic, TOPIC_CONFIG_WIFI) == 0) {
        const char* ssid = doc["ssid"] | "";
        const char* pass = doc["password"] | doc["pass"] | "";
        if (strlen(ssid) > 0) {
            mqttMgr.setWiFiCredentials(ssid, pass);
        }
    }
    // --- BLE configuration ---
    else if (strcmp(topic, TOPIC_CONFIG_BLE) == 0) {
        if (doc.containsKey("rssi_close")) {
            int close = doc["rssi_close"];
            int near = doc["rssi_near"] | bleMgr.getRSSINear();
            bleMgr.setRSSIThresholds(close, near);
        }
    }
    // --- Arm/Disarm ---
    else if (strcmp(topic, TOPIC_CONTROL_ARM) == 0) {
        const char* system = doc["system"] | "all";
        bool armed = doc["armed"] | true;
        if (armed) security.armSystem(system);
        else security.disarmSystem(system);
        
        mqttMgr.publishSystemStatus(
            security.getArmedStates().isFullyArmed(),
            security.getStateString(),
            power.getUptime()
        );
    }
    // --- Camera control ---
    else if (strcmp(topic, TOPIC_CONTROL_CAMERA) == 0) {
        const char* action = doc["action"] | "";
        if (strcmp(action, "start") == 0 || strcmp(action, "start_recording") == 0) {
            camera.startRecording();
            mqttMgr.publishCameraCommand("start_recording");
        } else if (strcmp(action, "stop") == 0 || strcmp(action, "stop_recording") == 0) {
            camera.stopRecording();
            mqttMgr.publishCameraCommand("stop_recording");
        } else if (strcmp(action, "photo") == 0 || strcmp(action, "take_photo") == 0) {
            camera.takePhoto();
            mqttMgr.publishCameraCommand("take_photo");
        }
        if (doc.containsKey("angle")) {
            camera.setAngle(doc["angle"]);
        }
    }
    // --- Flashlight control ---
    else if (strcmp(topic, TOPIC_CONTROL_FLASHLIGHT) == 0) {
        bool on = doc["on"] | false;
        lightSensor.setFlashlight(on);
        lightSensor.setAutoMode(false); // manual override disables auto
        mqttMgr.publishLight(lightSensor.isDark(), lightSensor.isFlashlightOn());
    }
    // --- Servo manual control ---
    else if (strcmp(topic, TOPIC_CONTROL_SERVO) == 0) {
        const char* target = doc["target"] | "";
        int angle = doc["angle"] | 0;
        if (strcmp(target, "camera") == 0) camera.setAngle(angle);
        else if (strcmp(target, "radar") == 0) radar.setManualAngle(angle);
        else if (strcmp(target, "door") == 0) {
            if (angle > 45) door.open("manual");
            else door.close();
        }
    }
    // --- Door control ---
    else if (strcmp(topic, TOPIC_CONTROL_DOOR) == 0) {
        bool open = doc["open"] | false;
        if (open) { door.open("remote"); mqttMgr.publishDoorStatus(true, "remote"); }
        else { door.close(); mqttMgr.publishDoorStatus(false, "remote"); }
    }
    // --- BLE device registration ---
    else if (strcmp(topic, TOPIC_BLE_REGISTER) == 0) {
        const char* action = doc["action"] | "";
        const char* mac = doc["mac"] | "";
        const char* name = doc["name"] | "Device";
        if (strcmp(action, "add") == 0) bleMgr.registerDevice(mac, name);
        else if (strcmp(action, "delete") == 0) bleMgr.removeDevice(mac);
        else if (strcmp(action, "update") == 0) bleMgr.updateDevice(mac, name);
    }
    // --- RFID card registration ---
    else if (strcmp(topic, TOPIC_RFID_REGISTER) == 0) {
        const char* action = doc["action"] | "";
        const char* uid = doc["uid"] | "";
        const char* name = doc["name"] | "Card";
        const char* level = doc["level"] | "guest";
        RFIDPermission perm = (strcmp(level, "owner") == 0) ? RFIDPermission::OWNER : RFIDPermission::GUEST;
        unsigned long expiry = doc["expires_in"] | 0UL;
        
        if (strcmp(action, "add") == 0) rfidMgr.registerCard(uid, name, perm, expiry);
        else if (strcmp(action, "delete") == 0) rfidMgr.removeCard(uid);
    }
    // --- RFID guest time ---
    else if (strcmp(topic, TOPIC_RFID_GUEST_TIME) == 0) {
        const char* uid = doc["uid"] | "";
        unsigned long expiry = doc["expires_in"] | (unsigned long)DEFAULT_GUEST_TIMEOUT_S;
        rfidMgr.setGuestExpiry(uid, expiry);
    }
}

// ============================================================================
// SETUP - Runs on Core 1 (Arduino default)
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("\n==================================="));
    Serial.println(F(" ESP32 IoT Home Security System"));
    Serial.println(F(" v2.0.0 - Dual-Core Architecture"));
    Serial.println(F("===================================\n"));

    // --- Phase 1: Display first (for boot animation) ---
    displayMgr.begin();
    displayMgr.playBootAnimation();

    // --- Phase 2: Sensors & Actuators (all on Core 1) ---
    radar.begin();
    camera.begin();
    lightSensor.begin();
    avoidance.begin();
    climate.begin();
    door.begin();
    touchBtn.begin();
    alarmLeds.begin();

    // --- Phase 3: Communication ---
    // RFID uses SPI - init before BLE (BLE init is heavy)
    rfidMgr.begin();
    
    // BLE init (launches its own FreeRTOS task on Core 0)
    bleMgr.begin();

    // WiFi + MQTT (launches network FreeRTOS task on Core 0)
    // This returns immediately - non-blocking!
    mqttMgr.begin(onMqttMessage);

    // --- Phase 4: Security Controller ---
    security.begin(&radar, &camera, &lightSensor, &avoidance,
                   &door, &bleMgr, &rfidMgr, &mqttMgr, &displayMgr, &alarmLeds);

    // --- Phase 5: Power Manager ---
    power.begin();

    // --- Load system config defaults ---
    sysConfig.radarMinAngle = DEFAULT_RADAR_MIN_ANGLE;
    sysConfig.radarMaxAngle = DEFAULT_RADAR_MAX_ANGLE;
    sysConfig.detectionThreshold = DEFAULT_DETECTION_THRESHOLD;
    sysConfig.alertDistance = DEFAULT_ALERT_DISTANCE;
    sysConfig.flashlightAutoMode = true;
    sysConfig.doorAutoCloseMs = DEFAULT_DOOR_AUTO_CLOSE_MS;
    sysConfig.bleRssiClose = DEFAULT_BLE_RSSI_CLOSE;
    sysConfig.bleRssiNear = DEFAULT_BLE_RSSI_NEAR;
    sysConfig.touchThreshold = DEFAULT_TOUCH_THRESHOLD;

    Serial.println(F("\n[SYSTEM] All modules initialized!"));
    Serial.print(F("[SYSTEM] Free heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
    Serial.println(F("[SYSTEM] Core 0: WiFi + MQTT + BLE (FreeRTOS tasks)"));
    Serial.println(F("[SYSTEM] Core 1: Sensors + Actuators + Display (Arduino loop)"));
    Serial.println(F("[SYSTEM] Entering main loop...\n"));
}

// ============================================================================
// MAIN LOOP - Runs on Core 1 (Application CPU)
// All sensor/actuator/display logic stays here.
// Network operations (WiFi, MQTT, BLE) run on Core 0 via FreeRTOS tasks.
// ============================================================================
void loop() {
    // ---- HIGH PRIORITY TASKS ----
    
    // Radar scan (50ms)
    if (power.shouldRun(PowerManager::TASK_RADAR)) {
        radar.update();
    }

    // Avoidance check (100ms)
    if (power.shouldRun(PowerManager::TASK_AVOIDANCE)) {
        avoidance.update();
        avoidance.checkReEnable();
    }

    // Touch button (50ms)
    if (power.shouldRun(PowerManager::TASK_TOUCH)) {
        touchBtn.update();
        if (touchBtn.hasNewPress()) {
            if (security.getState() == SecurityState::DISARMED || !security.getArmedStates().isFullyArmed()) {
                security.armSystem("all");
            } else {
                Serial.println(F("[TOUCH] Ignored - system already active"));
            }
        }
    }

    // Process incoming MQTT commands from Core 0 (50ms)
    if (power.shouldRun(PowerManager::TASK_INCOMING_MQTT)) {
        mqttMgr.processIncoming();
    }

    // Camera servo update (15ms for smooth motion)
    if (power.shouldRun(PowerManager::TASK_CAMERA)) {
        camera.update();
    }

    // ---- MEDIUM PRIORITY TASKS ----

    // RFID check (200ms)
    if (power.shouldRun(PowerManager::TASK_RFID_CHECK)) {
        rfidMgr.update();
    }

    // Door servo update (200ms)
    if (power.shouldRun(PowerManager::TASK_DOOR)) {
        door.update();
    }

    // ---- SECURITY CONTROLLER (processes events from all modules) ----
    security.update();
    if (security.hasAlarmTriggered()) {
        alarmScreenUntilMs = millis() + 5000;
    }

    // ---- LOW PRIORITY TASKS ----

    // Climate reading (2s)
    if (power.shouldRun(PowerManager::TASK_CLIMATE)) {
        climate.update();
    }

    // Light sensor (500ms)
    if (power.shouldRun(PowerManager::TASK_LIGHT)) {
        lightSensor.update();
    }

    // Alarm LEDs update (100ms)
    if (power.shouldRun(PowerManager::TASK_ALARM_LEDS)) {
        alarmLeds.update();
    }

    // ---- DISPLAY UPDATE (100ms) ----
    if (power.shouldRun(PowerManager::TASK_DISPLAY)) {
        updateDisplay();
    }

    // ---- MQTT PUBLISH (1s periodic data via queue to Core 0) ----
    if (power.shouldRun(PowerManager::TASK_MQTT_PUBLISH)) {
        publishPeriodicData();
    }

    // ---- SYSTEM STATUS (5s) ----
    if (power.shouldRun(PowerManager::TASK_SYSTEM_STATUS)) {
        mqttMgr.publishSystemStatus(
            security.getArmedStates().isFullyArmed(),
            security.getStateString(),
            power.getUptime()
        );
    }

    // ---- Camera command forwarding ----
    if (camera.hasRecordingChanged()) {
        mqttMgr.publishCameraCommand(camera.isRecording() ? "start_recording" : "stop_recording");
    }
    if (camera.hasPhotoRequest()) {
        mqttMgr.publishCameraCommand("take_photo");
    }

    // ---- Door state forwarding ----
    if (door.hasStateChanged()) {
        mqttMgr.publishDoorStatus(door.isOpen(), door.getLastReason());
    }
}

// ============================================================================
// Display update logic - shows appropriate screen based on system state
// ============================================================================
void updateDisplay() {
    unsigned long now = millis();

    if (security.isEmergency() || now < alarmScreenUntilMs) {
        displayMgr.showAlarmScreen("Intruso en entrada");
        return;
    }

    RadarMode rMode = radar.getMode();
    
    if (rMode == RadarMode::DETECTION || rMode == RadarMode::TRACKING) {
        const char* presenceMode = (rMode == RadarMode::TRACKING) ? "TRACK" : "DETECT";
        displayMgr.showPresenceDetected(
            radar.getLastDistance(),
            radar.getCurrentAngle(),
            presenceMode
        );
    } else {
        // General status screen
        const char* radarStr;
        switch (rMode) {
            case RadarMode::PATROL:    radarStr = "PATROL"; break;
            case RadarMode::DETECTION: radarStr = "DETECT"; break;
            case RadarMode::MANUAL:    radarStr = "MANUAL"; break;
            case RadarMode::OFF:      radarStr = "OFF";    break;
            default:                   radarStr = "---";    break;
        }

        displayMgr.showGeneralStatus(
            mqttMgr.isWiFiConnected(),
            mqttMgr.isMQTTConnected(),
            security.getStateString(),
            climate.getTemperature(),
            climate.getHumidity(),
            radarStr,
            door.isOpen()
        );
    }
}

// ============================================================================
// Periodic MQTT data publishing (sensor data, radar scans)
// All publish calls are non-blocking (enqueue to Core 0 via FreeRTOS queue)
// ============================================================================
void publishPeriodicData() {
    // Radar scan data
    if (radar.getMode() == RadarMode::PATROL || radar.getMode() == RadarMode::DETECTION) {
        const char* modeStr;
        switch (radar.getMode()) {
            case RadarMode::PATROL:    modeStr = "patrol"; break;
            case RadarMode::DETECTION: modeStr = "detection"; break;
            default:                   modeStr = "other"; break;
        }
        mqttMgr.publishRadarScan(radar.getCurrentAngle(), radar.getLastDistance(), modeStr);
    }

    // Climate data
    if (climate.hasValidReading()) {
        mqttMgr.publishClimate(climate.getTemperature(), climate.getHumidity());
    }

    // Light status
    mqttMgr.publishLight(lightSensor.isDark(), lightSensor.isFlashlightOn());
}