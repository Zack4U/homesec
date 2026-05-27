# Getting Started

Esta guía deja el proyecto listo de punta a punta: broker MQTT, red, firmware del ESP32, variables de la web, compilación y servidor.

Para ver el esquema de conexiones del ESP32, consulta la [guía de pines](PIN_OUT.md).

## 1) Requisitos previos

### Hardware

- 1 x ESP32 DevKit compatible con `esp32dev`.
- Pantalla OLED SSD1306 I2C.
- Sensor ultrasónico HC-SR04.
- 3 servomotores: radar, cámara y puerta.
- Sensor de luz digital.
- LED para linterna.
- Sensor IR de evasión/proximidad.
- DHT22.
- Lector RFID RC522.
- Botón táctil capacitivo.
- 3 LEDs de alarma con resistencias.
- Fuente externa de 5V para servos y cableado con GND común.
- Red WiFi local estable.

### Software

- Windows con permisos de administrador para configurar servicios y firewall.
- PlatformIO para compilar y cargar el ESP32.
- Node.js y npm para la carpeta `src/web`.
- Mosquitto instalado en la red local.

## 2) Preparar Mosquitto para red local

El proyecto necesita que el broker MQTT acepte conexiones desde la LAN y no solo desde `127.0.0.1`.

### Configuración mínima recomendada

Edita el archivo `mosquitto.conf` del equipo donde corre el broker y deja una configuración equivalente a esta:

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

Si hay líneas antiguas que limiten el acceso solo a localhost, elimínalas o coméntalas. La idea es que el broker escuche en todas las interfaces de red de la máquina.

### Pasos

1. Instala Mosquitto.
2. Abre `mosquitto.conf` con permisos de administrador.
3. Asegura que exista un `listener` en el puerto `1883` escuchando en `0.0.0.0`.
4. Activa `allow_anonymous true` para pruebas iniciales.
5. Reinicia el servicio de Mosquitto.
6. Abre el puerto `1883/TCP` en el firewall de Windows o del sistema anfitrión.

### Verificación rápida

- Desde otro equipo de la LAN, prueba conectarte al broker usando la IP real del host, por ejemplo `mqtt://192.168.0.110:1883`.
- No uses `127.0.0.1` si la web, el ESP32 o el broker van a estar en máquinas distintas.

## 3) Definir la red del proyecto

Antes de compilar, decide estas direcciones:

- IP del equipo que corre Mosquitto.
- IP del ESP32 en la red local, idealmente reservada por DHCP en el router.

El firmware del proyecto usa por defecto un broker MQTT apuntando a `192.168.0.110:1883`, así que debes cambiar ese valor si tu broker está en otra IP.

## 4) Configurar la web

La carpeta de la web está en `src/web` y carga sus variables desde `src/web/.env`.

El archivo esperado es este:

```env
MQTT_BROKER_URL=mqtt://<IP_DEL_BROKER>:1883
MQTT_USER=esp32
MQTT_PASS=esp32pass
PORT=3001
```

### Qué cambiar

- `MQTT_BROKER_URL`: reemplázalo por la IP LAN real del broker.
- `MQTT_USER` y `MQTT_PASS`: si mantienes `allow_anonymous true`, pueden quedar como referencia o ajustarse a lo que use tu red.
- `PORT`: puerto del servidor Node.js para la web.

### Importante

Si la web y Mosquitto corren en la misma PC, igual usa la IP LAN de esa PC, no `localhost` ni `127.0.0.1`, para que el ESP32 y otros equipos puedan verla desde la red.

## 5) Preparar el firmware del ESP32

El firmware vive en la raíz del proyecto y se compila con PlatformIO.

### Antes de compilar

- Revisa el cableado con las asignaciones de pines de `include/config.h`.
- Verifica alimentación correcta de servos y módulos a 3.3V o 5V según corresponda.
- Asegura GND común entre ESP32, sensores y fuente externa.
- Comprueba que la red WiFi definida en el firmware sea la correcta.
- Verifica que el broker MQTT esté accesible desde la red local.

### Notas de hardware importantes

- El RC522 debe ir a 3.3V, no a 5V.
- El HC-SR04 suele trabajar a 5V y su salida ECHO debe adaptarse a 3.3V.
- Los servos no deben alimentarse desde el pin de 3.3V del ESP32.

## 6) Compilar y cargar el ESP32

El entorno de PlatformIO está definido para `esp32dev` en `platformio.ini`.

### Desde PlatformIO

1. Abre el proyecto raíz en VS Code.
2. Deja que PlatformIO indexe dependencias.
3. Selecciona la plataforma `esp32dev` si hace falta.
4. Ejecuta la compilación.
5. Conecta el ESP32 por USB.
6. Carga el firmware al ESP32.
7. Abre el monitor serie a `115200` para confirmar que arranca correctamente.

### Resultado esperado

- El ESP32 inicializa pantalla, sensores, BLE y MQTT.
- En el monitor serie deben aparecer mensajes de arranque y conexión.
- El dispositivo debe publicar y suscribirse a los tópicos `home/security/#`.

## 7) Montar y arrancar la web

La interfaz web y el servidor Node.js están en `src/web`.

### Instalar dependencias

Desde `src/web`:

```bash
npm install
```

### Ejecutar en desarrollo

```bash
npm run dev
```

### Ejecutar el servidor

```bash
npm start
```

El servidor usa `PORT=3001` por defecto y expone la API, Socket.IO y la integración MQTT.

## 8) Secuencia recomendada de arranque

1. Enciende o inicia el equipo que corre Mosquitto.
2. Verifica que Mosquitto escuche en la LAN y acepte conexiones anónimas.
3. Conecta el ESP32 a la red WiFi y carga el firmware.
4. Confirma en el monitor serie que el ESP32 llegó a MQTT.
5. Ajusta `src/web/.env` con la IP correcta del broker.
6. Instala dependencias y arranca la web.
7. Abre la interfaz en el navegador y valida el estado MQTT.

## 9) Comprobaciones finales

- El broker responde desde otros equipos de la red.
- El ESP32 publica telemetría en `home/security/#`.
- La web muestra estado MQTT conectado.
- Los comandos enviados desde la web llegan al ESP32.
- La base de datos y los eventos de la web se cargan sin errores.

## 10) Si algo falla

- Si la web no conecta, revisa la IP en `src/web/.env`.
- Si el ESP32 no publica, revisa WiFi, broker y credenciales MQTT.
- Si solo funciona en localhost, revisa `listener 1883 0.0.0.0` y el firewall.
- Si un sensor no responde, revisa voltaje, GND común y cableado de pines.
