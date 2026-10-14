// Semáforo ESP32 — Secuencia tipo semáforo con reporte a servidor web por Serial (USB) y WiFi (HTTP opcional)
// Rojo: pin 2, 20 s | Amarillo: pin 4, 5 s | Verde: pin 5, 15 s
// Secuencia: VERDE (15s) -> AMARILLO (5s) -> ROJO (20s) -> repetir
//
// La ESP32 manda por Serial (115200) una línea JSON cada segundo y en cada cambio de luz:
//   {"light":"green","pin":5,"remaining":14,"durations":{"green":15,"yellow":5,"red":20}}
// El servidor Python (server.py) lee ese Serial y actualiza la página web.
//
// WiFi opcional: pon USE_WIFI en 1 y configura WIFI_SSID / WIFI_PASS / SERVER_URL.
// Entonces además hace POST a http://TU_PC:8000/api/update con el mismo JSON.

#define USE_WIFI 0

#if USE_WIFI 
  #include <WiFi.h>
  #include <HTTPClient.h>
  const char* WIFI_SSID = "TU_WIFI";
  const char* WIFI_PASS = "TU_PASSWORD";
  const char* SERVER_URL = "http://192.168.1.100:8000/api/update"; // IP de este equipo
#endif

#include <Arduino.h>

// ---- Pines y duraciones (segundos) ----
const int RED_PIN    = 2;   // requerido
const int YELLOW_PIN = 4;   // libre elección
const int GREEN_PIN  = 5;   // libre elección

const unsigned long DUR_RED    = 20;
const unsigned long DUR_YELLOW = 5;
const unsigned long DUR_GREEN  = 15;

enum Light { GREEN, YELLOW, RED };
Light currentLight = GREEN;
unsigned long stateStartMs = 0;
unsigned long lastReportMs = 0;

const char* lightName(Light l) {
  switch (l) {
    case GREEN:  return "green";
    case YELLOW: return "yellow";
    case RED:    return "red";
  }
  return "unknown";
}

int lightPin(Light l) {
  switch (l) {
    case GREEN:  return GREEN_PIN;
    case YELLOW: return YELLOW_PIN;
    case RED:    return RED_PIN;
  }
  return -1;
}

unsigned long lightDuration(Light l) {
  switch (l) {
    case GREEN:  return DUR_GREEN;
    case YELLOW: return DUR_YELLOW;
    case RED:    return DUR_RED;
  }
  return 0;
}

void applyLights() {
  digitalWrite(RED_PIN,    currentLight == RED    ? HIGH : LOW);
  digitalWrite(YELLOW_PIN, currentLight == YELLOW ? HIGH : LOW);
  digitalWrite(GREEN_PIN,  currentLight == GREEN  ? HIGH : LOW);
}

Light nextLight(Light l) {
  switch (l) {
    case GREEN:  return YELLOW;
    case YELLOW: return RED;
    case RED:    return GREEN;
  }
  return GREEN;
}

#if USE_WIFI
void sendViaWiFi(); // declaración anticipada
#endif

// remaining en segundos (redondeo hacia arriba para cuenta regresiva)
long remainingSec() {
  unsigned long elapsed = (millis() - stateStartMs) / 1000;
  long rem = (long)lightDuration(currentLight) - (long)elapsed;
  return rem < 0 ? 0 : rem;
}

void sendStatus(bool force) {
  unsigned long now = millis();
  if (!force && (now - lastReportMs < 1000)) return;
  lastReportMs = now;

  // JSON por Serial para el servidor local (USB)
  Serial.print("{\"light\":\"");
  Serial.print(lightName(currentLight));
  Serial.print("\",\"pin\":");
  Serial.print(lightPin(currentLight));
  Serial.print(",\"remaining\":");
  Serial.print(remainingSec());
  Serial.print(",\"durations\":{\"green\":");
  Serial.print(DUR_GREEN);
  Serial.print(",\"yellow\":");
  Serial.print(DUR_YELLOW);
  Serial.print(",\"red\":");
  Serial.print(DUR_RED);
  Serial.println("}}");

#if USE_WIFI
  sendViaWiFi();
#endif
}

#if USE_WIFI
void sendViaWiFi() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"light\":\"") + lightName(currentLight) +
                String("\",\"pin\":") + lightPin(currentLight) +
                String(",\"remaining\":") + remainingSec() + String("}");
  http.POST(body);
  http.end();
}
#endif

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  Serial.begin(115200);
  delay(500);

#if USE_WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(250);
#endif

  currentLight = GREEN;
  stateStartMs = millis();
  applyLights();
  sendStatus(true);
}

void loop() {
  unsigned long elapsed = (millis() - stateStartMs) / 1000;

  if (elapsed >= lightDuration(currentLight)) {
    currentLight = nextLight(currentLight);
    stateStartMs = millis();
    applyLights();
    sendStatus(true);
  } else {
    sendStatus(false); // reporte 1 vez/seg con la cuenta regresiva
  }

  delay(50);
}
