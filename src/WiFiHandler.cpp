#include "WiFiHandler.h"
#include "WebRoutes.h"
WiFiHandler::WiFiHandler() {
  configServer = nullptr;
  apIP = IPAddress(192, 168, 4, 1);
  isConfigMode = false;
}

WiFiHandler::~WiFiHandler() {
  if (configServer) {
    delete configServer;
  }
}

bool WiFiHandler::begin() {
  // Initialize LittleFS if not already initialized
  if (!LittleFS.begin(true)) {
    Serial.println("An error occurred while mounting LittleFS");
    return false;
  }
  
  // Try to load saved credentials
  if (loadWiFiConfig()) {
    // Try to connect with loaded credentials
    if (connectToWiFi()) {
      return true;
    }
    Serial.println("Failed to connect with saved credentials. Starting configuration portal...");
  } else {
    Serial.println("No WiFi credentials found.");
  }
  
  // Start configuration portal
  startAP();
  return false;
}

bool WiFiHandler::connectToWiFi() {
  Serial.println("Attempting to connect to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long startAttemptTime = millis();
  
  // Wait for connection or timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < connectionTimeout) {
    Serial.print(".");
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    isConfigMode = false;
    return true;
  }
  
  return false;
}

void WiFiHandler::startAP(const char* apName) {
  Serial.println("Starting AP mode");
  isConfigMode = true;
  
  // Start the AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName, apPassword);
  
  Serial.print("AP started with IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Start DNS server for captive portal
  dnsServer.start(53, "*", apIP);
  
  // Setup the web server for configuration
  setupConfigPortal();
}

void WiFiHandler::setupConfigPortal() {   
  // Create server if it doesn't exist
  if (!configServer) {
    configServer = new AsyncWebServer(80);
  }
  
  // Load HTML from file
  String htmlContent = loadHTMLFromFile("/wifi_config.html");
  if (htmlContent.isEmpty()) {
    Serial.println("Failed to load HTML file, using default content");
    htmlContent = "WiFi Configuration Portal - Error loading content";
  }
  
  // Root route - serve the configuration page
  configServer->on("/", HTTP_GET, [htmlContent](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlContent);
  });
  
  // Handle form submission
  configServer->on("/save-config", HTTP_POST, [this](AsyncWebServerRequest *request) {
    String newSSID = "";
    String newPassword = "";
    
    if (request->hasParam("ssid", true)) {
      newSSID = request->getParam("ssid", true)->value();
    }
    
    if (request->hasParam("password", true)) {
      newPassword = request->getParam("password", true)->value();
    }
    
    if (newSSID.length() > 0) {
      // Save the new configuration
      if (saveWiFiConfig(newSSID, newPassword)) {        
        // Schedule a restart
        delay(3000);
        ESP.restart();
      } else {
        request->send(500, "text/plain", "Failed to save configuration");
      }
    } else {
      request->send(400, "text/plain", "SSID is required");
    }
  });

  // Handle captive portal for all other requests
  configServer->onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });
  
  // Start the server
  configServer->begin();
}

String WiFiHandler::loadHTMLFromFile(const char* filename) {
  if (!LittleFS.exists(filename)) {
    Serial.printf("File %s does not exist\n", filename);
    return "";
  }
  
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.printf("Failed to open file %s\n", filename);
    return "";
  }
  
  String content = file.readString();
  file.close();
  return content;
}

bool WiFiHandler::saveWiFiConfig(String newSSID, String newPassword) {
  JSONVar wifiConfig;
  
  wifiConfig["ssid"] = newSSID;
  wifiConfig["password"] = newPassword;
  
  String jsonString = JSON.stringify(wifiConfig);
  
  File configFile = LittleFS.open(this->configFile, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  
  configFile.print(jsonString);
  configFile.close();
  
  // Update local variables
  ssid = newSSID;
  password = newPassword;
  
  Serial.println("WiFi configuration saved successfully");
  return true;
}

bool WiFiHandler::loadWiFiConfig() {
  if (!LittleFS.exists(this->configFile)) {
    Serial.println("Config file does not exist");
    return false;
  }
  
  File configFile = LittleFS.open(this->configFile, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }
  
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    configFile.close();
    return false;
  }
  
  String jsonString = configFile.readString();
  configFile.close();
  
  JSONVar wifiConfig = JSON.parse(jsonString);
  
  if (JSON.typeof(wifiConfig) == "undefined") {
    Serial.println("Failed to parse config file");
    return false;
  }
  
  if (wifiConfig.hasOwnProperty("ssid") && wifiConfig.hasOwnProperty("password")) {
    ssid = (const char*)wifiConfig["ssid"];
    password = (const char*)wifiConfig["password"];
    return true;
  }

  return false;
}

void WiFiHandler::resetSettings() {
  // Remove the config file
  if (LittleFS.exists(this->configFile)) {
    LittleFS.remove(this->configFile);
    Serial.println("WiFi configuration reset");
  }
  
  ESP.restart();
}

String WiFiHandler::getIP() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  } else if (isConfigMode) {
    return WiFi.softAPIP().toString();
  }
  return "Not connected";
}

bool WiFiHandler::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String WiFiHandler::getSSID() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  }
  return "";
}

void WiFiHandler::handleEvents() {
  // Process DNS if in AP mode
  if (isConfigMode) {
    dnsServer.processNextRequest();
  }
  
  // Check connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED && !isConfigMode && ssid.length() > 0) {
    Serial.println("WiFi connection lost, attempting to reconnect...");
    if (!connectToWiFi()) {
      Serial.println("Failed to reconnect. Starting configuration portal...");
      startAP();
    }
  }
}

