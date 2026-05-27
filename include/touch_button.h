#pragma once

#include <Arduino.h>
#include "config.h"

class TouchButton {
public:
    void begin() {
        threshold_ = DEFAULT_TOUCH_THRESHOLD;
        uint32_t sum = 0;
        const int samples = 16;
        for (int i = 0; i < samples; i++) {
            sum += touchRead(PIN_TOUCH_BUTTON);
            delay(10);
        }
        int baseline = (int)(sum / samples);
        threshold_ = max(15, baseline - 15);
        Serial.print(F("[TOUCH] Initialized on GPIO33, baseline="));
        Serial.print(baseline);
        Serial.print(F(", threshold="));
        Serial.println(threshold_);
    }

    void update() {
        unsigned long now = millis();
        if (now - lastCheck_ < 50) return;
        lastCheck_ = now;
        
        uint16_t val = touchRead(PIN_TOUCH_BUTTON);
        bool currentTouched = (val < threshold_);
        
        if (currentTouched && !touched_ && (now - lastTrigger_ > TOUCH_DEBOUNCE_MS)) {
            touched_ = true;
            newPress_ = true;
            lastTrigger_ = now;
            Serial.println(F("[TOUCH] Button pressed!"));
        } else if (!currentTouched) {
            touched_ = false;
        }
    }

    bool hasNewPress() { bool p = newPress_; newPress_ = false; return p; }
    bool isTouched() const { return touched_; }
    void setThreshold(int t) { threshold_ = t; }

private:
    bool touched_ = false, newPress_ = false;
    int threshold_ = DEFAULT_TOUCH_THRESHOLD;
    unsigned long lastCheck_ = 0, lastTrigger_ = 0;
};
