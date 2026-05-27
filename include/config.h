#pragma once

#include <Arduino.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// --- OLED Display (I2C) ---
#define PIN_OLED_SDA          21
#define PIN_OLED_SCL          22
#define OLED_WIDTH            128
#define OLED_HEIGHT           64
#define OLED_ADDRESS          0x3C

// --- Ultrasonic Sensor HC-SR04 ---
#define PIN_ULTRASONIC_TRIG   5    // Strapping pin, OK as brief OUTPUT pulse
#define PIN_ULTRASONIC_ECHO   35   // Input-only pin, perfecto para ECHO

// --- Servos ---
#define PIN_SERVO_RADAR       13   // Moved from GPIO4 (strapping pin) to GPIO13 (safe)
#define PIN_SERVO_CAMERA      16
#define PIN_SERVO_DOOR        17

// --- Light Sensor Module (3-pin digital) ---
#define PIN_LIGHT_SENSOR      34   // Digital output: HIGH=dark, LOW=light

// --- LED Flashlight ---
#define PIN_LED_FLASHLIGHT    27

// --- Avoidance Sensor IR (single, door/window) ---
#define PIN_AVOIDANCE         25   // Digital: LOW=obstacle detected

// --- DHT22 Temperature/Humidity ---
#define PIN_DHT22             14   // Moved from GPIO15 (strapping pin conflicts)

// --- RFID RC522 (VSPI - pines nativos) ---
#define PIN_RFID_SCK          18   // VSPI default SCK
#define PIN_RFID_MISO         19   // VSPI default MISO
#define PIN_RFID_MOSI         23   // VSPI default MOSI
#define PIN_RFID_SS           26   // Chip Select
#define PIN_RFID_RST          32   // Reset

// --- Touch Button (Capacitive) ---
#define PIN_TOUCH_BUTTON      33   // Touch8

// --- Alarm LEDs (3 LEDs per project spec) ---
#define PIN_ALARM_LED_1       2    // Onboard LED (safe)
#define PIN_ALARM_LED_2       15   // Strapping pin, OK as output after boot
#define PIN_ALARM_LED_3       4    // Freed from servo, now alarm LED

// ============================================================================
// SYSTEM DEFAULTS (all configurable via MQTT)
// ============================================================================

// --- Radar ---
#define DEFAULT_RADAR_MIN_ANGLE       0
#define DEFAULT_RADAR_MAX_ANGLE       90
#define DEFAULT_RADAR_STEP_DELAY_MS   50    // ms per degree step
#define DEFAULT_DETECTION_THRESHOLD   50    // cm
#define DEFAULT_ALERT_DISTANCE        30    // cm - triggers alert
#define DEFAULT_TRACKING_LOST_COUNT   10    // readings before losing target
#define RADAR_MAX_DISTANCE            400   // cm - HC-SR04 max range
#define RADAR_SCAN_MAP_SIZE           181   // 0-180 degrees max

// --- BLE ---
#define DEFAULT_BLE_SCAN_DURATION     2     // seconds
#define DEFAULT_BLE_SCAN_INTERVAL_MS  5000  // ms between scans
#define DEFAULT_BLE_RSSI_CLOSE        -55   // dBm - considered "close"
#define DEFAULT_BLE_RSSI_NEAR         -70   // dBm - considered "near"
#define MAX_BLE_DEVICES               10

// --- RFID ---
#define MAX_RFID_CARDS                20
#define DEFAULT_GUEST_TIMEOUT_S       1800  // 30 minutes
#define RFID_CHECK_INTERVAL_MS        200

// --- Climate ---
#define DEFAULT_CLIMATE_INTERVAL_MS   2000  // DHT22 minimum

// --- Light ---
#define DEFAULT_LIGHT_CHECK_MS        500
#define LED_PWM_CHANNEL               7
#define LED_PWM_FREQ                  5000
#define LED_PWM_RESOLUTION            8

// --- Touch ---
#define DEFAULT_TOUCH_THRESHOLD       40    // below = touched
#define TOUCH_DEBOUNCE_MS             500

// --- Door ---
#define DOOR_OPEN_ANGLE               90
#define DOOR_CLOSED_ANGLE             0
#define DEFAULT_DOOR_AUTO_CLOSE_MS    10000 // 10 seconds

// --- MQTT ---
#define DEFAULT_MQTT_SERVER           "192.168.0.110"
#define DEFAULT_MQTT_PORT             1883
#define DEFAULT_MQTT_USER             "esp32"
#define DEFAULT_MQTT_PASS             "esp32pass"
#define DEFAULT_MQTT_CLIENT_ID        "ESP32_Security"
#define MQTT_RECONNECT_INTERVAL_MS    5000
#define MQTT_PUBLISH_INTERVAL_MS      1000
#define MQTT_STATUS_INTERVAL_MS       5000

// --- WiFi ---
#define DEFAULT_WIFI_SSID             "NIKEFE 2"
#define DEFAULT_WIFI_PASS             "24331343Salazar2"
#define WIFI_CONNECT_TIMEOUT_MS       15000
#define WIFI_RETRY_INTERVAL_MS        10000

// --- Display ---
#define DISPLAY_UPDATE_INTERVAL_MS    100
#define DISPLAY_DIM_TIMEOUT_MS        30000 // dim after 30s inactivity

// --- Power Manager ---
#define SERVO_DETACH_DELAY_MS         500   // detach servo after idle

// ============================================================================
// DUAL-CORE FreeRTOS CONFIGURATION
// ============================================================================

// Core assignments
#define CORE_NETWORK          0     // WiFi, MQTT, BLE - Protocol CPU
#define CORE_APPLICATION      1     // Sensors, actuators, display - App CPU

// Task stack sizes (bytes)
#define STACK_SIZE_NETWORK    8192  // WiFi+MQTT+TLS needs large stack
#define STACK_SIZE_BLE        4096  // BLE scanning task

// Task priorities (higher = more priority, max ~24)
#define PRIORITY_NETWORK      2     // Network management
#define PRIORITY_BLE          1     // BLE scanning (lower than network)
#define PRIORITY_APP          1     // Arduino loop() default

// Queue sizes
#define MQTT_PUBLISH_QUEUE_SIZE    16   // Outbound messages from Core 1 -> Core 0
#define MQTT_INCOMING_QUEUE_SIZE   8    // Incoming commands from Core 0 -> Core 1

// ============================================================================
// MQTT TOPICS
// ============================================================================

// Publish (ESP32 -> Server)
#define TOPIC_RADAR_SCAN        "home/security/radar/scan"
#define TOPIC_RADAR_ALERT       "home/security/radar/alert"
#define TOPIC_RADAR_TRACKING    "home/security/radar/tracking"
#define TOPIC_CLIMATE_DATA      "home/security/climate/data"
#define TOPIC_LIGHT_STATUS      "home/security/light/status"
#define TOPIC_AVOIDANCE         "home/security/avoidance/triggered"
#define TOPIC_BLE_DEVICE        "home/security/ble/device"
#define TOPIC_RFID_ACCESS       "home/security/rfid/access"
#define TOPIC_DOOR_STATUS       "home/security/door/status"
#define TOPIC_CAMERA_CMD        "home/security/camera/command"
#define TOPIC_SYSTEM_STATUS     "home/security/system/status"
#define TOPIC_TOUCH_EVENT       "home/security/touch/event"
#define TOPIC_DISPLAY_STATUS    "home/security/system/display"

// Subscribe (Server -> ESP32)
#define TOPIC_CONFIG_RADAR      "home/security/config/radar"
#define TOPIC_CONFIG_LIGHT      "home/security/config/light"
#define TOPIC_CONFIG_DETECTION  "home/security/config/detection"
#define TOPIC_CONFIG_WIFI       "home/security/config/wifi"
#define TOPIC_CONFIG_BLE        "home/security/config/ble"
#define TOPIC_CONTROL_ARM       "home/security/control/arm"
#define TOPIC_CONTROL_CAMERA    "home/security/control/camera"
#define TOPIC_CONTROL_FLASHLIGHT "home/security/control/flashlight"
#define TOPIC_CONTROL_SERVO     "home/security/control/servo"
#define TOPIC_CONTROL_DOOR      "home/security/control/door"
#define TOPIC_BLE_REGISTER      "home/security/ble/register"
#define TOPIC_RFID_REGISTER     "home/security/rfid/register"
#define TOPIC_RFID_GUEST_TIME   "home/security/rfid/guest_time"

// ============================================================================
// DATA STRUCTURES
// ============================================================================

enum class RadarMode : uint8_t {
    PATROL,
    DETECTION,
    TRACKING,
    MANUAL,
    OFF
};

enum class SecurityState : uint8_t {
    ARMED_FULL,
    ARMED_PERIMETER,   // Only radar + avoidance
    DISARMED,
    EMERGENCY
};

enum class DisplayScreen : uint8_t {
    BOOT,
    SHUTDOWN,
    GENERAL_STATUS,
    PRESENCE_VIEW,
    ALARM_VIEW,
    RADAR_VIEW,
    TRACKING_VIEW,
    ALERT_MODE,
    BLE_STATUS,
    RFID_STATUS,
    CONFIG_VIEW
};

enum class RFIDPermission : uint8_t {
    OWNER = 0,
    GUEST = 1
};

struct BLEDeviceEntry {
    char mac[18];       // "AA:BB:CC:DD:EE:FF\0"
    char name[20];
    bool active;
    int rssi;
    unsigned long lastSeen;
};

struct RFIDCardEntry {
    char uid[16];       // hex UID string
    char name[20];
    RFIDPermission level;
    unsigned long expiryTime;   // 0 = no expiry (OWNER)
    bool active;
};

struct SystemConfig {
    // Radar
    int radarMinAngle;
    int radarMaxAngle;
    int detectionThreshold;     // cm
    int alertDistance;           // cm
    
    // Light
    bool flashlightAutoMode;
    
    // Door
    unsigned long doorAutoCloseMs;
    
    // BLE
    int bleRssiClose;
    int bleRssiNear;
    
    // Touch
    int touchThreshold;
};

// Global armed states per subsystem
struct ArmedStates {
    bool radarArmed;
    bool cameraArmed;
    bool avoidanceArmed;
    bool rfidArmed;
    bool bleArmed;
    
    bool isFullyArmed() const {
        return radarArmed && cameraArmed && avoidanceArmed && rfidArmed && bleArmed;
    }
    
    void armAll() {
        radarArmed = cameraArmed = avoidanceArmed = rfidArmed = bleArmed = true;
    }
    
    void disarmAll() {
        radarArmed = cameraArmed = avoidanceArmed = rfidArmed = bleArmed = false;
    }
};

// ============================================================================
// MQTT MESSAGE STRUCTURES (for FreeRTOS queues between cores)
// ============================================================================

// Outbound: Core 1 -> Core 0 (publish requests)
struct MqttOutMessage {
    char topic[80];
    char payload[512];
    bool retained;
};

// Inbound: Core 0 -> Core 1 (received commands)
struct MqttInMessage {
    char topic[80];
    char payload[512];
};
