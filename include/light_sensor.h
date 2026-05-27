#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================================
// Light Sensor Module (3-pin digital) + LED Flashlight Controller
// Module outputs: HIGH = dark, LOW = light (built-in comparator)
//
// LEDC API compatibility: supports both ESP32 Arduino Core v2.x and v3.x
// ============================================================================

class LightSensor {
public:
    void begin() {
        pinMode(PIN_LIGHT_SENSOR, INPUT);
        pinMode(PIN_LED_FLASHLIGHT, OUTPUT);
        
        // Setup PWM for LED brightness control
        // Compatible with both ESP32 Arduino Core v2.x and v3.x
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        // New API (v3.0+): ledcAttach(pin, freq, resolution)
        ledcAttach(PIN_LED_FLASHLIGHT, LED_PWM_FREQ, LED_PWM_RESOLUTION);
#else
        // Legacy API (v2.x): ledcSetup + ledcAttachPin
        ledcSetup(LED_PWM_CHANNEL, LED_PWM_FREQ, LED_PWM_RESOLUTION);
        ledcAttachPin(PIN_LED_FLASHLIGHT, LED_PWM_CHANNEL);
#endif
        ledcWrite(LED_PWM_WRITE_TARGET, 0);
        
        Serial.println(F("[LIGHT] Initialized OK"));
    }

    void update() {
        unsigned long now = millis();
        if (now - lastCheck_ < DEFAULT_LIGHT_CHECK_MS) return;
        lastCheck_ = now;
        
        // Read digital output from module
        bool currentDark = digitalRead(PIN_LIGHT_SENSOR) == HIGH;
        
        // Debounce: require consistent readings
        if (currentDark != isDark_) {
            if (currentDark == pendingState_) {
                debounceCount_++;
                if (debounceCount_ >= 3) {
                    isDark_ = currentDark;
                    stateChanged_ = true;
                    
                    // Auto mode: turn on flashlight when dark
                    if (autoMode_ && enabled_) {
                        setFlashlight(isDark_);
                    }
                    
                    Serial.print(F("[LIGHT] State: "));
                    Serial.println(isDark_ ? F("DARK") : F("LIGHT"));
                }
            } else {
                pendingState_ = currentDark;
                debounceCount_ = 0;
            }
        }
    }

    // Manual flashlight control
    void setFlashlight(bool on) {
        flashlightOn_ = on;
        ledcWrite(LED_PWM_WRITE_TARGET, on ? 255 : 0);
    }

    void setFlashlightBrightness(uint8_t brightness) {
        flashlightOn_ = (brightness > 0);
        ledcWrite(LED_PWM_WRITE_TARGET, brightness);
    }

    void setAutoMode(bool autoMode) { autoMode_ = autoMode; }
    void setEnabled(bool enabled) { 
        enabled_ = enabled;
        if (!enabled_) setFlashlight(false);
    }

    bool isDark() const { return isDark_; }
    bool isFlashlightOn() const { return flashlightOn_; }
    bool isAutoMode() const { return autoMode_; }
    bool hasStateChanged() { 
        bool changed = stateChanged_;
        stateChanged_ = false;
        return changed;
    }

private:
    // LEDC write target differs between API versions
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    static constexpr uint8_t LED_PWM_WRITE_TARGET = PIN_LED_FLASHLIGHT;  // v3.x uses pin
#else
    static constexpr uint8_t LED_PWM_WRITE_TARGET = LED_PWM_CHANNEL;     // v2.x uses channel
#endif

    bool isDark_ = false;
    bool flashlightOn_ = false;
    bool autoMode_ = true;
    bool enabled_ = true;
    bool stateChanged_ = false;
    bool pendingState_ = false;
    uint8_t debounceCount_ = 0;
    unsigned long lastCheck_ = 0;
};
