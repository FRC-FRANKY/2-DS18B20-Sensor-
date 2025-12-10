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

  // ====== Relay & LED Pins ======
  #define RELAY_PIN 26
  #define LED_PIN 5      

  // ====== Sensor Detection LED ======
  #define LED_SENSOR1 33

  // ====== WiFi Credentials ======
  ///char ssid [] = "BABIHOUSE";
  //char pass[] = "#BabiCPA080522";

  char ssid[] = "MONKEY D. LUFFY";
  char pass[] = "KINGOFTHEPIRATES";
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

  // Brew duration (milliseconds) configurable from Firebase
  const unsigned long DEFAULT_BREW_DURATION = 1UL * 60UL * 1000UL;  // default 1 minute
  unsigned long brewDuration = DEFAULT_BREW_DURATION;

  unsigned long previousBlinkMillis = 0;
  const unsigned long blinkInterval = 500;
  bool ledState = false;

  unsigned long lastBlink1 = 0;
  bool blinkState1 = false;
  const unsigned long fastBlinkInterval = 150; // Blink fast for sensor error

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
    currentTemp = sensors.getTempCByIndex(0);

    // Sensor LED logic
    if (currentTemp == -127.0) {
      if (millis() - lastBlink1 >= fastBlinkInterval) {
        lastBlink1 = millis();
        blinkState1 = !blinkState1;
        digitalWrite(LED_SENSOR1, blinkState1 ? HIGH : LOW);
      }
    } else {
      digitalWrite(LED_SENSOR1, HIGH);
      blinkState1 = true;
    }

    Serial.print("🌡 Water Sensor Temp: ");
    Serial.println(currentTemp);
    Blynk.virtualWrite(V0, currentTemp);

    if (Firebase.ready()) {
      Firebase.setFloat(fbdo, "/coffee/temperature", currentTemp);
    }

    // Brew Logic
    if (currentTemp < 30.0 && !isBrewing) {
      fetchBrewDuration(); // ensure we use the latest duration before starting
      Serial.println("⚡ Temp below 30°C — starting brew!");
      isBrewing = true;
      brewStartTime = millis();
      relayOn();
      Firebase.setString(fbdo, "/coffee/status", "brewing");
      Firebase.setBool(fbdo, "/coffee/command/brewNow", true);
    }

    if (currentTemp >= 30.0 && isBrewing && !relayState) {
      relayOn();
      Serial.println("🔥 Maintaining heat...");
    }
  }

  // ====== Brew Countdown ======
  void showBrewCountdown() {
    if (!isBrewing) return;

    unsigned long elapsed = millis() - brewStartTime;
    unsigned long remaining = (brewDuration > elapsed) ? (brewDuration - elapsed) : 0;

    // Correct countdown math
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
        fetchBrewDuration(); // pull latest duration before starting manual brew
        isBrewing = true;
        brewStartTime = millis();
        relayOn();
        Firebase.setString(fbdo, "/coffee/status", "brewing");
        Serial.println("☕ Manual brew started");
      } 
      else if (!brewNow && isBrewing) {
        isBrewing = false;
        relayOff();
        Firebase.setString(fbdo, "/coffee/status", "idle");
        Serial.println("🛑 Manual brew stopped");
      }
    }
  }

// Fetch brew duration (ms) from Firebase so the timer matches the app selection.
// If missing/invalid, we keep the last known value (or default on first boot) and log a warning.
  bool fetchBrewDuration() {
    const char* path = "/coffee/command/brewDurationMs";
    if (Firebase.getInt(fbdo, path)) {
      int value = fbdo.intData();
      if (value > 0 && value <= 900000) { // <= 15 minutes per rule
        brewDuration = (unsigned long)value;
        Serial.printf("⏱ Brew duration updated: %lu ms\n", brewDuration);
        return true;
      }
    } else {
      Serial.printf("⚠️ fetchBrewDuration failed: %s\n", fbdo.errorReason().c_str());
    }

  Serial.println("⚠️ Brew duration invalid/missing; keeping previous value");
  return false;
  }

  // ====== Setup ======
  void setup() {
    Serial.begin(115200);
    sensors.begin();

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LED_SENSOR1, OUTPUT);

    relayOff();
    digitalWrite(LED_PIN, LOW);
    digitalWrite(LED_SENSOR1, LOW);

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

    // Get the latest duration at startup
    fetchBrewDuration();

    if (Firebase.ready()) {
      Firebase.setString(fbdo, "/coffee/status", "idle");
    }

    timer.setInterval(3000L, sendTemperature);
    timer.setInterval(1000L, showBrewCountdown);
    timer.setInterval(5000L, fetchBrewDuration);

    Serial.println("✅ System Ready");
  }

  // ====== Loop ======
  void loop() {
    Blynk.run();
    timer.run();
    checkBrewCommand();
    handleStatusLED();
  }
