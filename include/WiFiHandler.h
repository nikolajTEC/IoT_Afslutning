#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>

class WiFiHandler {
  private:
    const char* configFile = "/wifi_config.json";
    String ssid = "";
    String password = "";
    bool isConfigMode = false;
    unsigned long connectionTimeout = 30000; // 30 seconds timeout
    
    // AP mode settings
    const char* apSSID = "ESP32-TempSensor";
    const char* apPassword = ""; // Empty for open network
    IPAddress apIP;
    DNSServer dnsServer;
    AsyncWebServer* configServer;
    
    // Save WiFi credentials to file
    bool saveWiFiConfig(String newSSID, String newPassword);
    
    // Load WiFi credentials from file
    bool loadWiFiConfig();
    
    // Handle captive portal requests
    void setupConfigPortal();
    
  public:
    WiFiHandler();
    ~WiFiHandler();
    
    // Initialize WiFi connection
    bool begin();
    
    // Reset saved WiFi settings and start AP mode
    void resetSettings();
    
    // Get current IP address as string
    String getIP();
    
    // Check if WiFi is connected
    bool isConnected();
    
    // Get SSID of connected network
    String getSSID();
    
    // Handle WiFi events - needs to be called in loop()
    void handleEvents();
    
    // Start access point with custom name
    void startAP(const char* apName = "ESP32-TempSensor");
    
    // Process DNS requests in AP mode - needs to be called in loop() when in AP mode
    void processDNS();
};

#endif