# ESP32 Pin Simulation

Este documento muestra solo las conexiones de señal del ESP32. No incluye VCC ni GND.

## Simulación Del ESP32

```text
                        ┌──────────────────────────────┐
                        │          ESP32 DEVKIT         │
                        │                              │
         OLED SDA  ────▶│ GPIO 21                GPIO 22│◀──── OLED SCL
     HC-SR04 TRIG  ────▶│ GPIO 5                 GPIO 35│◀──── HC-SR04 ECHO
                        │                              │
     Servo radar   ────▶│ GPIO 13                GPIO 16│◀──── Servo cámara
     Servo puerta   ────▶│ GPIO 17                GPIO 34│◀──── Sensor de luz
                        │                              │
        LED linterna ──▶│ GPIO 27                GPIO 25│◀──── Sensor IR evasión
           DHT22 DATA ─▶│ GPIO 14                GPIO 18│◀──── RC522 SCK
                        │                              │
         RC522 RST   ───▶│ GPIO 32                GPIO 19│◀──── RC522 MISO
      Botón táctil   ───▶│ GPIO 33                GPIO 23│◀──── RC522 MOSI
                        │                              │
      LED alarma 1   ───▶│ GPIO 2                 GPIO 26│◀──── RC522 SS / SDA
      LED alarma 2   ───▶│ GPIO 15                      │
      LED alarma 3   ───▶│ GPIO 4                       │
                        └──────────────────────────────┘
```

## Mapa De Pines

| Componente | Señal | GPIO |
|---|---|---:|
| OLED SSD1306 | SDA | 21 |
| OLED SSD1306 | SCL | 22 |
| HC-SR04 | TRIG | 5 |
| HC-SR04 | ECHO | 35 |
| Servo radar | Signal | 13 |
| Servo cámara | Signal | 16 |
| Servo puerta | Signal | 17 |
| Sensor de luz | DO | 34 |
| LED linterna | Control | 27 |
| Sensor IR evasión | OUT | 25 |
| DHT22 | DATA | 14 |
| RC522 | SCK | 18 |
| RC522 | MISO | 19 |
| RC522 | MOSI | 23 |
| RC522 | SS / SDA | 26 |
| RC522 | RST | 32 |
| Botón táctil | Touch | 33 |
| LED alarma 1 | Control | 2 |
| LED alarma 2 | Control | 15 |
| LED alarma 3 | Control | 4 |

## Notas Rápidas

- GPIO 34 y GPIO 35 son solo entrada.
- El RC522 usa SPI.
- El DHT22 usa un solo pin de datos.
- Los servos solo muestran su pin de señal en este esquema.