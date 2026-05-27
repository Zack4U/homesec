# ESP32 IoT Home Security System

## Sistema de Seguridad Automatizada para el Hogar

Sistema integral de seguridad IoT basado en ESP32-WROOM-32U con arquitectura dual-core.
El Core 0 gestiona todas las comunicaciones de red (WiFi, MQTT, BLE) de forma asíncrona,
mientras que el Core 1 ejecuta todos los sensores, actuadores y la lógica de seguridad
sin interrupciones.

---

## Arquitectura Dual-Core

```
┌──────────────────────────────────┐  ┌──────────────────────────────────┐
│         CORE 0 (PRO CPU)         │  │        CORE 1 (APP CPU)          │
│    Tareas FreeRTOS de Red        │  │    Arduino loop() principal      │
│                                  │  │                                  │
│  • Conexión WiFi (auto-reconexión)│ │  • Radar: Patrulla/Detección/    │
│  • Cliente MQTT (connect/publish)│  │    Rastreo con filtro Kalman     │
│  • Escaneo BLE (dispositivos     │  │  • Servo cámara (seguimiento)    │
│    registrados)                  │  │  • Servo puerta (apertura auto)  │
│  • Cola de publicación           │  │  • Sensor ultrasónico HC-SR04    │
│    (mensajes de Core 1)          │  │  • Sensor de evasión IR          │
│  • Cola de comandos entrantes    │  │  • DHT22 temperatura/humedad     │
│    (mensajes del servidor)       │  │  • Sensor de luz + linterna LED  │
│                                  │  │  • Botón táctil capacitivo       │
│  No bloquea Core 1 jamás.        │  │  • Lector RFID RC522             │
│                                  │  │  • Pantalla OLED I2C             │
│                                  │  │  • 3 LEDs de alarma              │
│                                  │  │  • Controlador de seguridad      │
└──────────────────────────────────┘  └──────────────────────────────────┘
```

---

## Guía de Conexión de Pines

### Microcontrolador: ESP32-WROOM-32U (DevKit V1)

> **IMPORTANTE:** Los servomotores requieren alimentación externa de 5V.
> NO alimentar los servos directamente desde el pin de 3.3V del ESP32.
> Conectar el GND de la fuente externa al GND del ESP32.

---

### Tabla Completa de Asignación de Pines

| # | Componente | GPIO | Dirección | Función | Notas |
|---|-----------|------|-----------|---------|-------|
| 1 | OLED SDA | **21** | I2C Data | Bus I2C datos | Pin I2C nativo |
| 2 | OLED SCL | **22** | I2C Clock | Bus I2C reloj | Pin I2C nativo |
| 3 | Ultrasónico TRIG | **5** | OUTPUT | Disparo de pulso | Pin strapping, OK como salida |
| 4 | Ultrasónico ECHO | **35** | INPUT | Recepción de eco | Pin solo-entrada, ideal para eco |
| 5 | Servo Radar | **13** | PWM OUT | Control servo radar | Pin seguro para PWM |
| 6 | Servo Cámara | **16** | PWM OUT | Control servo cámara | Pin seguro |
| 7 | Servo Puerta | **17** | PWM OUT | Control servo puerta | Pin seguro |
| 8 | Sensor de Luz | **34** | INPUT | Lectura digital | Pin solo-entrada, lectura digital |
| 9 | LED Linterna | **27** | PWM OUT | Control brillo LED | Pin seguro, PWM LEDC |
| 10 | Sensor Evasión IR | **25** | INPUT | Detección obstáculos | LOW = obstáculo detectado |
| 11 | DHT22 | **14** | DATA | Temp. y humedad | Pin seguro, requiere pull-up 4.7kΩ |
| 12 | RFID SCK | **18** | SPI CLK | Reloj SPI | VSPI nativo |
| 13 | RFID MISO | **19** | SPI MISO | Datos SPI entrada | VSPI nativo |
| 14 | RFID MOSI | **23** | SPI MOSI | Datos SPI salida | VSPI nativo |
| 15 | RFID SS (SDA) | **26** | OUTPUT | Chip Select RFID | Pin seguro |
| 16 | RFID RST | **32** | OUTPUT | Reset RFID | Pin seguro |
| 17 | Botón Táctil | **33** | TOUCH | Entrada capacitiva | Touch8 nativo |
| 18 | LED Alarma 1 | **2** | OUTPUT | Indicador estado | LED integrado en placa |
| 19 | LED Alarma 2 | **15** | OUTPUT | Indicador alerta | Pin strapping, OK tras boot |
| 20 | LED Alarma 3 | **4** | OUTPUT | Indicador emergencia | Pin seguro |

**Total: 20 GPIOs utilizados** de los 25 disponibles en el ESP32 DevKit.

---

### Diagrama de Conexión por Componente

#### 1. Pantalla OLED SSD1306 (I2C - 128x64)

```
OLED          ESP32
────          ─────
VCC  ───────► 3.3V
GND  ───────► GND
SDA  ───────► GPIO 21
SCL  ───────► GPIO 22
```

Dirección I2C: `0x3C`

---

#### 2. Sensor Ultrasónico HC-SR04

```
HC-SR04       ESP32
───────       ─────
VCC  ───────► 5V (fuente externa o VIN)
GND  ───────► GND
TRIG ───────► GPIO 5
ECHO ─[R]──► GPIO 35

[R] = Divisor de voltaje recomendado:
      ECHO ──► R1 (1kΩ) ──► GPIO35
                           ├── R2 (2kΩ) ──► GND
```

> **Nota:** El HC-SR04 opera a 5V. GPIO35 tolera hasta 3.3V.
> Se recomienda un divisor de voltaje en ECHO, aunque muchos módulos
> ya entregan señales de 3.3V en la salida ECHO.

---

#### 3. Servomotores (×3)

```
Servo Radar         ESP32
───────────         ─────
Señal (naranja) ──► GPIO 13
VCC (rojo)     ──► 5V EXTERNO ⚡
GND (marrón)   ──► GND COMÚN

Servo Cámara        ESP32
────────────        ─────
Señal (naranja) ──► GPIO 16
VCC (rojo)     ──► 5V EXTERNO ⚡
GND (marrón)   ──► GND COMÚN

Servo Puerta        ESP32
────────────        ─────
Señal (naranja) ──► GPIO 17
VCC (rojo)     ──► 5V EXTERNO ⚡
GND (marrón)   ──► GND COMÚN
```

> **⚡ IMPORTANTE:** Los servos consumen hasta 500mA cada uno bajo carga.
> Usar una fuente de alimentación externa de 5V/2A mínimo para los 3 servos.
> Conectar TODOS los GND juntos (ESP32 + fuente + servos).

---

#### 4. Módulo Sensor de Luz (3 pines, digital)

```
Módulo Luz    ESP32
──────────    ─────
VCC  ───────► 3.3V
GND  ───────► GND
DO   ───────► GPIO 34
```

Salida digital: `HIGH` = oscuro, `LOW` = luz.
Ajustar sensibilidad con el potenciómetro del módulo.

---

#### 5. LED Linterna (Super Brillante)

```
LED           ESP32
───           ─────
Ánodo (+) ──[R]──► GPIO 27
Cátodo (-) ─────► GND

[R] = Resistencia de 220Ω - 330Ω
```

Control PWM para brillo variable (canal LEDC).

---

#### 6. Sensor de Evasión IR (Obstacle Avoidance)

```
IR Sensor     ESP32
─────────     ─────
VCC  ───────► 3.3V
GND  ───────► GND
OUT  ───────► GPIO 25
```

Salida: `LOW` = obstáculo detectado, `HIGH` = libre.

---

#### 7. Sensor DHT22 (Temperatura y Humedad)

```
DHT22         ESP32
─────         ─────
VCC  ───────► 3.3V
GND  ───────► GND
DATA ──┬────► GPIO 14
       └─[R]─► 3.3V

[R] = Resistencia pull-up de 4.7kΩ (o 10kΩ)
```

> **Nota:** El DHT22 requiere un mínimo de 2 segundos entre lecturas.

---

#### 8. Lector RFID RC522 (SPI)

```
RC522         ESP32
─────         ─────
3.3V ───────► 3.3V  ⚠️ (NO usar 5V)
GND  ───────► GND
RST  ───────► GPIO 32
SDA  ───────► GPIO 26  (Chip Select / SS)
MOSI ───────► GPIO 23  (VSPI MOSI)
MISO ───────► GPIO 19  (VSPI MISO)
SCK  ───────► GPIO 18  (VSPI SCK)
IRQ  ───────  (No conectado)
```

> **⚠️ IMPORTANTE:** El RC522 opera SOLO a 3.3V.
> Conectar a 5V puede dañar el módulo.

---

#### 9. Botón Táctil Capacitivo

```
Touch         ESP32
─────         ─────
GPIO 33 ─── Superficie conductiva (cable, pad metálico, lámina)
```

Usa el sensor capacitivo nativo Touch8 del ESP32.
No requiere componentes externos. Calibración automática al inicio.

---

#### 10. LEDs de Alarma (×3)

```
LED 1 (Estado)       ESP32
──────────────       ─────
Ánodo (+) ──[R]──► GPIO 2   (LED integrado en placa)
Cátodo (-) ─────► GND

LED 2 (Alerta)       ESP32
──────────────       ─────
Ánodo (+) ──[R]──► GPIO 15
Cátodo (-) ─────► GND

LED 3 (Emergencia)   ESP32
──────────────────   ─────
Ánodo (+) ──[R]──► GPIO 4
Cátodo (-) ─────► GND

[R] = Resistencia de 220Ω - 330Ω para cada LED
```

**Modos de los LEDs:**
| Modo | LED 1 (GPIO2) | LED 2 (GPIO15) | LED 3 (GPIO4) |
|------|---------------|----------------|---------------|
| Apagado | ○ | ○ | ○ |
| Armado | ● | ○ | ○ |
| Detectando | ◐ (lento) | ○ | ○ |
| Rastreando | ◐ (medio) | ◐ (medio) | ○ |
| Alarma | ◑ (rápido) | ◑ (rápido) | ◑ (rápido) |
| Emergencia | ◑ (alterno) | ◐ (alterno) | ◑ (alterno) |

● = encendido, ○ = apagado, ◐/◑ = parpadeo

---

### Diagrama Esquemático de Alimentación

```
                    ┌─────────────────┐
                    │   Fuente 5V/3A  │
                    │   (USB o DC)     │
                    └────┬───────┬────┘
                         │       │
                    ┌────┴──┐   │
                    │ ESP32 │   │
                    │ VIN   │   │
                    └───┬───┘   │
                        │       │
                   3.3V │     5V│
                   ┌────┘       │
                   │            ├──► Servo Radar (GPIO13)
    OLED ◄─────────┤            ├──► Servo Cámara (GPIO16)
    DHT22 ◄────────┤            ├──► Servo Puerta (GPIO17)
    RC522 ◄────────┤            └──► HC-SR04 VCC
    Sensor Luz ◄───┤
    Sensor IR ◄────┤
                   │
                   GND ──── TODOS los GND unidos ────
```

---

## Temas MQTT

### Publicación (ESP32 → Servidor)

| Tópico | Descripción | Datos |
|--------|-------------|-------|
| `home/security/radar/scan` | Datos de escaneo radar | `{angle, distance, mode}` |
| `home/security/radar/alert` | Alerta de proximidad | `{angle, distance, approaching}` |
| `home/security/radar/tracking` | Datos de rastreo activo | `{angle, distance, predicted_angle}` |
| `home/security/climate/data` | Temperatura y humedad | `{temperature, humidity}` |
| `home/security/light/status` | Estado del sensor de luz | `{dark, flashlight}` |
| `home/security/avoidance/triggered` | Cruce de umbral IR | `{triggered, location}` |
| `home/security/ble/device` | Dispositivo BLE detectado | `{mac, rssi, name, known}` |
| `home/security/rfid/access` | Acceso por tarjeta RFID | `{uid, granted, level, name}` |
| `home/security/door/status` | Estado de la puerta | `{open, reason}` |
| `home/security/camera/command` | Comando a cámara PC | `{action}` |
| `home/security/system/status` | Estado del sistema | `{armed, mode, uptime, freeHeap}` |
| `home/security/touch/event` | Evento botón táctil | `{emergency_toggle, armed}` |

### Suscripción (Servidor → ESP32)

| Tópico | Descripción | Datos |
|--------|-------------|-------|
| `home/security/config/radar` | Configurar radar | `{min_angle, max_angle, threshold}` |
| `home/security/config/light` | Configurar luz | `{auto}` |
| `home/security/config/detection` | Umbral detección | `{threshold, alert_distance}` |
| `home/security/config/wifi` | Cambiar WiFi | `{ssid, password}` |
| `home/security/control/arm` | Armar/desarmar | `{system, armed}` |
| `home/security/control/camera` | Control cámara | `{action, angle}` |
| `home/security/control/flashlight` | Control linterna | `{on}` |
| `home/security/control/servo` | Control manual servo | `{target, angle}` |
| `home/security/control/door` | Control puerta | `{open}` |
| `home/security/ble/register` | Registrar dispositivo BLE | `{action, mac, name}` |
| `home/security/rfid/register` | Registrar tarjeta RFID | `{action, uid, name, level}` |
| `home/security/rfid/guest_time` | Tiempo de invitado RFID | `{uid, expires_in}` |

---

## Configuración WiFi y MQTT

Editar en `include/config.h`:

```cpp
#define DEFAULT_WIFI_SSID    "TU_RED_WIFI"
#define DEFAULT_WIFI_PASS    "TU_CONTRASEÑA"
#define DEFAULT_MQTT_SERVER  "IP_DEL_SERVIDOR"
#define DEFAULT_MQTT_PORT    8883
#define DEFAULT_MQTT_USER    "esp32"
#define DEFAULT_MQTT_PASS    "esp32pass"
```

Las credenciales WiFi se pueden actualizar remotamente vía MQTT
al tópico `home/security/config/wifi`.

---

## Dependencias (PlatformIO)

```ini
lib_deps = 
    adafruit/Adafruit SSD1306@^2.5.7
    adafruit/Adafruit GFX Library@^1.11.5
    adafruit/DHT sensor library@^1.4.4
    adafruit/Adafruit Unified Sensor@^1.1.9
    madhephaestus/ESP32Servo@^1.1.1
    miguelbalboa/MFRC522@^1.4.10
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^6.21.3
```

---

## Compilación

```bash
# Compilar
platformio run

# Compilar y subir al ESP32
platformio run --target upload

# Monitor serial
platformio device monitor --baud 115200
```
