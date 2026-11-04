#define BLYNK_TEMPLATE_ID   "your id"
#define BLYNK_TEMPLATE_NAME "Temperature"
#define BLYNK_AUTH_TOKEN    "your auth token"
#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// ------------ WIFI CREDENTIALS ------------
char ssid[] = "coldchain";
char pass[] = "12345678";
// ------------ GPIO CONFIGURATION ------------
#define DS18B20_PIN  4
#define GREEN_LED    5
#define YELLOW_LED   6
#define RED_LED      7
#define BUZZER_PIN   15
#define LCD_SDA      8
#define LCD_SCL      9

// ------------ TEMPERATURE LIMITS ------------

const float SAFE_MIN = 30.0;
const float SAFE_MAX = 33.5;
const float CRITICAL_TEMP = 35.0;

const unsigned long READ_INTERVAL = 1000UL;
const unsigned long CLOUD_INTERVAL = 2000UL;
const unsigned long RECONNECT_INTERVAL = 10000UL;

// ------------ VARIABLES ------------

unsigned long lastReadTime = 0;
unsigned long lastCloudTime = 0;
unsigned long lastReconnectTime = 0;

float temperatureC = 0.0;

String currentStatus = "STARTING";
String previousStatus = "";

bool greenState = false;
bool yellowState = false;
bool redState = false;
bool buzzerState = false;

// ------------ HARDWARE OBJECTS ------------

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------ OUTPUT CONTROL ------------

void setOutputs(bool green,
                bool yellow,
                bool red,
                bool buzzer) {

  greenState = green;
  yellowState = yellow;
  redState = red;
  buzzerState = buzzer;

  digitalWrite(GREEN_LED, green ? HIGH : LOW);
  digitalWrite(YELLOW_LED, yellow ? HIGH : LOW);
  digitalWrite(RED_LED, red ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, buzzer ? HIGH : LOW);
}

// ------------ LCD DISPLAY ------------

void displayLCD(float temp, String status) {

  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temp, 1);
  lcd.write((uint8_t)223);
  lcd.print("C       ");

  lcd.setCursor(0, 1);
  lcd.print("Status: ");
  lcd.print(status);

  int used = 8 + status.length();

  while (used < 16) {
    lcd.print(' ');
    used++;
  }
}

// ------------ BLYNK UPDATE ------------

void updateBlynk() {

  if (!Blynk.connected()) {
    return;
  }

  // V0: Temperature
  if (currentStatus == "SENSOR ERROR") {
    Blynk.virtualWrite(V0, -999);
  } else {
    Blynk.virtualWrite(V0, temperatureC);
  }

  // V1: Text status
  Blynk.virtualWrite(V1, currentStatus);

  // V2: Green LED state
  Blynk.virtualWrite(V2, greenState ? 1 : 0);

  // V3: Yellow LED state
  Blynk.virtualWrite(V3, yellowState ? 1 : 0);

  // V4: Red LED state
  Blynk.virtualWrite(V4, redState ? 1 : 0);
}

// ------------ WIFI / BLYNK RECONNECT ------------

void maintainConnection() {

  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {

    if (now - lastReconnectTime >= RECONNECT_INTERVAL) {

      lastReconnectTime = now;

      Serial.println("Reconnecting WiFi...");
      WiFi.reconnect();
    }

    return;
  }

  if (!Blynk.connected()) {

    if (now - lastReconnectTime >= RECONNECT_INTERVAL) {

      lastReconnectTime = now;

      Serial.println("Reconnecting Blynk...");
      Blynk.connect(500);
    }
  }
}

// ------------ SETUP ------------

void setup() {

  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  setOutputs(false, false, false, false);

  // Initialize LCD
  Wire.begin(LCD_SDA, LCD_SCL);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("COLD CHAIN");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // Initialize DS18B20
  sensors.begin();
  sensors.setResolution(12);

  // Start WiFi and Blynk
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect(1000);

  delay(1000);
  lcd.clear();

  Serial.println("Cold-Chain Monitoring Started");
}

// ------------ MAIN LOOP ------------

void loop() {

  // Keep Blynk connection active
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }

  maintainConnection();

  unsigned long now = millis();

  // Read temperature every second
  if (now - lastReadTime < READ_INTERVAL) {
    return;
  }

  lastReadTime = now;

  sensors.requestTemperatures();

  float temp = sensors.getTempCByIndex(0);

  // ------------ SENSOR ERROR ------------

  if (temp == DEVICE_DISCONNECTED_C ||
      temp < -55.0 ||
      temp > 125.0) {

    currentStatus = "SENSOR ERROR";

    setOutputs(false, false, true, true);

    lcd.setCursor(0, 0);
    lcd.print("SENSOR ERROR    ");

    lcd.setCursor(0, 1);
    lcd.print("Check DS18B20   ");

    Serial.println("ERROR: Check DS18B20 wiring");

    if (now - lastCloudTime >= CLOUD_INTERVAL) {
      lastCloudTime = now;
      updateBlynk();
    }

    previousStatus = currentStatus;

    return;
  }

  temperatureC = temp;

  // ------------ IMMEDIATE STATUS CHECK ------------

  if (temperatureC >= CRITICAL_TEMP) {

    currentStatus = "CRITICAL";

    setOutputs(false, false, true, true);
  }

  else if (temperatureC < SAFE_MIN ||
           temperatureC > SAFE_MAX) {

    currentStatus = "WARNING";

    setOutputs(false, true, false, true);
  }

  else {

    currentStatus = "SAFE";

    setOutputs(true, false, false, false);
  }

  // ------------ LCD UPDATE ------------

  displayLCD(temperatureC, currentStatus);

  // ------------ SERIAL MONITOR ------------

  Serial.print("Temperature: ");
  Serial.print(temperatureC, 2);
  Serial.print(" C | Status: ");
  Serial.println(currentStatus);

  // ------------ BLYNK CLOUD UPDATE ------------

  if (now - lastCloudTime >= CLOUD_INTERVAL) {

    lastCloudTime = now;

    updateBlynk();
  }

  previousStatus = currentStatus;
}
