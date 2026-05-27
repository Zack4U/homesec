#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================================
// IR Avoidance Sensor - Single sensor between door and window
// Output: LOW = obstacle detected, HIGH = clear
// ============================================================================

class AvoidanceSensor {
public:
    void begin() {
        pinMode(PIN_AVOIDANCE, INPUT);
        Serial.println(F("[AVOIDANCE] Initialized OK"));
    }

    void update() {
        if (!enabled_) return;
        
        unsigned long now = millis();
        if (now - lastCheck_ < 100) return; // 100ms check interval
        lastCheck_ = now;
        
        bool currentTriggered = digitalRead(PIN_AVOIDANCE) == LOW;
        
        if (currentTriggered && !triggered_) {
            // New trigger - start debounce
            if (!debouncing_) {
                debouncing_ = true;
                debounceStart_ = now;
            } else if (now - debounceStart_ > 150) {
                // Confirmed trigger after 150ms debounce
                triggered_ = true;
                newTrigger_ = true;
                triggerTime_ = now;
                debouncing_ = false;
                Serial.println(F("[AVOIDANCE] *** THRESHOLD CROSSED ***"));
            }
        } else if (!currentTriggered) {
            debouncing_ = false;
            if (triggered_ && (now - triggerTime_ > 2000)) {
                // Clear after 2 seconds of no detection
                triggered_ = false;
                Serial.println(F("[AVOIDANCE] Cleared"));
            }
        }
    }

    void enable() { enabled_ = true; }
    void disable() { 
        enabled_ = false; 
        triggered_ = false;
        newTrigger_ = false;
    }
    
    void temporaryDisable(unsigned long durationMs) {
        disable();
        reEnableTime_ = millis() + durationMs;
        tempDisabled_ = true;
        Serial.print(F("[AVOIDANCE] Disabled for "));
        Serial.print(durationMs / 1000);
        Serial.println(F("s"));
    }

    // Call in main loop to handle temp disable expiry
    void checkReEnable() {
        if (tempDisabled_ && millis() >= reEnableTime_) {
            enable();
            tempDisabled_ = false;
            Serial.println(F("[AVOIDANCE] Re-enabled after timeout"));
        }
    }

    bool isTriggered() const { return triggered_; }
    bool isEnabled() const { return enabled_; }
    
    bool hasNewTrigger() {
        bool t = newTrigger_;
        newTrigger_ = false;
        return t;
    }

private:
    bool enabled_ = true;
    bool triggered_ = false;
    bool newTrigger_ = false;
    bool debouncing_ = false;
    bool tempDisabled_ = false;
    unsigned long lastCheck_ = 0;
    unsigned long debounceStart_ = 0;
    unsigned long triggerTime_ = 0;
    unsigned long reEnableTime_ = 0;
};
