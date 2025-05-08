#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Arduino_JSON.h>

class WiFiHandler {
public:
  WiFiHandler();
  ~WiFiHandler();
  
  // Initialize WiFi and try to connect
  bool begin();
  
  // Start the configuration portal in AP mode
  void startAP(const char* apName = "NikoCars");
  
  // Reset WiFi settings
  void resetSettings();
  
  // Get current IP address (local IP or AP IP)
  String getIP();
  
  // Check if connected to WiFi
  bool isConnected();
  
  // Get current SSID
  String getSSID();
  
  // Handle WiFi events - call this in loop()
  void handleEvents();

  // Save WiFi configuration to flash
  bool saveWiFiConfig(String ssid, String password);
  
private:
  AsyncWebServer* configServer;
  DNSServer dnsServer;
  
  IPAddress apIP;
  String ssid;
  String password;
  bool isConfigMode;
  
  const char* apPassword = "";  // Empty for open network
  const char* configFile = "/wifi_config.json";
  const unsigned long connectionTimeout = 10000;  // 10 seconds
  
  // Setup web server for configuration portal
  void setupConfigPortal();
  
  // Try to connect to WiFi with current credentials
  bool connectToWiFi();
  
  // Load HTML content from file
  String loadHTMLFromFile(const char* filename);  
  
  // Load WiFi configuration from flash
  bool loadWiFiConfig();
};

#endif