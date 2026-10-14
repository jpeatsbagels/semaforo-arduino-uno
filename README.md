# Semáforo Arduino UNO + Página web en vivo

Muestra en la página el status del Arduino UNO conectado a este equipo por cable
USB: qué luz está activa (pin) y la cuenta regresiva para el cambio.

## Estructura

| Archivo | Qué es |
|---|---|
| `semaforo_arduino_uno.ino` | Firmware principal (Arduino UNO): secuencia verde 15 s → amarilla 5 s → roja 20 s. Rojo en pin 2, amarilla pin 4, verde pin 5. Reporta por Serial USB. Sin WiFi, sin librerías extra. |
| `semaforo_esp32.ino` | Respaldo (ESP32): misma secuencia y protocolo por Serial USB (+ WiFi opcional). |
| `server.py` | Servidor en este equipo: lee el Serial USB de la placa y sirve la página + API. |
| `index.html` / `styles.css` / `app.js` | Página web: semáforo, status, cuenta regresiva, pin activo. |

## 1. Arduino UNO (Arduino IDE)

1. Abre `semaforo_arduino_uno.ino`, selecciona placa **Arduino Uno** y su puerto COM.
2. Sube el código. No necesita librerías.
3. Abre el Monitor Serial a **115200**: verás líneas como
   `{"light":"green","pin":5,"remaining":14,...}` cada segundo.
4. Conecta: Rojo→pin 2, Amarillo→pin 4, Verde→pin 5 (cada LED con resistencia 220 Ω a GND).
5. Deja el UNO conectado por USB: ese mismo cable lleva la señal al servidor.

## 2. Servidor (este equipo)

Necesitas Python (https://www.python.org/downloads/) y una vez:

```powershell
pip install pyserial
python server.py --port COM3
```

- Cambia `COM3` por el puerto de tu UNO (ver en Administrador de dispositivos).
- Si omites `--port`, intenta autodetectar; si no hay placa, entra en **modo simulado**.
- `python server.py --sim` fuerza la simulación (para probar la web sin hardware).
- Abre **http://localhost:8000**.

## 3. Cómo se actualiza la página

```
Arduino UNO (pines 2/4/5) --Serial USB 115200--> server.py --GET /api/status--> página (poll 1 s + conteo fluido)
```

El firmware manda `light + pin + remaining` cada segundo y en cada cambio de
luz; el servidor guarda el último estado y la página lo muestra con cuenta
regresiva interpolada y barra de progreso.

## Respaldo ESP32

Si usas el ESP32 (`semaforo_esp32.ino`), funciona igual por USB sin cambiar el
servidor. Su opción WiFi (`#define USE_WIFI 1` + `SERVER_URL` con la IP de este
equipo) sigue aceptada en `POST /api/update` por compatibilidad.
