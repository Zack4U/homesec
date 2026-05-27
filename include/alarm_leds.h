#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================================
// Alarm LEDs Controller - 3 LEDs for status indication
// LED 1 (GPIO2):  System status - ON when armed, blink when detecting
// LED 2 (GPIO15): Alert level - blink when tracking, solid in emergency
// LED 3 (GPIO4):  Emergency/alarm - flash rapidly during alarm/emergency
// ============================================================================

class AlarmLEDs {
public:
    enum Mode : uint8_t {
        MODE_OFF = 0,
        MODE_ARMED,        // LED1 solid
        MODE_DETECTING,    // LED1 blink slow
        MODE_TRACKING,     // LED1 + LED2 blink medium
        MODE_ALARM,        // All LEDs flash fast
        MODE_EMERGENCY     // All LEDs alternating flash
    };

    void begin() {
        pinMode(PIN_ALARM_LED_1, OUTPUT);
        pinMode(PIN_ALARM_LED_2, OUTPUT);
        pinMode(PIN_ALARM_LED_3, OUTPUT);
        allOff();
        Serial.println(F("[LEDS] Alarm LEDs initialized (GPIO2, GPIO15, GPIO4)"));
    }

    void setMode(Mode mode) {
        if (mode == currentMode_) return;
        currentMode_ = mode;
        if (mode == MODE_OFF) allOff();
    }

    void update() {
        unsigned long now = millis();
        unsigned long elapsed = now - lastUpdate_;
        
        switch (currentMode_) {
            case MODE_OFF:
                allOff();
                break;
                
            case MODE_ARMED:
                // LED1 solid on, others off
                digitalWrite(PIN_ALARM_LED_1, HIGH);
                digitalWrite(PIN_ALARM_LED_2, LOW);
                digitalWrite(PIN_ALARM_LED_3, LOW);
                break;
                
            case MODE_DETECTING:
                // LED1 blinks slow (500ms on/500ms off)
                if (elapsed >= 500) {
                    lastUpdate_ = now;
                    blinkState_ = !blinkState_;
                }
                digitalWrite(PIN_ALARM_LED_1, blinkState_ ? HIGH : LOW);
                digitalWrite(PIN_ALARM_LED_2, LOW);
                digitalWrite(PIN_ALARM_LED_3, LOW);
                break;
                
            case MODE_TRACKING:
                // LED1 + LED2 blink medium (300ms)
                if (elapsed >= 300) {
                    lastUpdate_ = now;
                    blinkState_ = !blinkState_;
                }
                digitalWrite(PIN_ALARM_LED_1, blinkState_ ? HIGH : LOW);
                digitalWrite(PIN_ALARM_LED_2, blinkState_ ? HIGH : LOW);
                digitalWrite(PIN_ALARM_LED_3, LOW);
                break;
                
            case MODE_ALARM:
                // All LEDs flash fast (100ms)
                if (elapsed >= 100) {
                    lastUpdate_ = now;
                    blinkState_ = !blinkState_;
                }
                digitalWrite(PIN_ALARM_LED_1, blinkState_ ? HIGH : LOW);
                digitalWrite(PIN_ALARM_LED_2, blinkState_ ? HIGH : LOW);
                digitalWrite(PIN_ALARM_LED_3, blinkState_ ? HIGH : LOW);
                break;
                
            case MODE_EMERGENCY:
                // Alternating flash pattern (150ms)
                if (elapsed >= 150) {
                    lastUpdate_ = now;
                    blinkState_ = !blinkState_;
                }
                digitalWrite(PIN_ALARM_LED_1, blinkState_ ? HIGH : LOW);
                digitalWrite(PIN_ALARM_LED_2, !blinkState_ ? HIGH : LOW);
                digitalWrite(PIN_ALARM_LED_3, blinkState_ ? HIGH : LOW);
                break;
        }
    }

    Mode getMode() const { return currentMode_; }

private:
    void allOff() {
        digitalWrite(PIN_ALARM_LED_1, LOW);
        digitalWrite(PIN_ALARM_LED_2, LOW);
        digitalWrite(PIN_ALARM_LED_3, LOW);
        blinkState_ = false;
    }

    Mode currentMode_ = MODE_OFF;
    bool blinkState_ = false;
    unsigned long lastUpdate_ = 0;
};
