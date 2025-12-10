// Firmware version
#define FIRMWARE_VERSION "v1.9"

// Reset button pin (D5 = GPIO14)
#define RESET_BUTTON_PIN D5

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

// SSD1306 128x64 I2C Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Config file path
#define CONFIG_FILE "/config.json"

// Create display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Blynk token (entered via WiFiManager portal)
char blynkToken[40] = "";

// Flag for saving config
bool shouldSaveConfig = false;

// Data received from server
double lastAmount = 0;
char lastDate[20] = "";

// Flag to disable button during WiFi setup
bool wifiSetupComplete = false;

// Forward declaration
void updateDisplay();

// Callback when WiFiManager enters config mode
void configModeCallback(WiFiManager *myWiFiManager)
{
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

  // WiFi info
  display.setCursor(0, 52);
  display.print("WiFi: ");
  display.print(WiFi.RSSI());
  display.println("dBm");

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
