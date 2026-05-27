#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================================
// Power Manager - Non-blocking task scheduler with priority-based execution
// Staggers high-power operations to avoid simultaneous current spikes
//
// NOTE: MQTT and BLE tasks are now managed by FreeRTOS tasks on Core 0.
// This scheduler only manages Core 1 (application) tasks.
// ============================================================================

class PowerManager {
public:
    // Task IDs (Core 1 only - network tasks moved to Core 0)
    enum Task : uint8_t {
        TASK_RADAR = 0,
        TASK_AVOIDANCE,
        TASK_TOUCH,
        TASK_INCOMING_MQTT,    // Process incoming MQTT commands from Core 0 queue
        TASK_RFID_CHECK,
        TASK_DISPLAY,
        TASK_CLIMATE,
        TASK_LIGHT,
        TASK_MQTT_PUBLISH,     // Periodic sensor data publish (enqueues to Core 0)
        TASK_SYSTEM_STATUS,
        TASK_DOOR,
        TASK_CAMERA,
        TASK_ALARM_LEDS,
        TASK_COUNT
    };

    void begin() {
        memset(lastRun_, 0, sizeof(lastRun_));
        
        // Set intervals (ms) per task
        intervals_[TASK_RADAR]          = DEFAULT_RADAR_STEP_DELAY_MS;
        intervals_[TASK_AVOIDANCE]      = 100;
        intervals_[TASK_TOUCH]          = 50;
        intervals_[TASK_INCOMING_MQTT]  = 50;    // Check incoming command queue
        intervals_[TASK_RFID_CHECK]     = RFID_CHECK_INTERVAL_MS;
        intervals_[TASK_DISPLAY]        = DISPLAY_UPDATE_INTERVAL_MS;
        intervals_[TASK_CLIMATE]        = DEFAULT_CLIMATE_INTERVAL_MS;
        intervals_[TASK_LIGHT]          = DEFAULT_LIGHT_CHECK_MS;
        intervals_[TASK_MQTT_PUBLISH]   = MQTT_PUBLISH_INTERVAL_MS;
        intervals_[TASK_SYSTEM_STATUS]  = MQTT_STATUS_INTERVAL_MS;
        intervals_[TASK_DOOR]           = 200;
        intervals_[TASK_CAMERA]         = 15;
        intervals_[TASK_ALARM_LEDS]     = 100;   // LED blink control
        
        Serial.println(F("[POWER] Task scheduler initialized (Core 1 only)"));
    }

    bool shouldRun(Task task) {
        unsigned long now = millis();
        if (now - lastRun_[task] >= intervals_[task]) {
            lastRun_[task] = now;
            return true;
        }
        return false;
    }

    void setInterval(Task task, unsigned long ms) {
        if (task < TASK_COUNT) intervals_[task] = ms;
    }

    unsigned long getUptime() const { return millis(); }

private:
    unsigned long lastRun_[TASK_COUNT];
    unsigned long intervals_[TASK_COUNT];
};
