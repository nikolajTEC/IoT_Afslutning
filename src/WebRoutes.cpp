#include "WebRoutes.h"
#include <Arduino.h>

// Define static member variables
const char* WebRoutes::csvFilePath = "/temperature_log.csv";

WebRoutes::WebRoutes(AsyncWebServer& server, WiFiHandler& wifiHandler)
    : server(server), wifiHandler(wifiHandler) {
}

void WebRoutes::initialize() {
    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    
    // Add route to display network info
    server.on("/network", HTTP_GET, [this](AsyncWebServerRequest *request) {
        String networkInfo = "SSID: " + wifiHandler.getSSID() + "<br>";
        networkInfo += "IP Address: " + wifiHandler.getIP() + "<br>";
        request->send(200, "text/html", networkInfo);
    });
    
    // Add route to reset WiFi settings
    server.on("/resetwifi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "WiFi settings reset. Restart device to configure new network.");
        wifiHandler.resetSettings();
    });
    
    server.on("/delete-file", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Csv file deleted");
        RemoveCsvFile();
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
    
    // Serve static files from LittleFS
    server.serveStatic("/", LittleFS, "/");
}

void WebRoutes::RemoveCsvFile() {
    if (LittleFS.exists(csvFilePath)) {
        if (LittleFS.remove(csvFilePath)) {
            Serial.println("File successfully removed.");
        } else {
            Serial.println("Failed to remove the file.");
        }
    } else {
        Serial.println("File does not exist.");
    }
}