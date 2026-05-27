#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// BLE Manager - Thread-safe BLE scanning with FreeRTOS task on Core 0
// 
// BLE scanning runs on Core 0 (same as WiFi - they share the radio).
// Device state is accessed from Core 1 via mutex-protected getters.
// ============================================================================

class BLEManager {
public:
    void begin() {
        bleMutex_ = xSemaphoreCreateMutex();
        if (!bleMutex_) {
            Serial.println(F("[BLE] FATAL: Failed to create mutex!"));
            return;
        }
        
        BLEDevice::init("ESP32_Security");
        pScan_ = BLEDevice::getScan();
        pScan_->setActiveScan(true);    // active scan = requests friendly names!
        pScan_->setInterval(100);
        pScan_->setWindow(50);
        loadConfig();
        loadDevices();
        Serial.print(F("[BLE] Initialized, registered devices: "));
        Serial.println(deviceCount_);
        
        // Launch BLE scan task on Core 0 (shares radio with WiFi)
        xTaskCreatePinnedToCore(
            bleScanTaskStatic,
            "BLEScanTask",
            STACK_SIZE_BLE,
            this,
            PRIORITY_BLE,
            &bleTaskHandle_,
            CORE_NETWORK     // Core 0 - same as WiFi
        );
        Serial.println(F("[BLE] Scan task launched on Core 0"));
    }

    // ---- Thread-safe device registration (NVS) - can be called from any core ----
    bool registerDevice(const char* mac, const char* name) {
        xSemaphoreTake(bleMutex_, portMAX_DELAY);
        if (deviceCount_ >= MAX_BLE_DEVICES) {
            xSemaphoreGive(bleMutex_);
            return false;
        }
        if (findDeviceUnsafe(mac) >= 0) {
            xSemaphoreGive(bleMutex_);
            return false;
        }

        strncpy(devices_[deviceCount_].mac, mac, 17);
        devices_[deviceCount_].mac[17] = '\0';
        strncpy(devices_[deviceCount_].name, name, 19);
        devices_[deviceCount_].name[19] = '\0';
        devices_[deviceCount_].active = false;
        devices_[deviceCount_].rssi = -100;
        devices_[deviceCount_].lastSeen = 0;
        deviceCount_++;
        saveDevices();
        xSemaphoreGive(bleMutex_);
        
        Serial.print(F("[BLE] Registered: ")); Serial.println(name);
        return true;
    }

    bool removeDevice(const char* mac) {
        xSemaphoreTake(bleMutex_, portMAX_DELAY);
        int idx = findDeviceUnsafe(mac);
        if (idx < 0) {
            xSemaphoreGive(bleMutex_);
            return false;
        }
        for (int i = idx; i < deviceCount_ - 1; i++) {
            devices_[i] = devices_[i + 1];
        }
        deviceCount_--;
        saveDevices();
        xSemaphoreGive(bleMutex_);
        
        Serial.print(F("[BLE] Removed: ")); Serial.println(mac);
        return true;
    }

    bool updateDevice(const char* mac, const char* newName) {
        xSemaphoreTake(bleMutex_, portMAX_DELAY);
        int idx = findDeviceUnsafe(mac);
        if (idx < 0) {
            xSemaphoreGive(bleMutex_);
            return false;
        }
        strncpy(devices_[idx].name, newName, 19);
        devices_[idx].name[19] = '\0';
        saveDevices();
        xSemaphoreGive(bleMutex_);
        return true;
    }

    // ---- Thread-safe approach correlation (called from Core 1) ----
    bool isKnownDeviceApproaching() {
        bool approaching = false;
        if (xSemaphoreTake(bleMutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (int i = 0; i < deviceCount_; i++) {
                if (devices_[i].active && devices_[i].rssi > rssiNear_) {
                    approaching = true;
                    break;
                }
            }
            xSemaphoreGive(bleMutex_);
        }
        return approaching;
    }

    bool isKnownDeviceClose() {
        bool close = false;
        if (xSemaphoreTake(bleMutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (int i = 0; i < deviceCount_; i++) {
                if (devices_[i].active && devices_[i].rssi > rssiClose_) {
                    close = true;
                    break;
                }
            }
            xSemaphoreGive(bleMutex_);
        }
        return close;
    }

    // ---- Thread-safe getters ----
    int getDeviceCount() {
        int count = 0;
        if (xSemaphoreTake(bleMutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            count = deviceCount_;
            xSemaphoreGive(bleMutex_);
        }
        return count;
    }

    // Copy device entry to caller's buffer (thread-safe)
    bool getDeviceCopy(int idx, BLEDeviceEntry& out) {
        bool ok = false;
        if (xSemaphoreTake(bleMutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (idx >= 0 && idx < deviceCount_) {
                out = devices_[idx];
                ok = true;
            }
            xSemaphoreGive(bleMutex_);
        }
        return ok;
    }

    // Get a copy of the last known device (thread-safe)
    bool getLastKnownDeviceCopy(BLEDeviceEntry& out) {
        bool ok = false;
        if (xSemaphoreTake(bleMutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (lastKnownDeviceIdx_ >= 0 && lastKnownDeviceIdx_ < deviceCount_) {
                out = devices_[lastKnownDeviceIdx_];
                ok = true;
            }
            xSemaphoreGive(bleMutex_);
        }
        return ok;
    }

    // Legacy compatibility — returns pointer (only safe within mutex scope)
    const BLEDeviceEntry* getDevice(int idx) {
        if (idx < 0 || idx >= deviceCount_) return nullptr;
        return &devices_[idx];
    }
    const BLEDeviceEntry* getLastKnownDevice() {
        if (lastKnownDeviceIdx_ < 0) return nullptr;
        return &devices_[lastKnownDeviceIdx_];
    }

    bool hasKnownDeviceDetected() { bool d = knownDeviceDetected_; knownDeviceDetected_ = false; return d; }
    bool hasScanCompleted() { bool s = scanComplete_; scanComplete_ = false; return s; }
    bool isScanning() const { return scanning_; }

    int getDiscoveredCount() {
        int count = 0;
        if (xSemaphoreTake(bleMutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            count = discoveredCount_;
            xSemaphoreGive(bleMutex_);
        }
        return count;
    }

    bool getDiscoveredCopy(int idx, BLEDeviceEntry& out) {
        bool ok = false;
        if (xSemaphoreTake(bleMutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (idx >= 0 && idx < discoveredCount_) {
                out = discovered_[idx];
                ok = true;
            }
            xSemaphoreGive(bleMutex_);
        }
        return ok;
    }

    void setEnabled(bool e) { enabled_ = e; }
    void blockScan(bool b) { scanBlocked_ = b; }
    int getRSSIClose() const { return rssiClose_; }
    int getRSSINear() const { return rssiNear_; }
    void setRSSIThresholds(int close, int near) { 
        rssiClose_ = close; 
        rssiNear_ = near; 
        saveConfig();
    }
    void setScanInterval(unsigned long ms) { 
        scanIntervalMs_ = ms; 
        saveConfig();
    }

    // update() is now a no-op — scanning happens in dedicated Core 0 task
    void update() { /* No-op: scanning moved to Core 0 FreeRTOS task */ }

private:
    // ========================================================================
    // BLE SCAN TASK (runs on Core 0)
    // ========================================================================
    static void bleScanTaskStatic(void* pvParameters) {
        BLEManager* self = static_cast<BLEManager*>(pvParameters);
        self->bleScanTaskLoop();
    }

    void bleScanTaskLoop() {
        for (;;) {
            if (!enabled_ || scanBlocked_) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            
            // Perform BLE scan (blocking on Core 0 is OK — doesn't affect Core 1)
            scanning_ = true;
            BLEScanResults results = pScan_->start(DEFAULT_BLE_SCAN_DURATION, false);
            scanning_ = false;
            
            // Process results under mutex
            xSemaphoreTake(bleMutex_, portMAX_DELAY);
            
            // Reset seen flags
            for (int i = 0; i < deviceCount_; i++) {
                devices_[i].active = false;
            }
            
            discoveredCount_ = 0; // reset discovered count for this scan
            
            unsigned long now = millis();
            int count = results.getCount();
            for (int i = 0; i < count; i++) {
                BLEAdvertisedDevice dev = results.getDevice(i);
                String mac = dev.getAddress().toString().c_str();
                mac.toUpperCase();
                int idx = findDeviceUnsafe(mac.c_str());
                if (idx >= 0) {
                    devices_[idx].active = true;
                    devices_[idx].rssi = dev.getRSSI();
                    devices_[idx].lastSeen = now;
                    knownDeviceDetected_ = true;
                    lastKnownDeviceIdx_ = idx;
                    
                    Serial.print(F("[BLE] Known device: "));
                    Serial.print(devices_[idx].name);
                    Serial.print(F(" RSSI: "));
                    Serial.println(devices_[idx].rssi);
                } else {
                    // Unregistered device - save to discovered_ list if we have space
                    if (discoveredCount_ < 15) {
                        strncpy(discovered_[discoveredCount_].mac, mac.c_str(), 17);
                        discovered_[discoveredCount_].mac[17] = '\0';
                        
                        String name = dev.haveName() ? dev.getName().c_str() : "Dispositivo Desconocido";
                        if (name.length() == 0) name = "Dispositivo Desconocido";
                        strncpy(discovered_[discoveredCount_].name, name.c_str(), 19);
                        discovered_[discoveredCount_].name[19] = '\0';
                        
                        discovered_[discoveredCount_].active = true;
                        discovered_[discoveredCount_].rssi = dev.getRSSI();
                        discovered_[discoveredCount_].lastSeen = now;
                        discoveredCount_++;
                    }
                }
            }
            
            xSemaphoreGive(bleMutex_);
            pScan_->clearResults();
            scanComplete_ = true;
            
            // Wait for scan interval
            vTaskDelay(pdMS_TO_TICKS(scanIntervalMs_));
        }
    }

    // Internal find (must be called with mutex held)
    int findDeviceUnsafe(const char* mac) const {
        for (int i = 0; i < deviceCount_; i++) {
            if (strncasecmp(devices_[i].mac, mac, 17) == 0) return i;
        }
        return -1;
    }

    void loadDevices() {
        Preferences prefs;
        prefs.begin("ble_dev", true);
        deviceCount_ = prefs.getUInt("count", 0);
        if (deviceCount_ > MAX_BLE_DEVICES) deviceCount_ = MAX_BLE_DEVICES;
        for (int i = 0; i < deviceCount_; i++) {
            String keyM = "mac_" + String(i);
            String keyN = "name_" + String(i);
            String mac = prefs.getString(keyM.c_str(), "");
            String name = prefs.getString(keyN.c_str(), "Unknown");
            strncpy(devices_[i].mac, mac.c_str(), 17);
            strncpy(devices_[i].name, name.c_str(), 19);
            devices_[i].active = false;
            devices_[i].rssi = -100;
        }
        prefs.end();
    }

    void saveDevices() {
        // Must be called with bleMutex_ held
        Preferences prefs;
        prefs.begin("ble_dev", false);
        prefs.putUInt("count", deviceCount_);
        for (int i = 0; i < deviceCount_; i++) {
            String keyM = "mac_" + String(i);
            String keyN = "name_" + String(i);
            prefs.putString(keyM.c_str(), devices_[i].mac);
            prefs.putString(keyN.c_str(), devices_[i].name);
        }
        prefs.end();
    }

    void loadConfig() {
        Preferences prefs;
        prefs.begin("ble_cfg", true);
        rssiClose_ = prefs.getInt("rssi_close", DEFAULT_BLE_RSSI_CLOSE);
        rssiNear_ = prefs.getInt("rssi_near", DEFAULT_BLE_RSSI_NEAR);
        scanIntervalMs_ = prefs.getULong("scan_int", DEFAULT_BLE_SCAN_INTERVAL_MS);
        prefs.end();
    }

    void saveConfig() {
        Preferences prefs;
        prefs.begin("ble_cfg", false);
        prefs.putInt("rssi_close", rssiClose_);
        prefs.putInt("rssi_near", rssiNear_);
        prefs.putULong("scan_int", scanIntervalMs_);
        prefs.end();
    }

    // FreeRTOS
    TaskHandle_t bleTaskHandle_ = nullptr;
    SemaphoreHandle_t bleMutex_ = nullptr;

    BLEScan* pScan_ = nullptr;
    BLEDeviceEntry devices_[MAX_BLE_DEVICES];
    int deviceCount_ = 0;
    BLEDeviceEntry discovered_[15];
    int discoveredCount_ = 0;
    int lastKnownDeviceIdx_ = -1;
    volatile bool enabled_ = true;
    volatile bool scanning_ = false;
    volatile bool scanBlocked_ = false;
    volatile bool knownDeviceDetected_ = false;
    volatile bool scanComplete_ = false;
    unsigned long scanIntervalMs_ = DEFAULT_BLE_SCAN_INTERVAL_MS;
    int rssiClose_ = DEFAULT_BLE_RSSI_CLOSE;
    int rssiNear_ = DEFAULT_BLE_RSSI_NEAR;
};
