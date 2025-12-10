// Firmware version
#define FIRMWARE_VERSION "v2.0"

// Reset button pin (D5 = GPIO14)
#define RESET_BUTTON_PIN D5

// RGB LED pins
#define LED_RED_PIN D6   // GPIO12
#define LED_GREEN_PIN D7 // GPIO13
#define LED_BLUE_PIN D0  // GPIO16 (D8/GPIO15 has boot issues)

// DHT11 sensor pin
#define DHT_PIN D3 // GPIO0
#define DHT_TYPE DHT11

// Blynk Template - get from Blynk Console > Templates
#define BLYNK_TEMPLATE_ID "TMPL2sDeyxtfV"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>

// SSD1306 128x64 I2C Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Config file path
#define CONFIG_FILE "/config.json"

// Create display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create DHT sensor object
DHT dht(DHT_PIN, DHT_TYPE);

// Blynk token (entered via WiFiManager portal)
char blynkToken[40] = "";

// Flag for saving config
bool shouldSaveConfig = false;

// Data received from server
double lastAmount = 0;
char lastDate[20] = "";

// Flag to disable button during WiFi setup
bool wifiSetupComplete = false;

// DHT sensor data
float lastTemperature = 0;
float lastHumidity = 0;
unsigned long lastDHTRead = 0;
#define DHT_READ_INTERVAL 5000 // Read every 5 seconds

// RGB LED state
bool ledCommonAnode = false; // Will be auto-detected
unsigned long lastLedUpdate = 0;
int ledPulseValue = 0;
int ledPulseDirection = 5;

// Forward declaration
void updateDisplay();
void updateLED();

// ============ RGB LED Functions ============

// Set RGB LED color (0-255 for each channel)
void setLedColor(int r, int g, int b)
{
  if (ledCommonAnode)
  {
    // Common anode: invert values (0 = full brightness)
    analogWrite(LED_RED_PIN, 255 - r);
    analogWrite(LED_GREEN_PIN, 255 - g);
    analogWrite(LED_BLUE_PIN, 255 - b);
  }
  else
  {
    // Common cathode: direct values
    analogWrite(LED_RED_PIN, r);
    analogWrite(LED_GREEN_PIN, g);
    analogWrite(LED_BLUE_PIN, b);
  }
}

// Turn off LED
void setLedOff()
{
  setLedColor(0, 0, 0);
}

// Auto-detect LED type by testing
void detectLedType()
{
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  // Start with common cathode assumption
  ledCommonAnode = false;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("LED Test");
  display.println("");
  display.println("Is LED glowing GREEN?");
  display.println("");
  display.println("Wait 3 seconds...");
  display.display();

  // Test: set green HIGH
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_BLUE_PIN, LOW);

  delay(3000);

  // If LED lit up, it's common cathode
  // If not, try common anode
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("LED Test");
  display.println("");
  display.println("Testing other type...");
  display.display();

  // Try common anode (invert)
  digitalWrite(LED_RED_PIN, HIGH);
  digitalWrite(LED_GREEN_PIN, LOW); // LOW = ON for common anode
  digitalWrite(LED_BLUE_PIN, HIGH);

  delay(2000);

  // We'll assume common cathode by default
  // User can check serial output if LED doesn't work right
  Serial.println("LED detection complete.");
  Serial.println("If LED was GREEN first: Common Cathode");
  Serial.println("If LED was GREEN second: Common Anode");
  Serial.println("Defaulting to Common Cathode. Edit ledCommonAnode if wrong.");

  setLedOff();
}

// Initialize LED pins
void initLED()
{
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  setLedOff();
}

// Update LED based on connection status (call in loop)
void updateLED()
{
  unsigned long now = millis();
  if (now - lastLedUpdate < 100)
    return;
  lastLedUpdate = now;

  if (!wifiSetupComplete)
  {
    // WiFi setup mode: blue steady
    setLedColor(0, 0, 50);
  }
  else if (Blynk.connected())
  {
    // Connected: green steady
    setLedColor(0, 50, 0);
  }
  else
  {
    // Disconnected: red steady
    setLedColor(80, 0, 0);
  }
}

// ============ DHT Sensor Functions ============

void readDHT()
{
  unsigned long now = millis();
  if (now - lastDHTRead < DHT_READ_INTERVAL)
    return;
  lastDHTRead = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Celsius

  if (!isnan(h) && !isnan(t))
  {
    lastHumidity = h;
    lastTemperature = t;
    Serial.print("DHT: ");
    Serial.print(t);
    Serial.print("°C, ");
    Serial.print(h);
    Serial.println("%");
  }
  else
  {
    Serial.println("DHT read failed");
  }
}

// Callback when WiFiManager enters config mode
void configModeCallback(WiFiManager *myWiFiManager)
{
  // Turn LED blue for setup mode
  setLedColor(0, 0, 50);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("WiFi Setup Mode");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 16);
  display.println("Connect to WiFi:");
  display.setTextSize(1);
  display.setCursor(0, 28);
  display.println("gigstack-Setup");
  display.setCursor(0, 42);
  display.println("Then open browser:");
  display.setCursor(0, 54);
  display.println("192.168.4.1");
  display.display();
}

// Callback to save config
void saveConfigCallback()
{
  shouldSaveConfig = true;
}

// Load config from filesystem
void loadConfig()
{
  if (LittleFS.exists(CONFIG_FILE))
  {
    File configFile = LittleFS.open(CONFIG_FILE, "r");
    if (configFile)
    {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, configFile);
      if (!error)
      {
        if (doc["blynkToken"].is<const char *>())
        {
          strlcpy(blynkToken, doc["blynkToken"], sizeof(blynkToken));
        }
      }
      configFile.close();
    }
  }
}

// Save config to filesystem
void saveConfig()
{
  JsonDocument doc;
  doc["blynkToken"] = blynkToken;

  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (configFile)
  {
    serializeJson(doc, configFile);
    configFile.close();
  }
}

void updateDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header with version and connection status
  display.setCursor(0, 0);
  display.print("gigstack ");
  display.print(FIRMWARE_VERSION);
  display.print(" ");
  display.println(Blynk.connected() ? "[OK]" : "[--]");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // Amount - full number with commas, dynamic text size
  unsigned long amount = (unsigned long)lastAmount;
  char formatted[24];

  if (amount >= 1000000000)
  {
    // Billions: X,XXX,XXX,XXX (13 chars + $)
    snprintf(formatted, sizeof(formatted), "$%lu,%03lu,%03lu,%03lu",
             amount / 1000000000,
             (amount / 1000000) % 1000,
             (amount / 1000) % 1000,
             amount % 1000);
  }
  else if (amount >= 1000000)
  {
    // Millions: XXX,XXX,XXX (11 chars + $)
    snprintf(formatted, sizeof(formatted), "$%lu,%03lu,%03lu",
             amount / 1000000,
             (amount / 1000) % 1000,
             amount % 1000);
  }
  else if (amount >= 1000)
  {
    // Thousands: XXX,XXX (7 chars + $)
    snprintf(formatted, sizeof(formatted), "$%lu,%03lu",
             amount / 1000,
             amount % 1000);
  }
  else
  {
    snprintf(formatted, sizeof(formatted), "$%lu", amount);
  }

  // Dynamic text size based on string length
  // Screen is 128px wide, each char at size 1 = 6px, size 2 = 12px, size 3 = 18px
  int len = strlen(formatted);
  int textSize;
  int yPos;

  if (len <= 7)
  {
    // $99,999 or less - size 3 (big and bold)
    textSize = 3;
    yPos = 16;
  }
  else if (len <= 10)
  {
    // $999,999,999 or less - size 2
    textSize = 2;
    yPos = 18;
  }
  else
  {
    // Billions - size 1
    textSize = 1;
    yPos = 20;
  }

  display.setTextSize(textSize);
  display.setCursor(0, yPos);
  display.println(formatted);

  // Date
  display.setTextSize(1);
  display.setCursor(0, 38);
  if (strlen(lastDate) > 0)
  {
    display.print("Date: ");
    display.println(lastDate);
  }

  // Bottom row: WiFi signal left, Temp/Humidity right
  display.setCursor(0, 56);
  display.print(WiFi.RSSI());
  display.print("dBm");

  // Temperature and humidity on right side
  if (lastTemperature != 0 || lastHumidity != 0)
  {
    char envStr[16];
    snprintf(envStr, sizeof(envStr), "%.0fC %.0f%%", lastTemperature, lastHumidity);
    int envWidth = strlen(envStr) * 6; // 6 pixels per char at size 1
    display.setCursor(128 - envWidth, 56);
    display.print(envStr);
  }

  display.display();
}

// V0 - Receive amount from server (can be string or number)
BLYNK_WRITE(V0)
{
  String raw = param.asStr();
  Serial.print("V0 raw value: ");
  Serial.println(raw);

  // Remove $ and commas to parse the number
  raw.replace("$", "");
  raw.replace(",", "");
  lastAmount = raw.toDouble();

  Serial.print("Parsed amount: $");
  Serial.println(lastAmount);
  updateDisplay();
}

// V1 - Receive date from server (max 16 chars)
BLYNK_WRITE(V1)
{
  String raw = param.asStr();
  Serial.print("V1 raw value: ");
  Serial.println(raw);
  strlcpy(lastDate, raw.c_str(), sizeof(lastDate));
  updateDisplay();
}

// Blynk connected callback
BLYNK_CONNECTED()
{
  Serial.println("Blynk connected!");
  // Sync values from server on connect
  Blynk.syncVirtual(V0, V1);
  updateDisplay();
}

BLYNK_DISCONNECTED()
{
  Serial.println("Blynk disconnected!");
  updateDisplay();
}

// Reset all settings
void resetSettings()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("RESETTING...");
  display.display();

  WiFiManager wifiManager;
  wifiManager.resetSettings();
  LittleFS.remove(CONFIG_FILE);

  delay(1000);
  ESP.restart();
}

// Button state tracking
unsigned long buttonPressStart = 0;
bool buttonHeld = false;

// Check button at boot (must hold 3 sec)
void checkResetButtonAtBoot()
{
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  delay(500); // Let pin stabilize

  // Check if button is pressed
  if (digitalRead(RESET_BUTTON_PIN) == HIGH)
  {
    Serial.println("Button not pressed at boot");
    return;
  }

  Serial.println("Button PRESSED at boot!");

  // Show countdown
  for (int i = 3; i > 0; i--)
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("RESET BUTTON HELD");
    display.setCursor(0, 20);
    display.print("Release to cancel");
    display.setCursor(0, 40);
    display.print("Reset in ");
    display.print(i);
    display.print("s...");
    display.display();

    delay(1000);

    if (digitalRead(RESET_BUTTON_PIN) == HIGH)
    {
      Serial.println("Cancelled");
      return;
    }
  }

  resetSettings();
}

// Check button during normal operation
void checkResetButton()
{
  // Don't check button until WiFi setup is complete
  if (!wifiSetupComplete)
    return;

  bool pressed = (digitalRead(RESET_BUTTON_PIN) == LOW);

  if (pressed && !buttonHeld)
  {
    // Button just pressed
    buttonPressStart = millis();
    buttonHeld = true;
  }
  else if (pressed && buttonHeld)
  {
    // Button still held - check duration
    unsigned long holdTime = millis() - buttonPressStart;

    if (holdTime >= 3000)
    {
      // Held for 3 seconds - reset!
      resetSettings();
    }
    else
    {
      // Show countdown on bottom of screen
      int secondsLeft = 3 - (holdTime / 1000);
      display.fillRect(0, 56, 128, 8, SSD1306_BLACK);
      display.setTextSize(1);
      display.setCursor(0, 56);
      display.print("Reset in ");
      display.print(secondsLeft);
      display.print("s...");
      display.display();
    }
  }
  else if (!pressed && buttonHeld)
  {
    // Button released - cancel
    buttonHeld = false;
    updateDisplay(); // Restore normal display
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("ESP8266 Started!");

  // Initialize I2C
  Wire.begin();

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("SSD1306 allocation failed!");
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("gigstack");
  display.setCursor(0, 16);
  display.println("Initializing...");
  display.display();

  // Initialize RGB LED
  initLED();

  // Quick RGB test - each color 1 second
  Serial.println("Testing RGB LED...");
  display.setCursor(0, 28);
  display.println("LED Test: RED");
  display.display();
  setLedColor(50, 0, 0);
  delay(1000);

  display.fillRect(0, 28, 128, 10, SSD1306_BLACK);
  display.setCursor(0, 28);
  display.println("LED Test: GREEN");
  display.display();
  setLedColor(0, 50, 0);
  delay(1000);

  display.fillRect(0, 28, 128, 10, SSD1306_BLACK);
  display.setCursor(0, 28);
  display.println("LED Test: BLUE");
  display.display();
  setLedColor(0, 0, 50);
  delay(1000);

  setLedOff();

  // Initialize DHT sensor
  dht.begin();
  Serial.println("DHT11 sensor initialized");

  // Check reset button at boot
  checkResetButtonAtBoot();

  // Initialize filesystem
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }

  // Load saved config
  loadConfig();
  Serial.print("Loaded Blynk Token: ");
  Serial.println(strlen(blynkToken) > 0 ? blynkToken : "(empty)");

  display.setCursor(0, 28);
  display.println("Connecting WiFi...");
  display.display();

  // Create WiFiManager instance
  WiFiManager wifiManager;

  // Set callbacks
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Custom parameter for Blynk Token
  WiFiManagerParameter customBlynkToken("blynk_token", "Blynk Token", blynkToken, 40);
  wifiManager.addParameter(&customBlynkToken);

  // Set timeout for config portal (5 minutes)
  wifiManager.setConfigPortalTimeout(300);

  // Try to connect, if fail start config portal
  if (!wifiManager.autoConnect("gigstack-Setup"))
  {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Failed!");
    display.println("Restarting...");
    display.display();
    delay(3000);
    ESP.restart();
  }

  // Get custom parameter value
  strlcpy(blynkToken, customBlynkToken.getValue(), sizeof(blynkToken));

  // Save config if needed
  if (shouldSaveConfig)
  {
    saveConfig();
    Serial.println("Config saved!");
  }

  // WiFi setup complete - enable button
  wifiSetupComplete = true;

  Serial.println("WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  display.setCursor(0, 40);
  display.println("Connecting Blynk...");
  display.display();

  // Connect to Blynk using saved token
  if (strlen(blynkToken) > 0)
  {
    Serial.print("Connecting to Blynk with token: ");
    Serial.println(blynkToken);
    Blynk.config(blynkToken);
    bool result = Blynk.connect(10000);
    Serial.print("Blynk connect result: ");
    Serial.println(result ? "SUCCESS" : "FAILED");
  }
  else
  {
    Serial.println("No Blynk token configured!");
    display.setCursor(0, 52);
    display.println("No token configured!");
    display.display();
    delay(3000);
  }

  updateDisplay();
}

void loop()
{
  // Check reset button
  checkResetButton();

  // Update RGB LED status
  updateLED();

  // Read DHT sensor periodically
  readDHT();

  // Run Blynk
  if (strlen(blynkToken) > 0)
  {
    Blynk.run();
  }

  // Update display periodically
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 5000)
  {
    lastUpdate = millis();
    updateDisplay();
  }
}
