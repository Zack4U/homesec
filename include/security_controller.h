#pragma once

#include <Arduino.h>
#include "config.h"
#include "radar_system.h"
#include "camera_controller.h"
#include "light_sensor.h"
#include "avoidance_sensor.h"
#include "door_controller.h"
#include "ble_manager.h"
#include "rfid_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"
#include "alarm_leds.h"

// ============================================================================
// Security Controller — Central orchestrator and state machine
// Coordinates all subsystems based on security state and events.
//
// Door logic (RFID / BLE):
//   RFID:
//     - Registered card → open door (if closed) → 10 s timer → close
//     - If avoidance triggered while open → close after 1 s delay
//     - If 10 s elapsed with no avoidance cross → close automatically
//
//   BLE:
//     - Registered device RSSI > BLE_CLOSE_THRESHOLD → open door (if closed)
//     - 10 s timer starts
//     - On timer expiry: re-check RSSI
//         · Still close enough → restart timer (person hasn't crossed yet)
//         · RSSI dropped OR device not detected → close door
//     - If avoidance triggered while open → close after 1 s delay
// ============================================================================

class SecurityController {
public:
    void begin(RadarSystem* radar, CameraController* camera, LightSensor* light,
               AvoidanceSensor* avoidance, DoorController* door, BLEManager* ble,
               RFIDManager* rfid, MQTTManager* mqtt, DisplayManager* display,
               AlarmLEDs* leds) {
        radar_     = radar;
        camera_    = camera;
        light_     = light;
        avoidance_ = avoidance;
        door_      = door;
        ble_       = ble;
        rfid_      = rfid;
        mqtt_      = mqtt;
        display_   = display;
        leds_      = leds;

        armed_.armAll();
        state_ = SecurityState::ARMED_FULL;
        if (leds_) leds_->setMode(AlarmLEDs::MODE_ARMED);

        // Security controller owns the door auto-close logic; disable DoorController's built-in.
        door_->setAutoClose(false);

        Serial.println(F("[SECURITY] Controller initialized - ARMED FULL"));
    }

    void update() {
        processRadarEvents();
        processBLEEvents();
        processRFIDEvents();
        processAvoidanceEvents();
        processDoorAutoClose();   // ← manages door timing centrally
        processEmergencyState();
        updateLEDs();
    }

    // ---- Arm / Disarm ----
    void armSystem(const char* system) {
        if (strcmp(system, "all") == 0) {
            armed_.armAll();
            state_ = SecurityState::ARMED_FULL;
            radar_->setMode(RadarMode::PATROL);
            avoidance_->enable();
        } else if (strcmp(system, "radar") == 0) {
            armed_.radarArmed = true;
            radar_->setMode(RadarMode::PATROL);
        } else if (strcmp(system, "camera") == 0) {
            armed_.cameraArmed = true;
        } else if (strcmp(system, "avoidance") == 0) {
            armed_.avoidanceArmed = true;
            avoidance_->enable();
        } else if (strcmp(system, "rfid") == 0) {
            armed_.rfidArmed = true;
        } else if (strcmp(system, "ble") == 0) {
            armed_.bleArmed = true;
        }
        Serial.print(F("[SECURITY] Armed: ")); Serial.println(system);
    }

    void disarmSystem(const char* system) {
        if (strcmp(system, "all") == 0) {
            armed_.disarmAll();
            state_ = SecurityState::DISARMED;
            radar_->setMode(RadarMode::OFF);
            avoidance_->disable();
            camera_->stopRecording();
            closeDoorInternal("disarm");
        } else if (strcmp(system, "radar") == 0) {
            armed_.radarArmed = false;
            radar_->setMode(RadarMode::OFF);
        } else if (strcmp(system, "camera") == 0) {
            armed_.cameraArmed = false;
            camera_->stopRecording();
        } else if (strcmp(system, "avoidance") == 0) {
            armed_.avoidanceArmed = false;
            avoidance_->disable();
        } else if (strcmp(system, "rfid") == 0) {
            armed_.rfidArmed = false;
        } else if (strcmp(system, "ble") == 0) {
            armed_.bleArmed = false;
        }

        if (!armed_.radarArmed && !armed_.avoidanceArmed)
            state_ = SecurityState::DISARMED;

        Serial.print(F("[SECURITY] Disarmed: ")); Serial.println(system);
    }

    // ---- Emergency Toggle (touch button) ----
    void toggleEmergency() {
        if (state_ == SecurityState::EMERGENCY) {
            state_ = SecurityState::ARMED_FULL;
            armed_.armAll();
            radar_->setMode(RadarMode::PATROL);
            avoidance_->enable();
            closeDoorInternal("emergency_off");
            camera_->stopRecording();
            Serial.println(F("[SECURITY] EMERGENCY OFF"));
            mqtt_->publishTouchEvent(true, true);
        } else {
            state_ = SecurityState::EMERGENCY;
            armed_.armAll();
            radar_->setMode(RadarMode::PATROL);
            avoidance_->enable();
            closeDoorInternal("emergency_on");
            camera_->startRecording();
            mqtt_->publishCameraCommand("start_recording");
            Serial.println(F("[SECURITY] *** EMERGENCY ON ***"));
            mqtt_->publishTouchEvent(true, true);
        }
        emergencyChanged_ = true;
    }

    // ---- State getters ----
    SecurityState getState() const { return state_; }
    const ArmedStates& getArmedStates() const { return armed_; }
    bool isEmergency() const { return state_ == SecurityState::EMERGENCY; }

    const char* getStateString() const {
        switch (state_) {
            case SecurityState::ARMED_FULL:     return "armed_full";
            case SecurityState::ARMED_PERIMETER: return "armed_perimeter";
            case SecurityState::DISARMED:        return "disarmed";
            case SecurityState::EMERGENCY:       return "emergency";
            default: return "unknown";
        }
    }

    bool hasEmergencyChanged() { bool c = emergencyChanged_; emergencyChanged_ = false; return c; }
    bool hasAlarmTriggered()   { bool a = alarmTriggered_;   alarmTriggered_   = false; return a; }

private:
    // ===========================================================================
    // Door State (managed here, not in DoorController)
    // ===========================================================================
    enum class DoorOpener : uint8_t { NONE, RFID, BLE };

    DoorOpener    doorOpener_       = DoorOpener::NONE;
    unsigned long doorOpenTime_     = 0;      // millis() when door was opened
    unsigned long doorCloseDelay_   = 0;      // if > 0: close at this timestamp (avoidance 1 s delay)
    bool          doorTimerActive_  = false;  // 10 s auto-close timer running
    bool          bleRecheckDone_   = false;  // BLE re-verification already fired this cycle

    // Open the door and start auto-close timer
    void openDoorInternal(const char* reason, DoorOpener opener) {
        if (door_->isOpen()) return; // already open
        door_->open(reason);
        mqtt_->publishDoorStatus(true, reason);
        doorOpener_      = opener;
        doorOpenTime_     = millis();
        doorTimerActive_  = true;
        doorCloseDelay_   = 0;
        bleRecheckDone_   = false;
        Serial.print(F("[DOOR] Opened by ")); Serial.println(reason);
    }

    void closeDoorInternal(const char* reason) {
        if (!door_->isOpen() && doorOpener_ == DoorOpener::NONE) return;
        door_->close();
        mqtt_->publishDoorStatus(false, reason);
        doorOpener_      = DoorOpener::NONE;
        doorTimerActive_ = false;
        doorCloseDelay_  = 0;
        Serial.print(F("[DOOR] Closed by ")); Serial.println(reason);
    }

    // ===========================================================================
    // processDoorAutoClose — runs every loop() tick
    // ===========================================================================
    void processDoorAutoClose() {
        if (!door_->isOpen()) return;

        unsigned long now = millis();

        // ── Avoidance 1 s delayed close ──────────────────────────────────────
        if (doorCloseDelay_ > 0 && now >= doorCloseDelay_) {
            Serial.println(F("[DOOR] Avoidance 1 s elapsed → closing"));
            closeDoorInternal("avoidance");
            return;
        }

        // ── Auto-close timer (10 s) ───────────────────────────────────────────
        if (!doorTimerActive_) return;
        if (now - doorOpenTime_ < DEFAULT_DOOR_AUTO_CLOSE_MS) return; // timer still running

        // Timer expired
        if (doorOpener_ == DoorOpener::RFID) {
            // RFID: simply close after 10 s (avoidance already handled above)
            Serial.println(F("[DOOR] RFID 10 s timer → closing"));
            closeDoorInternal("rfid_timeout");
        }
        else if (doorOpener_ == DoorOpener::BLE) {
            // BLE: re-verify proximity before closing
            if (ble_->isKnownDeviceClose()) {
                // Device still nearby → person hasn't crossed yet → restart timer
                Serial.println(F("[DOOR] BLE re-check: device still close → restarting timer"));
                doorOpenTime_    = now;
                bleRecheckDone_  = true;
            } else {
                // Device out of range → close
                Serial.println(F("[DOOR] BLE re-check: device gone → closing"));
                closeDoorInternal("ble_timeout");
            }
        }
    }

    // ===========================================================================
    // processRadarEvents
    // ===========================================================================
    void processRadarEvents() {
        if (!armed_.radarArmed) return;

        // Radar entered tracking mode
        if (radar_->hasModeChanged() && radar_->getMode() == RadarMode::TRACKING) {
            if (armed_.bleArmed && ble_->isKnownDeviceApproaching()) {
                radar_->markTargetAsKnown();
                BLEDeviceEntry devCopy;
                if (ble_->getLastKnownDeviceCopy(devCopy)) {
                    Serial.print(F("[SECURITY] Known BLE target: ")); Serial.println(devCopy.name);
                    mqtt_->publishCameraCommand("take_photo");
                    mqtt_->publishBLEDevice(devCopy.mac, devCopy.rssi, devCopy.name, true);
                    display_->showBLESafe(devCopy.name, devCopy.rssi);
                }
                return;
            }

            // Unknown target — start recording
            if (armed_.cameraArmed) {
                camera_->setTrackingMode(true);
                mqtt_->publishCameraCommand("start_recording");
            }
        }

        // Sync camera servo with radar during tracking
        if (radar_->isTrackingActive() && armed_.cameraArmed) {
            camera_->syncWithRadar(radar_->getCurrentAngle());
            mqtt_->publishRadarTracking(
                radar_->getCurrentAngle(),
                radar_->getLastDistance(),
                radar_->getPredictedAngle()
            );
        }

        // Radar alert (object crossed alert distance threshold)
        if (radar_->hasAlertTriggered()) {
            alarmActive_      = true;
            alarmActiveUntil_ = millis() + 5000;
            mqtt_->publishRadarAlert(
                radar_->getCurrentAngle(),
                radar_->getLastDistance(),
                radar_->isApproaching()
            );
        }

        // Radar returned to patrol from tracking
        if (radar_->hasModeChanged() && radar_->getMode() == RadarMode::PATROL) {
            if (camera_->isTrackingMode()) {
                camera_->setTrackingMode(false);
                camera_->stopRecording();
                mqtt_->publishCameraCommand("stop_recording");
            }
        }

        // Clear alarm after timeout
        if (alarmActive_ && millis() > alarmActiveUntil_) {
            alarmActive_ = false;
        }
    }

    // ===========================================================================
    // processBLEEvents
    // ===========================================================================
    void processBLEEvents() {
        if (!armed_.bleArmed) return;

        if (ble_->hasScanCompleted()) {
            // Publish registered devices (known = true)
            int count = ble_->getDeviceCount();
            for (int i = 0; i < count; i++) {
                BLEDeviceEntry devCopy;
                if (ble_->getDeviceCopy(i, devCopy) && devCopy.active) {
                    mqtt_->publishBLEDevice(devCopy.mac, devCopy.rssi, devCopy.name, true);
                }
            }

            // Publish unregistered discovered devices (known = false)
            int discCount = ble_->getDiscoveredCount();
            for (int i = 0; i < discCount; i++) {
                BLEDeviceEntry devCopy;
                if (ble_->getDiscoveredCopy(i, devCopy)) {
                    mqtt_->publishBLEDevice(devCopy.mac, devCopy.rssi, devCopy.name, false);
                }
            }

            // ── BLE door open logic ──────────────────────────────────────────
            // Only open if door is currently closed AND armed
            if (armed_.bleArmed && !door_->isOpen()) {
                if (ble_->isKnownDeviceClose()) {
                    BLEDeviceEntry devCopy;
                    if (ble_->getLastKnownDeviceCopy(devCopy)) {
                        Serial.print(F("[BLE] Known device close: ")); Serial.println(devCopy.name);
                        openDoorInternal("ble", DoorOpener::BLE);
                        display_->showBLESafe(devCopy.name, devCopy.rssi);
                    }
                }
            }
        }
    }

    // ===========================================================================
    // processRFIDEvents
    // ===========================================================================
    void processRFIDEvents() {
        if (!armed_.rfidArmed) return;

        if (rfid_->hasNewAccess()) {
            const char* levelStr = (rfid_->getLastAccessLevel() == RFIDPermission::OWNER) ? "owner" : "guest";

            mqtt_->publishRFIDAccess(rfid_->getLastUID(), rfid_->wasAccessGranted(),
                                     levelStr, rfid_->getLastName());
            display_->showRFIDAccess(rfid_->getLastName(), rfid_->wasAccessGranted(), levelStr);

            if (rfid_->wasAccessGranted() && !door_->isOpen()) {
                openDoorInternal("rfid", DoorOpener::RFID);
            }
        }
    }

    // ===========================================================================
    // processAvoidanceEvents
    // ===========================================================================
    void processAvoidanceEvents() {
        if (!armed_.avoidanceArmed) return;

        if (avoidance_->hasNewTrigger()) {
            mqtt_->publishAvoidance(true);
            alarmTriggered_   = true;
            alarmActive_      = true;
            alarmActiveUntil_ = millis() + 5000;

            // If door is open due to RFID or BLE, schedule close after 1 s
            if (door_->isOpen() && doorOpener_ != DoorOpener::NONE) {
                Serial.println(F("[AVOIDANCE] Person crossed threshold → closing door in 1 s"));
                doorTimerActive_ = false;              // cancel 10 s timer
                doorCloseDelay_  = millis() + 1000;   // close in 1 s
            } else {
                // Intrusion — record + alarm
                if (radar_->isTrackingActive()) {
                    radar_->returnToPatrol();
                }
                if (armed_.cameraArmed) {
                    camera_->setTrackingMode(false);
                    camera_->setAngleImmediate(0); // point at entry
                    camera_->startRecording();
                    mqtt_->publishCameraCommand("start_recording");
                }
                Serial.println(F("[SECURITY] *** AVOIDANCE TRIGGERED - INTRUSION ***"));
            }
        }
    }

    // ===========================================================================
    // processEmergencyState
    // ===========================================================================
    void processEmergencyState() {
        if (state_ != SecurityState::EMERGENCY) return;
        if (door_->isOpen()) {
            closeDoorInternal("emergency");
        }
    }

    // ===========================================================================
    // updateLEDs
    // ===========================================================================
    void updateLEDs() {
        if (!leds_) return;
        if (state_ == SecurityState::EMERGENCY) {
            leds_->setMode(AlarmLEDs::MODE_EMERGENCY);
        } else if (alarmActive_) {
            leds_->setMode(AlarmLEDs::MODE_ALARM);
        } else if (radar_->isTrackingActive()) {
            leds_->setMode(AlarmLEDs::MODE_TRACKING);
        } else if (radar_->getMode() == RadarMode::DETECTION) {
            leds_->setMode(AlarmLEDs::MODE_DETECTING);
        } else if (state_ == SecurityState::ARMED_FULL || state_ == SecurityState::ARMED_PERIMETER) {
            leds_->setMode(AlarmLEDs::MODE_ARMED);
        } else {
            leds_->setMode(AlarmLEDs::MODE_OFF);
        }
    }

    // ===========================================================================
    // Member variables
    // ===========================================================================
    RadarSystem*    radar_     = nullptr;
    CameraController* camera_  = nullptr;
    LightSensor*    light_     = nullptr;
    AvoidanceSensor* avoidance_ = nullptr;
    DoorController* door_      = nullptr;
    BLEManager*     ble_       = nullptr;
    RFIDManager*    rfid_      = nullptr;
    MQTTManager*    mqtt_      = nullptr;
    DisplayManager* display_   = nullptr;
    AlarmLEDs*      leds_      = nullptr;

    SecurityState state_ = SecurityState::ARMED_FULL;
    ArmedStates   armed_;
    bool emergencyChanged_ = false;
    bool alarmTriggered_   = false;
    bool alarmActive_      = false;
    unsigned long alarmActiveUntil_ = 0;
};
