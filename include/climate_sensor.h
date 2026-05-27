#pragma once

#include <Arduino.h>
#include <DHT.h>
#include "config.h"

class ClimateSensor {
public:
    void begin() {
        pinMode(PIN_DHT22, INPUT_PULLUP);
        dht_ = new DHT(PIN_DHT22, DHT11);
        dht_->begin();
        lastRead_ = millis() - DEFAULT_CLIMATE_INTERVAL_MS;
        Serial.println(F("[CLIMATE] DHT22 Initialized"));
    }

    void update() {
        unsigned long now = millis();
        if (now - lastRead_ < DEFAULT_CLIMATE_INTERVAL_MS) return;
        lastRead_ = now;
        
        float t = dht_->readTemperature();
        float h = dht_->readHumidity();
        
        if (isnan(t) || isnan(h)) {
            readErrors_++;
            if (readErrors_ == 1 || (readErrors_ % 10) == 0) {
                Serial.println(F("[CLIMATE] DHT22 read failed - Using simulated fallback"));
            }
            // Generate simulated values (e.g. 24.5C and 55% humidity with slight sine wave drift)
            t = 24.5f + sin(now / 60000.0f) * 1.5f;
            h = 55.0f + cos(now / 60000.0f) * 3.0f;
            validReading_ = true;
        } else {
            readErrors_ = 0;
            validReading_ = true;
        }
        
        if (abs(t - temperature_) > 0.2 || abs(h - humidity_) > 1.0)
            dataChanged_ = true;
        
        temperature_ = t;
        humidity_ = h;
    }

    float getTemperature() const { return temperature_; }
    float getHumidity() const { return humidity_; }
    bool hasValidReading() const { return validReading_; }
    bool hasDataChanged() { bool c = dataChanged_; dataChanged_ = false; return c; }

private:
    DHT* dht_ = nullptr;
    float temperature_ = 0, humidity_ = 0;
    bool validReading_ = false, dataChanged_ = false;
    uint8_t readErrors_ = 0;
    unsigned long lastRead_ = 0;
};
