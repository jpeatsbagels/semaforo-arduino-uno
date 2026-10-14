// Semáforo Arduino UNO — Secuencia tipo semáforo con reporte al servidor por USB (Serial)
// Rojo: pin 2, 20 s | Amarillo: pin 4, 5 s | Verde: pin 5, 15 s
// Secuencia: VERDE (15s) -> AMARILLO (5s) -> ROJO (20s) -> repetir
//
// El UNO manda por Serial USB (115200) una línea JSON cada segundo y en cada cambio de luz:
//   {"light":"green","pin":5,"remaining":14,"durations":{"green":15,"yellow":5,"red":20}}
// El servidor Python (server.py) lee ese Serial y actualiza la página web.
//
// Conexión: solo cable USB, sin WiFi. No requiere librerías extra.
// Nota: los pines 0 y 1 quedan libres porque los usa el Serial USB.
// Nota: al abrir el puerto desde el servidor el UNO se reinicia; el servidor
// toma el último estado recibido, así que se recupera solo en 1-2 s.

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

// remaining en segundos para la cuenta regresiva
long remainingSec() {
  unsigned long elapsed = (millis() - stateStartMs) / 1000;
  long rem = (long)lightDuration(currentLight) - (long)elapsed;
  return rem < 0 ? 0 : rem;
}

void sendStatus(bool force) {
  unsigned long now = millis();
  if (!force && (now - lastReportMs < 1000)) return;
  lastReportMs = now;

  // JSON por Serial USB para el servidor local
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
}

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  Serial.begin(115200);
  delay(500);

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
