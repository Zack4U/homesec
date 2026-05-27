#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ============================================================================
// Bitmap icons (8x8 pixels)
// ============================================================================
static const uint8_t PROGMEM icon_wifi[] = {
    0x00, 0x3C, 0x42, 0x18, 0x24, 0x00, 0x08, 0x00
};
static const uint8_t PROGMEM icon_mqtt[] = {
    0x00, 0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x00
};
static const uint8_t PROGMEM icon_shield[] = {
    0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18
};
static const uint8_t PROGMEM icon_lock[] = {
    0x18, 0x24, 0x24, 0x7E, 0xFF, 0xFF, 0xFF, 0x7E
};
static const uint8_t PROGMEM icon_unlock[] = {
    0x18, 0x20, 0x20, 0x7E, 0xFF, 0xFF, 0xFF, 0x7E
};
static const uint8_t PROGMEM icon_alert[] = {
    0x18, 0x18, 0x3C, 0x3C, 0x7E, 0x7E, 0x18, 0x7E
};
static const uint8_t PROGMEM icon_target[] = {
    0x3C, 0x42, 0x99, 0xBD, 0xBD, 0x99, 0x42, 0x3C
};

class DisplayManager {
public:
    void begin() {
        display_ = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
        if (!display_->begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
            Serial.println(F("[DISPLAY] SSD1306 init FAILED"));
            initialized_ = false;
            return;
        }
        initialized_ = true;
        display_->clearDisplay();
        display_->setTextColor(SSD1306_WHITE);
        display_->display();
        Serial.println(F("[DISPLAY] Initialized OK"));
    }

    // ========================================================================
    // BOOT ANIMATION - "ENCENDIDO"
    // ========================================================================
    void playBootAnimation() {
        if (!initialized_) return;
        
        // Frame 1: Shield icon builds up
        for (int y = 0; y < 32; y += 2) {
            display_->clearDisplay();
            display_->fillRect(48, 16, 32, y, SSD1306_WHITE);
            display_->display();
            delay(15);
        }
        
        // Frame 2: Shield outline
        display_->clearDisplay();
        display_->drawBitmap(56, 12, icon_shield, 8, 8, SSD1306_WHITE);
        // Scale shield icon
        for (int s = 1; s <= 4; s++) {
            display_->clearDisplay();
            int x = 64 - (s * 4);
            int y = 20 - (s * 4);
            display_->drawRect(x, y, s * 8, s * 8, SSD1306_WHITE);
            display_->display();
            delay(80);
        }
        
        // Frame 3: "ENCENDIDO" typewriter effect
        display_->clearDisplay();
        display_->drawBitmap(60, 8, icon_shield, 8, 8, SSD1306_WHITE);
        display_->setTextSize(1);
        const char* text = "ENCENDIDO";
        int len = strlen(text);
        int startX = 64 - (len * 3); // center text
        for (int i = 0; i < len; i++) {
            display_->setCursor(startX + (i * 6), 30);
            display_->print(text[i]);
            display_->display();
            delay(60);
        }
        
        // Frame 4: Progress bar
        display_->drawRect(14, 46, 100, 8, SSD1306_WHITE);
        for (int w = 0; w < 98; w += 2) {
            display_->fillRect(15, 47, w, 6, SSD1306_WHITE);
            display_->display();
            delay(15);
        }
        
        // Frame 5: Flash effect
        display_->invertDisplay(true);
        delay(100);
        display_->invertDisplay(false);
        delay(200);
        
        Serial.println(F("[DISPLAY] Boot animation complete"));
    }

    // ========================================================================
    // SHUTDOWN ANIMATION - "APAGADO"
    // ========================================================================
    void playShutdownAnimation() {
        if (!initialized_) return;
        
        display_->clearDisplay();
        display_->setTextSize(2);
        display_->setCursor(16, 10);
        display_->print(F("APAGADO"));
        display_->display();
        delay(500);
        
        // Shrink effect - lines closing from top and bottom
        for (int i = 0; i < 32; i++) {
            display_->drawLine(0, i, 127, i, SSD1306_BLACK);
            display_->drawLine(0, 63 - i, 127, 63 - i, SSD1306_BLACK);
            display_->display();
            delay(20);
        }
        
        // Single dot in center then off
        display_->clearDisplay();
        display_->drawPixel(64, 32, SSD1306_WHITE);
        display_->display();
        delay(300);
        display_->clearDisplay();
        display_->display();
    }

    // ========================================================================
    // GENERAL STATUS SCREEN
    // ========================================================================
    void showGeneralStatus(bool wifiOk, bool mqttOk, const char* securityState,
                           float temp, float humidity,
                           const char* radarMode, bool doorOpen) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::GENERAL_STATUS;

        display_->clearDisplay();

        // Top bar - status icons
        if (wifiOk) display_->drawBitmap(0, 0, icon_wifi, 8, 8, SSD1306_WHITE);
        if (mqttOk) display_->drawBitmap(12, 0, icon_mqtt, 8, 8, SSD1306_WHITE);
        if (wifiOk && mqttOk) {
            display_->drawBitmap(110, 0, icon_lock, 8, 8, SSD1306_WHITE);
        } else {
            display_->drawBitmap(110, 0, icon_unlock, 8, 8, SSD1306_WHITE);
        }
        display_->drawLine(0, 10, 127, 10, SSD1306_WHITE);

        display_->setTextSize(1);

        // Line 1: Connectivity
        display_->setCursor(0, 14);
        display_->print(F("WiFi:"));
        display_->print(wifiOk ? F("OK") : F("NO"));
        display_->print(F(" MQTT:"));
        display_->print(mqttOk ? F("OK") : F("NO"));

        // Line 2: Temperature & Humidity
        display_->setCursor(0, 26);
        display_->print(F("T:"));
        display_->print(temp, 1);
        display_->print(F("C H:"));
        display_->print(humidity, 0);
        display_->print(F("%"));

        // Line 3: Security state
        display_->setCursor(0, 38);
        display_->print(F("Seg:"));
        display_->print(securityState);

        // Line 4: Radar & door
        display_->setCursor(0, 50);
        display_->print(F("Rad:"));
        display_->print(radarMode);
        display_->print(F(" D:"));
        display_->print(doorOpen ? F("ABI") : F("CER"));

        display_->display();
    }

    // ========================================================================
    // PRESENCE DETECTED SCREEN
    // ========================================================================
    void showPresenceDetected(float distance, int angle, const char* mode) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::PRESENCE_VIEW;

        display_->clearDisplay();

        display_->setTextSize(1);
        display_->setCursor(14, 0);
        display_->print(F("PRESENCIA DETECTADA"));
        display_->drawLine(0, 10, 127, 10, SSD1306_WHITE);

        display_->setCursor(0, 16);
        display_->print(F("Modo: "));
        display_->print(mode);

        display_->setCursor(0, 28);
        display_->print(F("Dist: "));
        if (distance > 0 && distance < RADAR_MAX_DISTANCE) {
            display_->print((int)distance);
            display_->print(F(" cm"));
        } else {
            display_->print(F("---"));
        }

        display_->setCursor(0, 38);
        display_->print(F("Ang: "));
        display_->print(angle);
        display_->print(F("\xF7"));

        int barWidth = 0;
        if (distance > 0 && distance < RADAR_MAX_DISTANCE) {
            float clamped = min(distance, (float)RADAR_MAX_DISTANCE);
            barWidth = (int)((1.0f - (clamped / (float)RADAR_MAX_DISTANCE)) * 120.0f);
            if (barWidth < 0) barWidth = 0;
            if (barWidth > 120) barWidth = 120;
        }

        display_->drawRect(4, 52, 120, 8, SSD1306_WHITE);
        if (barWidth > 0) {
            display_->fillRect(5, 53, barWidth, 6, SSD1306_WHITE);
        }

        display_->display();
    }

    // ========================================================================
    // ALARM SCREEN
    // ========================================================================
    void showAlarmScreen(const char* reason) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::ALARM_VIEW;

        display_->clearDisplay();

        unsigned long now = millis();
        if ((now / 200) % 2 == 0) {
            display_->drawRect(0, 0, 128, 64, SSD1306_WHITE);
            display_->drawRect(1, 1, 126, 62, SSD1306_WHITE);
        }

        display_->drawBitmap(56, 6, icon_alert, 8, 8, SSD1306_WHITE);
        display_->setTextSize(2);
        display_->setCursor(18, 18);
        display_->print(F("ALARMA"));

        display_->setTextSize(1);
        display_->setCursor(6, 44);
        display_->print(reason);

        display_->display();
    }

    // ========================================================================
    // RADAR VIEW SCREEN
    // ========================================================================
    void showRadarView(int currentAngle, float distance, RadarMode mode,
                       const float* scanMap, int mapSize) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::RADAR_VIEW;
        
        display_->clearDisplay();
        
        // Title
        display_->setTextSize(1);
        display_->setCursor(0, 0);
        display_->print(F("RADAR"));
        
        // Mode indicator
        display_->setCursor(80, 0);
        switch (mode) {
            case RadarMode::PATROL:    display_->print(F("[PATROL]")); break;
            case RadarMode::DETECTION: display_->print(F("[DETECT]")); break;
            case RadarMode::TRACKING:  display_->print(F("[TRACK]"));  break;
            case RadarMode::MANUAL:    display_->print(F("[MANUAL]")); break;
            case RadarMode::OFF:      display_->print(F("[OFF]"));    break;
        }
        
        // Draw radar arc (bottom-left origin)
        int cx = 30, cy = 60;
        int radius = 50;
        
        // Draw arc outline
        for (int a = 0; a <= 90; a += 5) {
            float rad = radians(a);
            int x = cx + (int)(radius * cos(rad));
            int y = cy - (int)(radius * sin(rad));
            display_->drawPixel(x, y, SSD1306_WHITE);
        }
        
        // Draw range rings
        for (int r = 15; r <= 45; r += 15) {
            for (int a = 0; a <= 90; a += 10) {
                float rad = radians(a);
                int x = cx + (int)(r * cos(rad));
                int y = cy - (int)(r * sin(rad));
                display_->drawPixel(x, y, SSD1306_WHITE);
            }
        }
        
        // Draw scan points from map
        if (scanMap != nullptr) {
            int minA = (mapSize > 90) ? 0 : 0;
            int maxA = min(mapSize - 1, 90);
            for (int a = minA; a <= maxA; a += 2) {
                if (scanMap[a] > 0 && scanMap[a] < RADAR_MAX_DISTANCE) {
                    float normDist = (scanMap[a] / (float)RADAR_MAX_DISTANCE) * radius;
                    normDist = min(normDist, (float)radius);
                    float rad = radians(a);
                    int x = cx + (int)(normDist * cos(rad));
                    int y = cy - (int)(normDist * sin(rad));
                    display_->fillCircle(x, y, 1, SSD1306_WHITE);
                }
            }
        }
        
        // Draw current sweep line
        float rad = radians(currentAngle);
        int lx = cx + (int)(radius * cos(rad));
        int ly = cy - (int)(radius * sin(rad));
        display_->drawLine(cx, cy, lx, ly, SSD1306_WHITE);
        
        // Info panel (right side)
        display_->setCursor(80, 14);
        display_->print(F("Ang:"));
        display_->print(currentAngle);
        display_->print(F("\xF7")); // degree symbol
        
        display_->setCursor(80, 26);
        display_->print(F("Dst:"));
        if (distance > 0 && distance < RADAR_MAX_DISTANCE) {
            display_->print((int)distance);
            display_->print(F("cm"));
        } else {
            display_->print(F("---"));
        }
        
        display_->display();
    }

    // ========================================================================
    // TRACKING VIEW SCREEN
    // ========================================================================
    void showTrackingView(int targetAngle, float distance, int predictedAngle) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::TRACKING_VIEW;
        
        display_->clearDisplay();
        
        // Flashing "TRACKING" header
        unsigned long now = millis();
        if ((now / 300) % 2 == 0) {
            display_->setTextSize(1);
            display_->setCursor(28, 0);
            display_->print(F(">> TRACKING <<"));
        }
        
        // Target icon
        display_->drawBitmap(60, 14, icon_target, 8, 8, SSD1306_WHITE);
        
        // Crosshair around target
        display_->drawLine(56, 18, 72, 18, SSD1306_WHITE);
        display_->drawLine(64, 10, 64, 26, SSD1306_WHITE);
        display_->drawCircle(64, 18, 12, SSD1306_WHITE);
        
        // Info
        display_->setTextSize(1);
        display_->setCursor(0, 32);
        display_->print(F("Angulo:    "));
        display_->print(targetAngle);
        display_->print(F("\xF7"));
        
        display_->setCursor(0, 42);
        display_->print(F("Distancia: "));
        display_->print((int)distance);
        display_->print(F("cm"));
        
        display_->setCursor(0, 52);
        display_->print(F("Prediccion:"));
        display_->print(predictedAngle);
        display_->print(F("\xF7"));
        
        display_->display();
    }

    // ========================================================================
    // ALERT MODE SCREEN
    // ========================================================================
    void showAlertMode(const char* alertType, const char* details) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::ALERT_MODE;
        
        display_->clearDisplay();
        
        // Flashing border
        unsigned long now = millis();
        if ((now / 200) % 2 == 0) {
            display_->drawRect(0, 0, 128, 64, SSD1306_WHITE);
            display_->drawRect(1, 1, 126, 62, SSD1306_WHITE);
        }
        
        // Alert icon
        display_->drawBitmap(56, 6, icon_alert, 8, 8, SSD1306_WHITE);
        
        // "ALERTA" large text
        display_->setTextSize(2);
        display_->setCursor(22, 20);
        display_->print(F("ALERTA"));
        
        // Alert type
        display_->setTextSize(1);
        display_->setCursor(4, 42);
        display_->print(alertType);
        
        // Details
        if (details) {
            display_->setCursor(4, 54);
            display_->print(details);
        }
        
        display_->display();
    }

    // ========================================================================
    // BLE DEVICE SAFE SCREEN
    // ========================================================================
    void showBLESafe(const char* deviceName, int rssi) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::BLE_STATUS;
        
        display_->clearDisplay();
        display_->setTextSize(1);
        display_->setCursor(10, 4);
        display_->print(F("DISPOSITIVO SEGURO"));
        display_->drawLine(0, 14, 127, 14, SSD1306_WHITE);
        
        display_->setCursor(0, 20);
        display_->print(F("Nombre: "));
        display_->print(deviceName);
        
        display_->setCursor(0, 32);
        display_->print(F("RSSI: "));
        display_->print(rssi);
        display_->print(F(" dBm"));
        
        // Signal bars
        int bars = 0;
        if (rssi > -50) bars = 4;
        else if (rssi > -60) bars = 3;
        else if (rssi > -70) bars = 2;
        else if (rssi > -80) bars = 1;
        
        for (int i = 0; i < 4; i++) {
            int h = 4 + (i * 3);
            int x = 80 + (i * 8);
            if (i < bars) {
                display_->fillRect(x, 50 - h, 5, h, SSD1306_WHITE);
            } else {
                display_->drawRect(x, 50 - h, 5, h, SSD1306_WHITE);
            }
        }
        
        display_->display();
    }

    // ========================================================================
    // RFID ACCESS SCREEN
    // ========================================================================
    void showRFIDAccess(const char* cardName, bool granted, const char* level) {
        if (!initialized_) return;
        currentScreen_ = DisplayScreen::RFID_STATUS;
        
        display_->clearDisplay();
        display_->setTextSize(1);
        display_->setCursor(20, 4);
        display_->print(F("ACCESO RFID"));
        display_->drawLine(0, 14, 127, 14, SSD1306_WHITE);
        
        display_->setCursor(0, 20);
        display_->print(F("Tarjeta: "));
        display_->print(cardName);
        
        display_->setCursor(0, 32);
        display_->print(F("Nivel: "));
        display_->print(level);
        
        display_->setTextSize(2);
        if (granted) {
            display_->setCursor(10, 46);
            display_->print(F("ACCESO OK"));
        } else {
            display_->setCursor(10, 46);
            display_->print(F("DENEGADO"));
        }
        
        display_->display();
    }

    // ========================================================================
    // UTILITY
    // ========================================================================
    void clear() {
        if (!initialized_) return;
        display_->clearDisplay();
        display_->display();
    }

    void setBrightness(uint8_t brightness) {
        if (!initialized_) return;
        display_->ssd1306_command(SSD1306_SETCONTRAST);
        display_->ssd1306_command(brightness);
    }

    void dimDisplay(bool dim) {
        setBrightness(dim ? 0 : 255);
        dimmed_ = dim;
    }

    bool isDimmed() const { return dimmed_; }
    bool isInitialized() const { return initialized_; }
    DisplayScreen getCurrentScreen() const { return currentScreen_; }
    Adafruit_SSD1306* getDisplay() { return display_; }

private:
    Adafruit_SSD1306* display_ = nullptr;
    bool initialized_ = false;
    bool dimmed_ = false;
    DisplayScreen currentScreen_ = DisplayScreen::BOOT;
};
