#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

// ============================================================================
// MQTT Manager - Dual-Core Non-Blocking Architecture
// 
// Core 0: Dedicated FreeRTOS task handles WiFi, MQTT connect/loop/publish
// Core 1: Arduino loop() enqueues publish requests via FreeRTOS Queue
//
// Communication:
//   - publishQueue_: Core 1 -> Core 0 (outbound MQTT messages)
//   - incomingQueue_: Core 0 -> Core 1 (incoming MQTT commands)
//   - stateMutex_: Protects shared connection state flags
// ============================================================================

// Forward declare callback type (used on Core 1 to process incoming commands)
typedef void (*MqttMessageCallback)(const char* topic, JsonDocument& doc);

class MQTTManager {
public:
    // ---- Initialization (called from setup() on Core 1) ----
    void begin(MqttMessageCallback callback) {
        userCallback_ = callback;
        loadConfig();
        
        // Create FreeRTOS synchronization primitives
        stateMutex_ = xSemaphoreCreateMutex();
        publishQueue_ = xQueueCreate(MQTT_PUBLISH_QUEUE_SIZE, sizeof(MqttOutMessage));
        incomingQueue_ = xQueueCreate(MQTT_INCOMING_QUEUE_SIZE, sizeof(MqttInMessage));
        
        if (!publishQueue_ || !incomingQueue_ || !stateMutex_) {
            Serial.println(F("[MQTT] FATAL: Failed to create queues/mutex!"));
            return;
        }
        
        // Launch network task on Core 0
        xTaskCreatePinnedToCore(
            networkTaskStatic,      // Task function
            "NetworkTask",          // Name
            STACK_SIZE_NETWORK,     // Stack size
            this,                   // Parameter (pass this pointer)
            PRIORITY_NETWORK,       // Priority
            &networkTaskHandle_,    // Task handle
            CORE_NETWORK            // Core 0
        );
        
        Serial.println(F("[MQTT] Network task launched on Core 0"));
    }

    // ---- Process incoming MQTT commands (called from loop() on Core 1) ----
    // This replaces the old update() method - now only processes incoming commands
    void processIncoming() {
        MqttInMessage msg;
        // Process up to 4 messages per call to avoid hogging Core 1
        for (int i = 0; i < 4; i++) {
            if (xQueueReceive(incomingQueue_, &msg, 0) == pdTRUE) {
                // Parse JSON and route to user callback
                StaticJsonDocument<512> doc;
                DeserializationError err = deserializeJson(doc, msg.payload);
                if (err) {
                    Serial.print(F("[MQTT] JSON parse error: "));
                    Serial.println(err.c_str());
                    continue;
                }
                if (userCallback_) {
                    userCallback_(msg.topic, doc);
                }
            } else {
                break; // Queue empty
            }
        }
    }

    // ---- Non-blocking publish helpers (enqueue for Core 0) ----
    bool publish(const char* topic, const char* payload, bool retained = false) {
        MqttOutMessage msg;
        strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
        msg.topic[sizeof(msg.topic) - 1] = '\0';
        strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
        msg.payload[sizeof(msg.payload) - 1] = '\0';
        msg.retained = retained;
        
        // Non-blocking enqueue (don't wait if queue is full - drop message)
        if (xQueueSend(publishQueue_, &msg, 0) != pdTRUE) {
            // Queue full - message dropped (not critical, will retry next cycle)
            return false;
        }
        return true;
    }

    bool publishJson(const char* topic, JsonDocument& doc, bool retained = false) {
        char buffer[512];
        serializeJson(doc, buffer, sizeof(buffer));
        return publish(topic, buffer, retained);
    }

    // --- Specific publish methods (all non-blocking via queue) ---
    void publishRadarScan(int angle, float distance, const char* mode) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<128> doc;
        doc["angle"] = angle;
        doc["distance"] = (int)distance;
        doc["mode"] = mode;
        publishJson(TOPIC_RADAR_SCAN, doc);
    }

    void publishRadarAlert(int angle, float distance, bool approaching) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<128> doc;
        doc["angle"] = angle;
        doc["distance"] = (int)distance;
        doc["approaching"] = approaching;
        publishJson(TOPIC_RADAR_ALERT, doc);
    }

    void publishRadarTracking(int angle, float distance, int predicted) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<128> doc;
        doc["angle"] = angle;
        doc["distance"] = (int)distance;
        doc["predicted_angle"] = predicted;
        publishJson(TOPIC_RADAR_TRACKING, doc);
    }

    void publishClimate(float temp, float humidity) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<96> doc;
        doc["temperature"] = serialized(String(temp, 1));
        doc["humidity"] = serialized(String(humidity, 1));
        publishJson(TOPIC_CLIMATE_DATA, doc);
    }

    void publishLight(bool dark, bool flashlight) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<64> doc;
        doc["dark"] = dark;
        doc["flashlight"] = flashlight;
        publishJson(TOPIC_LIGHT_STATUS, doc);
    }

    void publishAvoidance(bool triggered) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<64> doc;
        doc["triggered"] = triggered;
        doc["location"] = "entry";
        publishJson(TOPIC_AVOIDANCE, doc);
    }

    void publishBLEDevice(const char* mac, int rssi, const char* name, bool known) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<192> doc;
        doc["mac"] = mac;
        doc["rssi"] = rssi;
        doc["name"] = name;
        doc["known"] = known;
        publishJson(TOPIC_BLE_DEVICE, doc);
    }

    void publishRFIDAccess(const char* uid, bool granted, const char* level, const char* name) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<192> doc;
        doc["uid"] = uid;
        doc["granted"] = granted;
        doc["level"] = level;
        doc["name"] = name;
        publishJson(TOPIC_RFID_ACCESS, doc);
    }

    void publishDoorStatus(bool open, const char* reason) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<96> doc;
        doc["open"] = open;
        doc["reason"] = reason;
        publishJson(TOPIC_DOOR_STATUS, doc);
    }

    void publishCameraCommand(const char* action) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<64> doc;
        doc["action"] = action;
        publishJson(TOPIC_CAMERA_CMD, doc);
    }

    void publishSystemStatus(bool armed, const char* mode, unsigned long uptime) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<128> doc;
        doc["armed"] = armed;
        doc["mode"] = mode;
        doc["uptime"] = uptime / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["online"] = true;
        publishJson(TOPIC_SYSTEM_STATUS, doc);
    }

    void publishTouchEvent(bool emergencyToggle, bool armed) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<64> doc;
        doc["emergency_toggle"] = emergencyToggle;
        doc["armed"] = armed;
        publishJson(TOPIC_TOUCH_EVENT, doc);
    }

    void publishDisplayStatus(const char* screen) {
        if (!isMQTTConnected()) return;
        StaticJsonDocument<64> doc;
        doc["screen"] = screen;
        publishJson(TOPIC_DISPLAY_STATUS, doc);
    }

    // ---- WiFi config (thread-safe) ----
    void setWiFiCredentials(const char* ssid, const char* pass) {
        if (ssid == nullptr || ssid[0] == '\0') {
            Serial.println(F("[MQTT] Ignoring empty WiFi SSID"));
            return;
        }

        xSemaphoreTake(stateMutex_, portMAX_DELAY);
        strncpy(wifiSSID_, ssid, 31);
        wifiSSID_[31] = '\0';
        strncpy(wifiPass_, pass, 63);
        wifiPass_[63] = '\0';
        wifiCredentialsChanged_ = true;
        xSemaphoreGive(stateMutex_);
        
        saveConfig();
        Serial.println(F("[MQTT] WiFi credentials updated"));
    }

    // ---- Thread-safe getters (can be called from any core) ----
    bool isWiFiConnected() {
        bool connected = false;
        if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
            connected = wifiConnected_;
            xSemaphoreGive(stateMutex_);
        }
        return connected;
    }

    bool isMQTTConnected() {
        bool connected = false;
        if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
            connected = mqttConnected_;
            xSemaphoreGive(stateMutex_);
        }
        return connected;
    }

    IPAddress getLocalIP() const { return WiFi.localIP(); }

private:
    // ========================================================================
    // NETWORK TASK (runs on Core 0)
    // ========================================================================
    static void networkTaskStatic(void* pvParameters) {
        MQTTManager* self = static_cast<MQTTManager*>(pvParameters);
        self->networkTaskLoop();
    }

    void networkTaskLoop() {
        // Initialize WiFi on Core 0
        WiFi.mode(WIFI_STA);
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(WIFI_PS_MIN_MODEM);
        
        // Initial WiFi connection attempt
        if (strlen(wifiSSID_) > 0 && strcmp(wifiSSID_, "YOUR_SSID") != 0) {
            WiFi.begin(wifiSSID_, wifiPass_);
            Serial.print(F("[WIFI] Connecting to "));
            Serial.print(wifiSSID_);
            Serial.println(F(" (Core 0)..."));
        } else {
            Serial.println(F("[WIFI] No valid SSID configured - running offline"));
        }
        
        // MQTT client setup
        mqtt_.setClient(espClient_);
        mqtt_.setServer(mqttServer_, mqttPort_);
        mqtt_.setBufferSize(1024);
        mqtt_.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->onMessage(topic, payload, length);
        });
        
        Serial.println(F("[MQTT] Network task running on Core 0"));
        
        unsigned long lastWifiRetry = 0;
        unsigned long lastMqttRetry = 0;
        
        // Main network loop - runs forever on Core 0
        for (;;) {
            unsigned long now = millis();
            
            // --- Check for WiFi credential changes ---
            bool credChanged = false;
            xSemaphoreTake(stateMutex_, portMAX_DELAY);
            credChanged = wifiCredentialsChanged_;
            wifiCredentialsChanged_ = false;
            xSemaphoreGive(stateMutex_);
            
            if (credChanged) {
                WiFi.disconnect(false, false);
                vTaskDelay(pdMS_TO_TICKS(100));
                WiFi.begin(wifiSSID_, wifiPass_);
                Serial.println(F("[WIFI] Reconnecting with new credentials..."));
            }
            
            // --- WiFi management ---
            if (WiFi.status() != WL_CONNECTED) {
                bool wasConnected = false;
                xSemaphoreTake(stateMutex_, portMAX_DELAY);
                wasConnected = wifiConnected_;
                wifiConnected_ = false;
                mqttConnected_ = false;
                xSemaphoreGive(stateMutex_);
                
                if (wasConnected) {
                    Serial.println(F("[WIFI] Connection lost"));
                }
                
                if (now - lastWifiRetry > WIFI_RETRY_INTERVAL_MS) {
                    lastWifiRetry = now;
                    connectWiFi();
                }
                
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            // WiFi just connected
            bool wasConnected = false;
            xSemaphoreTake(stateMutex_, portMAX_DELAY);
            wasConnected = wifiConnected_;
            wifiConnected_ = true;
            xSemaphoreGive(stateMutex_);
            
            if (!wasConnected) {
                Serial.print(F("[WIFI] Connected! IP: "));
                Serial.println(WiFi.localIP());
            }
            
            // --- MQTT management ---
            if (!mqtt_.connected()) {
                xSemaphoreTake(stateMutex_, portMAX_DELAY);
                mqttConnected_ = false;
                xSemaphoreGive(stateMutex_);
                
                if (now - lastMqttRetry > MQTT_RECONNECT_INTERVAL_MS) {
                    lastMqttRetry = now;
                    connectMQTT();
                }
            } else {
                xSemaphoreTake(stateMutex_, portMAX_DELAY);
                mqttConnected_ = true;
                xSemaphoreGive(stateMutex_);
                
                // Process MQTT keepalive and incoming messages
                mqtt_.loop();
                
                // Process outbound publish queue
                MqttOutMessage outMsg;
                // Process up to 8 messages per iteration
                for (int i = 0; i < 8; i++) {
                    if (xQueueReceive(publishQueue_, &outMsg, 0) == pdTRUE) {
                        mqtt_.publish(outMsg.topic, outMsg.payload, outMsg.retained);
                    } else {
                        break;
                    }
                }
            }
            
            // Yield to other Core 0 tasks (WiFi stack, BLE)
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    // ---- WiFi connection (called on Core 0) ----
    void connectWiFi() {
        if (strlen(wifiSSID_) == 0 || strcmp(wifiSSID_, "YOUR_SSID") == 0) {
            return;
        }
        Serial.print(F("[WIFI] Reconnecting to ")); Serial.println(wifiSSID_);
        WiFi.disconnect(false, false);
        WiFi.begin(wifiSSID_, wifiPass_);
    }

    // ---- MQTT connection (called on Core 0, blocking is OK here) ----
    void connectMQTT() {
        Serial.print(F("[MQTT] Connecting to broker..."));
        
        const char* willTopic = TOPIC_SYSTEM_STATUS;
        const char* willMessage = "{\"armed\":false,\"mode\":\"offline\",\"uptime\":0,\"freeHeap\":0,\"online\":false}";
        
        if (mqtt_.connect(DEFAULT_MQTT_CLIENT_ID, mqttUser_, mqttPass_, willTopic, 0, true, willMessage)) {
            Serial.println(F(" OK!"));
            subscribeAll();
            
            xSemaphoreTake(stateMutex_, portMAX_DELAY);
            mqttConnected_ = true;
            xSemaphoreGive(stateMutex_);
        } else {
            Serial.print(F(" FAILED rc="));
            Serial.println(mqtt_.state());
        }
    }

    void subscribeAll() {
        mqtt_.subscribe(TOPIC_CONFIG_RADAR);
        mqtt_.subscribe(TOPIC_CONFIG_LIGHT);
        mqtt_.subscribe(TOPIC_CONFIG_DETECTION);
        mqtt_.subscribe(TOPIC_CONFIG_WIFI);
        mqtt_.subscribe(TOPIC_CONFIG_BLE);
        mqtt_.subscribe(TOPIC_CONTROL_ARM);
        mqtt_.subscribe(TOPIC_CONTROL_CAMERA);
        mqtt_.subscribe(TOPIC_CONTROL_FLASHLIGHT);
        mqtt_.subscribe(TOPIC_CONTROL_SERVO);
        mqtt_.subscribe(TOPIC_CONTROL_DOOR);
        mqtt_.subscribe(TOPIC_BLE_REGISTER);
        mqtt_.subscribe(TOPIC_RFID_REGISTER);
        mqtt_.subscribe(TOPIC_RFID_GUEST_TIME);
        Serial.println(F("[MQTT] Subscribed to all topics"));
    }

    // ---- Incoming message handler (called on Core 0 by PubSubClient) ----
    void onMessage(char* topic, byte* payload, unsigned int length) {
        MqttInMessage msg;
        strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
        msg.topic[sizeof(msg.topic) - 1] = '\0';
        
        size_t len = min((unsigned int)(sizeof(msg.payload) - 1), length);
        memcpy(msg.payload, payload, len);
        msg.payload[len] = '\0';

        Serial.print(F("[MQTT] RX: ")); Serial.print(topic);
        Serial.print(F(" -> ")); Serial.println(msg.payload);

        // Enqueue for Core 1 processing (non-blocking)
        if (xQueueSend(incomingQueue_, &msg, 0) != pdTRUE) {
            Serial.println(F("[MQTT] WARNING: Incoming queue full, command dropped!"));
        }
    }

    // ---- Config persistence ----
    void loadConfig() {
        Preferences prefs;
        if (!prefs.begin("wifi_cfg", true)) {
            return;
        }

        String ssid = prefs.getString("ssid", DEFAULT_WIFI_SSID);
        String pass = prefs.getString("pass", DEFAULT_WIFI_PASS);
        if (ssid.length() == 0) ssid = DEFAULT_WIFI_SSID;
        if (pass.length() == 0) pass = DEFAULT_WIFI_PASS;
        ssid.toCharArray(wifiSSID_, 32);
        pass.toCharArray(wifiPass_, 64);
        prefs.end();

        if (!prefs.begin("mqtt_cfg", true)) {
            return;
        }

        String server = prefs.getString("server", DEFAULT_MQTT_SERVER);
        if (server.length() == 0) server = DEFAULT_MQTT_SERVER;
        server.toCharArray(mqttServer_, 64);
        mqttPort_ = prefs.getUShort("port", DEFAULT_MQTT_PORT);
        String user = prefs.getString("user", DEFAULT_MQTT_USER);
        String mpass = prefs.getString("pass", DEFAULT_MQTT_PASS);
        if (user.length() == 0) user = DEFAULT_MQTT_USER;
        if (mpass.length() == 0) mpass = DEFAULT_MQTT_PASS;
        user.toCharArray(mqttUser_, 32);
        mpass.toCharArray(mqttPass_, 64);
        prefs.end();
    }

    void saveConfig() {
        Preferences prefs;
        prefs.begin("wifi_cfg", false);
        prefs.putString("ssid", wifiSSID_);
        prefs.putString("pass", wifiPass_);
        prefs.end();
    }

    // ---- Members ----
    WiFiClient espClient_;
    PubSubClient mqtt_;
    MqttMessageCallback userCallback_ = nullptr;

    // FreeRTOS primitives
    TaskHandle_t networkTaskHandle_ = nullptr;
    QueueHandle_t publishQueue_ = nullptr;
    QueueHandle_t incomingQueue_ = nullptr;
    SemaphoreHandle_t stateMutex_ = nullptr;

    // Config (loaded once, protected by mutex for credential changes)
    char wifiSSID_[32] = DEFAULT_WIFI_SSID;
    char wifiPass_[64] = DEFAULT_WIFI_PASS;
    char mqttServer_[64] = DEFAULT_MQTT_SERVER;
    char mqttUser_[32] = DEFAULT_MQTT_USER;
    char mqttPass_[64] = DEFAULT_MQTT_PASS;
    uint16_t mqttPort_ = DEFAULT_MQTT_PORT;

    // Shared state (protected by stateMutex_)
    volatile bool wifiConnected_ = false;
    volatile bool mqttConnected_ = false;
    volatile bool wifiCredentialsChanged_ = false;
};
