#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

class DoorController {
public:
    void begin() {
        servo_.attach(PIN_SERVO_DOOR);
        close();
        Serial.println(F("[DOOR] Initialized - CLOSED"));
    }

    void update() {
        unsigned long now = millis();
        // Auto-close logic
        if (isOpen_ && autoCloseEnabled_ && (now - openTime_ >= autoCloseMs_)) {
            close();
            autoCloseTriggered_ = true;
            Serial.println(F("[DOOR] Auto-closed"));
        }
        // Detach servo after movement completes to save power
        if (servoAttached_ && (now - lastMoveTime_ > SERVO_DETACH_DELAY_MS)) {
            servo_.detach();
            servoAttached_ = false;
        }
    }

    void open(const char* reason = "manual") {
        if (!servoAttached_) { servo_.attach(PIN_SERVO_DOOR); servoAttached_ = true; }
        servo_.write(DOOR_OPEN_ANGLE);
        isOpen_ = true;
        openTime_ = millis();
        lastMoveTime_ = openTime_;
        stateChanged_ = true;
        strncpy(lastReason_, reason, sizeof(lastReason_) - 1);
        Serial.print(F("[DOOR] OPENED - reason: ")); Serial.println(reason);
    }

    void close() {
        if (!servoAttached_) { servo_.attach(PIN_SERVO_DOOR); servoAttached_ = true; }
        servo_.write(DOOR_CLOSED_ANGLE);
        isOpen_ = false;
        lastMoveTime_ = millis();
        stateChanged_ = true;
        Serial.println(F("[DOOR] CLOSED"));
    }

    // NOTE: SecurityController manages door close timing centrally.
    // The built-in auto-close is disabled by default.
    void setAutoClose(bool enabled, unsigned long ms = DEFAULT_DOOR_AUTO_CLOSE_MS) {
        autoCloseEnabled_ = enabled;
        autoCloseMs_ = ms;
    }

    bool isOpen() const { return isOpen_; }
    unsigned long openTime() const { return openTime_; }
    const char* getLastReason() const { return lastReason_; }
    bool hasStateChanged() { bool c = stateChanged_; stateChanged_ = false; return c; }
    bool wasAutoCloseTriggered() { bool c = autoCloseTriggered_; autoCloseTriggered_ = false; return c; }

private:
    Servo servo_;
    bool isOpen_ = false, servoAttached_ = true;
    bool stateChanged_ = false, autoCloseTriggered_ = false;
    bool autoCloseEnabled_ = false; // Disabled — SecurityController owns close timing
    unsigned long openTime_ = 0, lastMoveTime_ = 0;
    unsigned long autoCloseMs_ = DEFAULT_DOOR_AUTO_CLOSE_MS;
    char lastReason_[16] = "init";
};
