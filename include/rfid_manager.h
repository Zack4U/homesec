#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// RFID Manager - RC522 card reader with permission levels and NVS storage
// Uses default VSPI bus (SCK=18, MISO=19, MOSI=23) - ECHO moved to GPIO 35
// ============================================================================

class RFIDManager {
public:
    void begin() {
        // SS and RST pin setup
        pinMode(PIN_RFID_SS, OUTPUT);
        digitalWrite(PIN_RFID_SS, HIGH);
        pinMode(PIN_RFID_RST, OUTPUT);
        digitalWrite(PIN_RFID_RST, HIGH);
        delay(50);
        
        // Use default VSPI (SCK=18, MISO=19, MOSI=23)
        SPI.begin();
        
        rfid_ = new MFRC522(PIN_RFID_SS, PIN_RFID_RST);
        rfid_->PCD_Init();
        delay(50);
        
        // Self-test
        byte v = rfid_->PCD_ReadRegister(MFRC522::VersionReg);
        if (v == 0x00 || v == 0xFF) {
            Serial.println(F("[RFID] WARNING: RC522 not detected!"));
            initialized_ = false;
        } else {
            initialized_ = true;
            Serial.print(F("[RFID] RC522 v0x"));
            Serial.print(v, HEX);
            Serial.println(F(" OK"));
        }
        
        loadCards();
        Serial.print(F("[RFID] Cards loaded: ")); Serial.println(cardCount_);
    }

    void update() {
        if (!enabled_ || !initialized_) return;
        unsigned long now = millis();
        if (now - lastCheck_ < RFID_CHECK_INTERVAL_MS) return;
        lastCheck_ = now;

        // Check guest expirations
        checkGuestExpiry(now);

        if (!rfid_->PICC_IsNewCardPresent()) return;
        if (!rfid_->PICC_ReadCardSerial()) return;

        // Build UID string
        char uid[16] = {0};
        for (byte i = 0; i < rfid_->uid.size && i < 7; i++) {
            sprintf(uid + (i * 2), "%02X", rfid_->uid.uidByte[i]);
        }

        rfid_->PICC_HaltA();
        rfid_->PCD_StopCrypto1();

        Serial.print(F("[RFID] Card: ")); Serial.println(uid);

        // Look up card
        int idx = findCard(uid);
        if (idx >= 0 && cards_[idx].active) {
            lastAccessGranted_ = true;
            lastAccessLevel_ = cards_[idx].level;
            strncpy(lastUID_, uid, 15);
            strncpy(lastName_, cards_[idx].name, 19);
            newAccess_ = true;
            Serial.print(F("[RFID] ACCESS GRANTED - "));
            Serial.println(cards_[idx].name);
        } else {
            lastAccessGranted_ = false;
            strncpy(lastUID_, uid, 15);
            strcpy(lastName_, "UNKNOWN");
            lastAccessLevel_ = RFIDPermission::GUEST;
            newAccess_ = true;
            Serial.println(F("[RFID] ACCESS DENIED"));
        }
    }

    // ---- Card Registration (NVS) ----
    bool registerCard(const char* uid, const char* name, RFIDPermission level, unsigned long expirySec = 0) {
        if (cardCount_ >= MAX_RFID_CARDS) return false;
        int existing = findCard(uid);
        if (existing >= 0) {
            // Update existing
            strncpy(cards_[existing].name, name, 19);
            cards_[existing].level = level;
            cards_[existing].expiryTime = (expirySec > 0) ? (millis() + expirySec * 1000) : 0;
            cards_[existing].active = true;
            saveCards();
            return true;
        }
        
        strncpy(cards_[cardCount_].uid, uid, 15);
        cards_[cardCount_].uid[15] = '\0';
        strncpy(cards_[cardCount_].name, name, 19);
        cards_[cardCount_].name[19] = '\0';
        cards_[cardCount_].level = level;
        cards_[cardCount_].expiryTime = (expirySec > 0) ? (millis() + expirySec * 1000) : 0;
        cards_[cardCount_].active = true;
        cardCount_++;
        saveCards();
        Serial.print(F("[RFID] Registered: ")); Serial.println(name);
        return true;
    }

    bool removeCard(const char* uid) {
        int idx = findCard(uid);
        if (idx < 0) return false;
        for (int i = idx; i < cardCount_ - 1; i++) cards_[i] = cards_[i + 1];
        cardCount_--;
        saveCards();
        return true;
    }

    void setGuestExpiry(const char* uid, unsigned long expirySec) {
        int idx = findCard(uid);
        if (idx >= 0) {
            cards_[idx].expiryTime = millis() + expirySec * 1000;
            cards_[idx].level = RFIDPermission::GUEST;
            saveCards();
        }
    }

    // ---- Getters ----
    bool hasNewAccess() { bool a = newAccess_; newAccess_ = false; return a; }
    bool wasAccessGranted() const { return lastAccessGranted_; }
    RFIDPermission getLastAccessLevel() const { return lastAccessLevel_; }
    const char* getLastUID() const { return lastUID_; }
    const char* getLastName() const { return lastName_; }
    int getCardCount() const { return cardCount_; }
    
    void setEnabled(bool e) { enabled_ = e; }
    bool isInitialized() const { return initialized_; }

private:
    int findCard(const char* uid) const {
        for (int i = 0; i < cardCount_; i++) {
            if (strcasecmp(cards_[i].uid, uid) == 0) return i;
        }
        return -1;
    }

    void checkGuestExpiry(unsigned long now) {
        for (int i = 0; i < cardCount_; i++) {
            if (cards_[i].level == RFIDPermission::GUEST && 
                cards_[i].expiryTime > 0 && now >= cards_[i].expiryTime) {
                cards_[i].active = false;
                Serial.print(F("[RFID] Guest expired: ")); 
                Serial.println(cards_[i].name);
            }
        }
    }

    void loadCards() {
        Preferences prefs;
        prefs.begin("rfid_dev", true);
        cardCount_ = prefs.getUInt("count", 0);
        if (cardCount_ > MAX_RFID_CARDS) cardCount_ = MAX_RFID_CARDS;
        for (int i = 0; i < cardCount_; i++) {
            String kU = "uid_" + String(i);
            String kN = "name_" + String(i);
            String kL = "lvl_" + String(i);
            prefs.getString(kU.c_str(), "").toCharArray(cards_[i].uid, 16);
            prefs.getString(kN.c_str(), "").toCharArray(cards_[i].name, 20);
            cards_[i].level = (RFIDPermission)prefs.getUChar(kL.c_str(), 0);
            cards_[i].expiryTime = 0;
            cards_[i].active = true;
        }
        prefs.end();
    }

    void saveCards() {
        Preferences prefs;
        prefs.begin("rfid_dev", false);
        prefs.putUInt("count", cardCount_);
        for (int i = 0; i < cardCount_; i++) {
            String kU = "uid_" + String(i);
            String kN = "name_" + String(i);
            String kL = "lvl_" + String(i);
            prefs.putString(kU.c_str(), cards_[i].uid);
            prefs.putString(kN.c_str(), cards_[i].name);
            prefs.putUChar(kL.c_str(), (uint8_t)cards_[i].level);
        }
        prefs.end();
    }

    MFRC522* rfid_ = nullptr;
    RFIDCardEntry cards_[MAX_RFID_CARDS];
    int cardCount_ = 0;
    bool enabled_ = true, initialized_ = false;
    bool newAccess_ = false, lastAccessGranted_ = false;
    RFIDPermission lastAccessLevel_ = RFIDPermission::GUEST;
    char lastUID_[16] = {0};
    char lastName_[20] = {0};
    unsigned long lastCheck_ = 0;
};
