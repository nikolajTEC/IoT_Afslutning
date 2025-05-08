#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"
#include <DNSServer.h>
#include "WiFiHandler.h"
#include "WebRoutes.h"

// Define GPIO pin for the button and debounce delay
#define BUTTON_PIN 35
#define DEBOUNCE_TIME 50 // in milliseconds

// Create WiFi handler object
WiFiHandler wifiHandler;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create WebRoutes handler object
WebRoutes webRoutes(server, wifiHandler);

// NTP server settings for time synchronization
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;      // GMT offset in seconds
const int   daylightOffset_sec = 3600; // Offset for daylight savings

// DS18B20 sensor setup
const int oneWireBus = 4; // GPIO pin connected to DS18B20
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Path to CSV file storing temperature logs
const char* csvFilePath = "/temperature_log.csv";

// WebSocket object for real-time communication
AsyncWebSocket ws("/ws");

// JSON variable to hold sensor readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 3000; // Delay between readings (ms), change to 300000 for 5 min

// Time handling variables
struct tm timeinfo;
char timeStringBuff[50];
unsigned long lastTimeUpdate = 0;
const unsigned long TimeUpdateInterval = 3600000; // 1 hour interval

// Initialize the DS18B20 sensor
void initDS18B20() {
  sensors.begin();
  Serial.println("DS18B20 sensor initialized");
}

// Format time to a readable string
String getFormattedTime() {
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "Time not available";
  }
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// Save temperature data to CSV file
void saveToCsvFile(float temperature, String timestamp) {
  File file = LittleFS.open(csvFilePath, "a");

  // If file can't be opened for appending, try creating it
  if (!file) {
    file = LittleFS.open(csvFilePath, "w");
    if (file) {
      file.println("timestamp,temperature");
    } else {
      Serial.println("Failed to create CSV file");
      return;
    }
  }

  // Write data line to file
  if (file) {
    file.print(timestamp);
    file.print(",");
    file.println(temperature);
    file.close();
    Serial.println("Data saved to CSV file");
  } else {
    Serial.println("Failed to open CSV file for writing");
  }
}

// Retrieve temperature reading and return as JSON
String getSensorReadings() {
  sensors.requestTemperatures(); 
  float temperature = sensors.getTempCByIndex(0);
  String timestamp = getFormattedTime();

  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Could not read temperature data");
    readings["temperature"] = "Error";
    readings["timestamp"] = timestamp;
  } else {
    readings["temperature"] = String(temperature);
    readings["timestamp"] = timestamp;
    saveToCsvFile(temperature, timestamp);
  }

  return JSON.stringify(readings);
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  }
}

// Initialize NTP time
void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  Serial.println("Time initialized successfully");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Notify all connected WebSocket clients with sensor data
void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

// Handle incoming WebSocket messages
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    if (message.equals("getReadings")) {
      String sensorReadings = getSensorReadings();
      Serial.println(sensorReadings);
      notifyClients(sensorReadings);
    }
  }
}

// WebSocket event handler
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

// Initialize WebSocket
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// Button debounce and long-press handling
unsigned long lastDebounceTime = 0;
unsigned long lastPrintTime = 0;
int secondsHeld = 0;
bool buttonIsHeld = false;
int buttonState;
int lastButtonState;
int buttonDefaultState;

void handleButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_TIME) {
    if (buttonState != reading) {
      buttonState = reading;

      if (buttonState != buttonDefaultState && !buttonIsHeld) {
        lastPrintTime = millis();
        secondsHeld = 0;
        buttonIsHeld = true;
        Serial.println("Button Pressed");
      }

      if (buttonState == buttonDefaultState && buttonIsHeld) {
        buttonIsHeld = false;
        Serial.println("Button Released");
      }
    }
  }

  if (buttonIsHeld) {
    if (millis() - lastPrintTime >= 1000) {
      secondsHeld++;
      if (secondsHeld == 10) {
        LittleFS.remove(csvFilePath);
        wifiHandler.resetSettings();
        Serial.println("System reset");
      } else {
        Serial.printf("Button held for %d seconds\n", secondsHeld);
      }
      lastPrintTime = millis();
    }
  }

  lastButtonState = reading;
}

// Handle sensor data logic, including timestamp verification
void handleSensorData() {
  if (getLocalTime(&timeinfo)) {
    String sensorReadings = getSensorReadings();
    Serial.println(sensorReadings);
    notifyClients(sensorReadings);
  } else {
    Serial.println("Skipping sensor reading - no valid timestamp available");
    initTime();
  }
}

// Main setup function
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  buttonDefaultState = digitalRead(BUTTON_PIN);
  lastButtonState = buttonDefaultState;
  buttonState = buttonDefaultState;

  initDS18B20();
  initLittleFS();

  if (wifiHandler.begin()) {
    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(wifiHandler.getIP());
  } else {
    Serial.println("Running in configuration mode");
  }

  webRoutes.initialize();
  initTime();
  initWebSocket();

  server.begin();
}

// Main loop function
void loop() {
  wifiHandler.handleEvents();

  if ((millis() - lastTime) > timerDelay) {
    handleSensorData();
    lastTime = millis();
  }

  if (millis() - lastTimeUpdate >= TimeUpdateInterval) {
    lastTimeUpdate = millis();
    initTime();
  }

  handleButton();

  ws.cleanupClients();
}