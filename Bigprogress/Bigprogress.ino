#define BLYNK_TEMPLATE_ID "TMPL6lja4bI6G"
#define BLYNK_TEMPLATE_NAME "DS18B20"
#define BLYNK_AUTH_TOKEN "4m7YhwacX7WGM1KJElOvJ6qvqmGlZkGv"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FirebaseESP32.h>

// ====== DS18B20 Sensor #1 ======
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ====== DS18B20 Sensor #2 ======
#define ONE_WIRE_BUS_2 18
OneWire oneWire2(ONE_WIRE_BUS_2);
DallasTemperature sensors2(&oneWire2);

// ====== Relay & LED Pins ======
#define RELAY_PIN 26
#define LED_PIN 5      

// ====== Sensor Detection LEDs ======
#define LED_SENSOR1 33
#define LED_SENSOR2 32

// ====== WiFi Credentials ======
//char ssid[] = "GlobeAtHome_259a8_2.4";
//char pass[] = "PXhsUbp4";

char ssid[] = "BABIHOUSE";
char pass[] = "#BabiCPA080522";

// ====== Firebase Config ======
#define FIREBASE_HOST "https://cuppa-c89f3-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "OVwysqwT0yrXKTUfdrL1BNf115wCvXjvv55PLkX4"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

BlynkTimer timer;

// ====== Global Variables ======
float currentTemp = 0.0;
bool relayState = false;
bool isBrewing = false;
unsigned long brewStartTime = 0;
const unsigned long brewDuration = 6UL * 60UL * 1000UL;
unsigned long previousBlinkMillis = 0;
const unsigned long blinkInterval = 500;
bool ledState = false;

// ====== NEW VARIABLES FOR SENSOR LED BLINKING ======
unsigned long lastBlink1 = 0;
unsigned long lastBlink2 = 0;
bool blinkState1 = false;
bool blinkState2 = false;

// ====== Relay Control ======
void relayOn() {
  digitalWrite(RELAY_PIN, LOW);
  relayState = true;
  Serial.println("🔌 Relay ON");
}

void relayOff() {
  digitalWrite(RELAY_PIN, HIGH);
  relayState = false;
  Serial.println("🔌 Relay OFF");
}

// ====== Temperature Reading & Control ======
void sendTemperature() {
  sensors.requestTemperatures();
  sensors2.requestTemperatures();

  currentTemp = sensors.getTempCByIndex(0);
  float temp2 = sensors2.getTempCByIndex(0);

  // ===== Sensor 1 Detection LED (Blink if error) =====
  if (!isnan(currentTemp)) {
    digitalWrite(LED_SENSOR1, HIGH);   // Sensor OK → LED ON
  } else {
    if (millis() - lastBlink1 >= 500) {
      lastBlink1 = millis();
      blinkState1 = !blinkState1;
      digitalWrite(LED_SENSOR1, blinkState1 ? HIGH : LOW);
    }
  }

  // ===== Sensor 2 Detection LED (Blink if error) =====
  if (!isnan(temp2)) {
    digitalWrite(LED_SENSOR2, HIGH);   // Sensor OK → LED ON
  } else {
    if (millis() - lastBlink2 >= 500) {
      lastBlink2 = millis();
      blinkState2 = !blinkState2;
      digitalWrite(LED_SENSOR2, blinkState2 ? HIGH : LOW);
    }
  }

  // ===== Print Temperatures =====
  Serial.print("🌡 Water Sensor  Temp: ");
  Serial.println(currentTemp);

  Serial.print("🌡 Boiler Plate Sensor Temp: ");
  Serial.println(temp2);

  // ===== Send to Blynk =====
  Blynk.virtualWrite(V0, currentTemp);
  Blynk.virtualWrite(V10, temp2);

  // ===== Send to Firebase =====
  if (Firebase.ready()) {
    Firebase.setFloat(fbdo, "/coffee/temperature", currentTemp);
    Firebase.setFloat(fbdo, "/coffee/temperature2", temp2);
  }

  // ===== Logic for Sensor #1 =====
  if (currentTemp < 30.0 && !isBrewing) {
    Serial.println("⚡ SENSOR 1: Temp below 30°C — starting brew!");
    isBrewing = true;
    brewStartTime = millis();
    relayOn();
    Firebase.setString(fbdo, "/coffee/status", "brewing");
    Firebase.setBool(fbdo, "/coffee/command/brewNow", true);
  }

  if (currentTemp >= 30.0 && isBrewing && !relayState) {
    relayOn();
    Serial.println("🔥 SENSOR 1: Maintaining heat...");
  }

  // ===== Logic for Sensor #2 =====
  if (temp2 < 30.0 && !isBrewing) {
    Serial.println("⚡ SENSOR 2: Temp below 30°C — starting brew!");
    isBrewing = true;
    brewStartTime = millis();
    relayOn();
    Firebase.setString(fbdo, "/coffee/status", "brewing");
    Firebase.setBool(fbdo, "/coffee/command/brewNow", true);
  }

  if (temp2 >= 30.0 && isBrewing && !relayState) {
    relayOn();
    Serial.println("🔥 Boiler Plate SENSOR : Maintaining heat...");
  }
}

// ====== Brew Countdown ======
void showBrewCountdown() {
  if (!isBrewing) return;

  unsigned long elapsed = millis() - brewStartTime;
  unsigned long remaining = (brewDuration > elapsed) ? (brewDuration - elapsed) : 0;

  unsigned int minutes = remaining / 60000;
  unsigned int seconds = (remaining % 60000) / 1000;

  Serial.printf("⏳ Brewing... %02u:%02u remaining\n", minutes, seconds);

  if (elapsed >= brewDuration) {
    isBrewing = false;
    relayOff();
    if (Firebase.ready()) {
      Firebase.setString(fbdo, "/coffee/status", "done");
      Firebase.setBool(fbdo, "/coffee/command/brewNow", false);
    }
    Serial.println("✅ Brewing complete — Relay OFF!");
  }
}

// ====== Blink LED While Brewing ======
void handleStatusLED() {
  if (isBrewing) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousBlinkMillis >= blinkInterval) {
      previousBlinkMillis = currentMillis;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

// ====== Manual Brew Control ======
void checkBrewCommand() {
  if (Firebase.getBool(fbdo, "/coffee/command/brewNow")) {
    bool brewNow = fbdo.boolData();

    if (brewNow && !isBrewing) {
      isBrewing = true;
      brewStartTime = millis();
      relayOn();
      Firebase.setString(fbdo, "/coffee/status", "brewing");
      Serial.println("☕ Manual brew started");
    } else if (!brewNow && isBrewing) {
      isBrewing = false;
      relayOff();
      Firebase.setString(fbdo, "/coffee/status", "idle");
      Serial.println("🛑 Manual brew stopped");
    }
  }
}

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  sensors.begin();
  sensors2.begin();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_SENSOR1, OUTPUT);
  pinMode(LED_SENSOR2, OUTPUT);

  relayOff();
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_SENSOR1, LOW);
  digitalWrite(LED_SENSOR2, LOW);

  Serial.println("🔌 Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready()) {
    Firebase.setString(fbdo, "/coffee/status", "idle");
  }

  timer.setInterval(3000L, sendTemperature);
  timer.setInterval(1000L, showBrewCountdown);

  Serial.println("✅ System Ready");
}

// ====== Loop ======
void loop() {
  Blynk.run();
  timer.run();
  checkBrewCommand();
  handleStatusLED();
}
