#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

class CameraController {
public:
    void begin() {
        servo_.attach(PIN_SERVO_CAMERA);
        servo_.write(0);
        currentAngle_ = 0;
        Serial.println(F("[CAMERA] Initialized at 0 deg"));
    }

    void update() {
        unsigned long now = millis();
        // Smooth movement towards target
        if (currentAngle_ != targetAngle_) {
            if (now - lastMoveTime_ >= 15) { // ~15ms per step for smooth motion
                lastMoveTime_ = now;
                if (!servoAttached_) { servo_.attach(PIN_SERVO_CAMERA); servoAttached_ = true; }
                if (currentAngle_ < targetAngle_) currentAngle_++;
                else currentAngle_--;
                servo_.write(currentAngle_);
            }
        } else if (servoAttached_ && (now - lastMoveTime_ > SERVO_DETACH_DELAY_MS)) {
            servo_.detach();
            servoAttached_ = false;
        }
    }

    void setAngle(int angle) {
        targetAngle_ = constrain(angle, 0, 180);
    }

    void setAngleImmediate(int angle) {
        targetAngle_ = constrain(angle, 0, 180);
        currentAngle_ = targetAngle_;
        if (!servoAttached_) { servo_.attach(PIN_SERVO_CAMERA); servoAttached_ = true; }
        servo_.write(currentAngle_);
        lastMoveTime_ = millis();
    }

    // Sync with radar servo angle for tracking mode
    void syncWithRadar(int radarAngle) {
        if (trackingMode_) setAngle(radarAngle);
    }

    void setTrackingMode(bool tracking) {
        trackingMode_ = tracking;
        if (tracking) {
            recording_ = true;
            Serial.println(F("[CAMERA] Tracking mode ON - recording"));
        } else {
            Serial.println(F("[CAMERA] Tracking mode OFF"));
        }
    }

    void startRecording() { recording_ = true; recordingChanged_ = true; }
    void stopRecording() { recording_ = false; recordingChanged_ = true; }
    void takePhoto() { photoRequested_ = true; }

    // Camera command that changed (for MQTT publishing)
    bool hasRecordingChanged() { bool c = recordingChanged_; recordingChanged_ = false; return c; }
    bool hasPhotoRequest() { bool p = photoRequested_; photoRequested_ = false; return p; }

    bool isRecording() const { return recording_; }
    bool isTrackingMode() const { return trackingMode_; }
    int getCurrentAngle() const { return currentAngle_; }
    int getTargetAngle() const { return targetAngle_; }

private:
    Servo servo_;
    int currentAngle_ = 0, targetAngle_ = 0;
    bool servoAttached_ = true, trackingMode_ = false;
    bool recording_ = false, recordingChanged_ = false, photoRequested_ = false;
    unsigned long lastMoveTime_ = 0;
};
