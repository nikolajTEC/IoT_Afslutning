#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"

// Replace with your network credentials
const char* ssid = "PassW0rd";
const char* password = "28368557";

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;      // GMT offset (seconds)
const int   daylightOffset_sec = 3600; // Change this for Daylight Saving Time

// DS18B20 sensor setup
// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;    // Change this to your actual GPIO pin
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// File paths to save data
const char* jsonFilePath = "/temperature_log.json";
const char* csvFilePath = "/temperature_log.csv";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 3000;

// Time variables
struct tm timeinfo;
char timeStringBuff[50]; // Buffer for formatted time
unsigned long lastTimeUpdate = 0;
const unsigned long TimeUpdateInterval = 3600000;
// Init DS18B20
void initDS18B20() {
  sensors.begin();
  Serial.println("DS18B20 sensor initialized");
}

// Format time as a string
String getFormattedTime() {
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "Time not available";
  }
  
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// Save data to CSV file
void saveToCsvFile(float temperature, String timestamp) {
  // Append to file
  File file = LittleFS.open(csvFilePath, "a");
  
  if (!file) {
    // If file doesn't exist, create and add header
    file = LittleFS.open(csvFilePath, "w");
    if (file) {
      file.println("timestamp,temperature");
    } else {
      Serial.println("Failed to create CSV file");
      return;
    }
  }
  
  // Write data line
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

// Get Sensor Readings and return JSON object
String getSensorReadings() {
  sensors.requestTemperatures(); 
  float temperature = sensors.getTempCByIndex(0);
  String timestamp = getFormattedTime();
  
  // Check if reading was successful
  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Could not read temperature data");
    readings["temperature"] = "Error";
    readings["timestamp"] = timestamp;
  } else {
    readings["temperature"] = String(temperature);
    readings["timestamp"] = timestamp;
    
    // Save data to files
    saveToCsvFile(temperature, timestamp);
  }
  
  String jsonString = JSON.stringify(readings);
  return jsonString;
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

// Initialize Time
void initTime() {
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  
  Serial.println("Time initialized successfully");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    // Check if the message is "getReadings"
    if (message.equals("getReadings")) {
      // If it is, send current sensor readings
      String sensorReadings = getSensorReadings();
      Serial.println(sensorReadings);
      notifyClients(sensorReadings);
    }
  }
}

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
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  
  initDS18B20();
  initWiFi();
  initLittleFS();
  initTime();
  initWebSocket();
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  
  // Add routes for data files
  server.on("/data/json", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if file exists
    if (!LittleFS.exists(jsonFilePath)) {
      // Create an empty array if file doesn't exist
      File file = LittleFS.open(jsonFilePath, "w");
      if (file) {
        file.print("[]");
        file.close();
      }
    }
    request->send(LittleFS, jsonFilePath, "application/json");
  });
  
  server.on("/data/csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if file exists
    if (!LittleFS.exists(csvFilePath)) {
      // Create an empty file with header if doesn't exist
      File file = LittleFS.open(csvFilePath, "w");
      if (file) {
        file.println("timestamp,temperature");
        file.close();
      }
    }
    request->send(LittleFS, csvFilePath, "text/csv");
  });
  
  server.serveStatic("/", LittleFS, "/");
  
  // Start server
  server.begin();
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    String sensorReadings = getSensorReadings();
    Serial.println(sensorReadings);
    notifyClients(sensorReadings);
    lastTime = millis();
  }
  if (millis() - lastTimeUpdate >= TimeUpdateInterval) {
    lastTimeUpdate = millis();
    initTime();
  }
  ws.cleanupClients();
}