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

#define BUTTON_PIN 35
#define DEBOUNCE_TIME 50

// Create WiFi handler
WiFiHandler wifiHandler;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create WebRoutes object
WebRoutes webRoutes(server, wifiHandler);

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
const char* csvFilePath = "/temperature_log.csv";

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

unsigned long lastDebounceTime = 0;
unsigned long lastPrintTime = 0;
int secondsHeld = 0;
bool buttonIsHeld = false;
int buttonState;            // Current stable button state
int lastButtonState;        // Last stable button state
int buttonDefaultState;     // The default state of the button (without pressing)

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  buttonDefaultState = digitalRead(BUTTON_PIN);
  lastButtonState = buttonDefaultState;
  buttonState = buttonDefaultState;

  Serial.print("Button default state detected as: ");
  Serial.println(buttonDefaultState == HIGH ? "HIGH" : "LOW");
  Serial.println("System will consider button pressed when state is DIFFERENT from default");
  Serial.println("Ready - waiting for button press");

  // Initialize components
  initDS18B20();
  initLittleFS();
  
  // Initialize WiFi with dedicated handler
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
  
  // Initialize and setup web routes
  
  // Start server
  server.begin();
}

void loop() {
  // Handle regular WiFi events (reconnection, etc.)
  wifiHandler.handleEvents();
  
  // Collect and send sensor data
  if ((millis() - lastTime) > timerDelay) {
    // Check if we have valid time before collecting and sending data
    if (getLocalTime(&timeinfo)) {
      String sensorReadings = getSensorReadings();
      Serial.println(sensorReadings);
      notifyClients(sensorReadings);
    } else {
      Serial.println("Skipping sensor reading - no valid timestamp available");
      // Try to reinitialize time
      initTime();
    }
    lastTime = millis();
  }
  
  // Update time occasionally
  if (millis() - lastTimeUpdate >= TimeUpdateInterval) {
    lastTimeUpdate = millis();
    initTime();
  }
  
  // Read the current raw button state
  int reading = digitalRead(BUTTON_PIN);

  // If the reading changed from the last reading, reset debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // If enough time has passed since the last change, consider the state stable
  if ((millis() - lastDebounceTime) > DEBOUNCE_TIME) {
    // Only update buttonState if it's different from the stable reading
    if (buttonState != reading) {
      buttonState = reading;

      // Button is pressed when state is DIFFERENT from default
      if (buttonState != buttonDefaultState && !buttonIsHeld) {
        lastPrintTime = millis();
        secondsHeld = 0;
        buttonIsHeld = true;
        Serial.println("Button Pressed");
      }

      // Button is released when state returns to default
      if (buttonState == buttonDefaultState && buttonIsHeld) {
        buttonIsHeld = false;
        Serial.println("Button Released");
      }
    }
  }

  // If button is held, count and print seconds
  if (buttonIsHeld) {
    if (millis() - lastPrintTime >= 1000) {
      secondsHeld++;
      if (secondsHeld == 10) {
        // Call RemoveCsvFile through WebRoutes class and reset WiFi settings
        // This needs to be added to WebRoutes or kept here
        LittleFS.remove(csvFilePath);
        wifiHandler.resetSettings();
        Serial.print("System reset");
      }
      else if (secondsHeld < 10){
        Serial.print("Button held for ");
        Serial.print(secondsHeld);
        Serial.println(" seconds");
      }
      lastPrintTime = millis();
    }
  }

  // Save the current reading for the next loop iteration
  lastButtonState = reading;

  ws.cleanupClients();
}