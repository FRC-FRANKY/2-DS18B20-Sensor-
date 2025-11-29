#define BLYNK_TEMPLATE_ID "TMPL6lja4bI6G"
#define BLYNK_TEMPLATE_NAME "DS18B20"
#define BLYNK_AUTH_TOKEN "4m7YhwacX7WGM1KJElOvJ6qvqmGlZkGv"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FirebaseESP32.h>

// ====== DS18B20 Sensor #1 ======
#define ONE_WIRE_BUS 4 // D4 YELLOW WIRE can interchange but same color hahaha
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ====== DS18B20 Sensor #2 ======
#define ONE_WIRE_BUS_2 18 // D18 YELLOW WIRE can interchange but same color as well  hahaha
OneWire oneWire2(ONE_WIRE_BUS_2);
DallasTemperature sensors2(&oneWire2);

// ====== Relay & LED Pins ======
#define RELAY_PIN 26 //IN pin in relay black wire
#define LED_PIN 5      

/// RELAY PINS

//the vcc connect into 3.3V or 5V and the GND are in Ground of ESP32

// ====== Sensor Detection LEDs ======
#define LED_SENSOR1 33
#define LED_SENSOR2 32

// ====== WiFi Credentials ======
//char ssid[] = "GlobeAtHome_259a8_2.4";
//char pass[] = "PXhsUbp4";

char ssid[] = "MONKEY D. LUFFY";
char pass[] = "KINGOFTHEPIRATES";

// ====== Firebase Config ======
#define FIREBASE_HOST "cuppa-c89f3-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "OVwysqwT0yrXKTUfdrL1BNf115wCvXjvv55PLkX4"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

BlynkTimer timer;

// ====== Global Variables ======
float currentTemp = 0.0;
bool relayState = false;
bool isBrewing = false;
// >>> NEW: Flag to track sensor error state <<<
bool isSensorError = false; 
unsigned long brewStartTime = 0;
const unsigned long brewDuration = 6UL * 60UL * 1000UL;
unsigned long previousBlinkMillis = 0;
const unsigned long BLINK_INTERVAL_NORMAL = 500;
const unsigned long BLINK_INTERVAL_FAST = 50; // 50ms interval for fast blink
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

  // >>> Check for SENSOR ERROR (-127°C) and set flag <<<
  if (currentTemp == -127.0 || isnan(currentTemp) || temp2 == -127.0 || isnan(temp2)) {
    isSensorError = true;
    relayOff(); // Ensure relay is off during an error
    Serial.println("❌ CRITICAL ERROR: Sensor not detected (-127C) or reading error!");
  } else {
    isSensorError = false;
  }

  // ===== Sensor 1 Detection LED (Blink if error) =====
  if (!isnan(currentTemp) && currentTemp != -127.0) {
    digitalWrite(LED_SENSOR1, HIGH);   // Sensor OK → LED ON
  } else {
    if (millis() - lastBlink1 >= 150) {
      lastBlink1 = millis();
      blinkState1 = !blinkState1;
      digitalWrite(LED_SENSOR1, blinkState1 ? HIGH : LOW);
    }
  }

  // ===== Sensor 2 Detection LED (Blink if error) =====
  if (!isnan(temp2) && temp2 != -127.0) {
    digitalWrite(LED_SENSOR2, HIGH);   // Sensor OK → LED ON
  } else {
    if (millis() - lastBlink2 >= 150) {
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

  // ===== Logic for Sensor #1 (Only run if no error) =====
  if (!isSensorError) {
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
  }

  // ===== Logic for Sensor #2 (Only run if no error) =====
  if (!isSensorError) {
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

// ====== Blink LED While Brewing/Error ======
void handleStatusLED() {
  unsigned long currentMillis = millis();
  unsigned long interval = BLINK_INTERVAL_NORMAL; // Default to normal interval

  if (isSensorError) {
    interval = BLINK_INTERVAL_FAST; // Use fast interval for error
  } else if (isBrewing) {
    interval = BLINK_INTERVAL_NORMAL; // Use normal interval when brewing
  } else {
    digitalWrite(LED_PIN, LOW); // LED off when idle
    previousBlinkMillis = currentMillis; // Reset timer when idle
    return; // Exit function if idle
  }
  
  // Handle the blinking logic using the selected interval
  if (currentMillis - previousBlinkMillis >= interval) {
    previousBlinkMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}

// ====== Manual Brew Control ======
void checkBrewCommand() {
  // Check if we successfully fetched the data first
  if (Firebase.getBool(fbdo, "/coffee/command/brewNow")) {
    // >>> CORRECTED LINE: Use the dot operator (.) instead of arrow (->) <<<
    bool brewNow = fbdo.boolData(); 

    // Added check: Only allow brewing if there's no sensor error
    if (brewNow && !isBrewing && !isSensorError) { 
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
