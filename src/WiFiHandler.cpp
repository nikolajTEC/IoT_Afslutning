#include "WiFiHandler.h"
#include "WebRoutes.h"

// Constructor initializes values and sets default AP IP
WiFiHandler::WiFiHandler() {
  configServer = nullptr;
  apIP = IPAddress(192, 168, 4, 1);
  isConfigMode = false;
}

// Destructor cleans up the web server if allocated
WiFiHandler::~WiFiHandler() {
  if (configServer) {
    delete configServer;
  }
}

// Starts the WiFi handler logic
bool WiFiHandler::begin() {
  // Initialize the filesystem
  if (!LittleFS.begin(true)) {
    Serial.println("An error occurred while mounting LittleFS");
    return false;
  }
  
  // Try loading saved credentials and connect
  if (loadWiFiConfig()) {
    if (connectToWiFi()) {
      return true;
    }
    Serial.println("Failed to connect with saved credentials. Starting configuration portal...");
  } else {
    Serial.println("No WiFi credentials found.");
  }
  
  // Start fallback configuration AP mode
  startAP();
  return false;
}

// Attempt to connect using saved SSID/password
bool WiFiHandler::connectToWiFi() {
  Serial.println("Attempting to connect to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long startAttemptTime = millis();
  
  // Try connecting until timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < connectionTimeout) {
    Serial.print(".");
    delay(500);
  }
  
  // Check if connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    isConfigMode = false;
    return true;
  }
  
  return false;
}

// Start Access Point (AP) mode for configuration
void WiFiHandler::startAP(const char* apName) {
  Serial.println("Starting AP mode");
  isConfigMode = true;
  
  // Configure AP IP settings
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName, apPassword);
  
  Serial.print("AP started with IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Start DNS server for redirecting all domains
  dnsServer.start(53, "*", apIP);
  
  // Start web server for config portal
  setupConfigPortal();
}

// Set up web server and define routes for config portal
void WiFiHandler::setupConfigPortal() {   
  if (!configServer) {
    configServer = new AsyncWebServer(80);
  }
  
  // Load HTML content from filesystem
  String htmlContent = loadHTMLFromFile("/wifi_config.html");
  if (htmlContent.isEmpty()) {
    Serial.println("Failed to load HTML file, using default content");
    htmlContent = "WiFi Configuration Portal - Error loading content";
  }
  
  // Serve the configuration page at "/"
  configServer->on("/", HTTP_GET, [htmlContent](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlContent);
  });
  
  // Handle config form submission
  configServer->on("/save-config", HTTP_POST, [this](AsyncWebServerRequest *request) {
    String newSSID = "";
    String newPassword = "";
    
    // Get new SSID and password from form
    if (request->hasParam("ssid", true)) {
      newSSID = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
      newPassword = request->getParam("password", true)->value();
    }
    
    // Validate and save new credentials
    if (newSSID.length() > 0) {
      if (saveWiFiConfig(newSSID, newPassword)) {
        delay(3000);  // Wait before restarting
        ESP.restart();
      } else {
        request->send(500, "text/plain", "Failed to save configuration");
      }
    } else {
      request->send(400, "text/plain", "SSID is required");
    }
  });

  // Redirect all other URLs to root (captive portal)
  configServer->onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });
  
  // Start server
  configServer->begin();
}

// Load HTML content from filesystem
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

// Save new WiFi credentials to filesystem
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
  
  // Update internal variables
  ssid = newSSID;
  password = newPassword;
  
  Serial.println("WiFi configuration saved successfully");
  return true;
}

// Load WiFi credentials from file
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

// Erase saved WiFi credentials and restart
void WiFiHandler::resetSettings() {
  if (LittleFS.exists(this->configFile)) {
    LittleFS.remove(this->configFile);
    Serial.println("WiFi configuration reset");
  }
  
  ESP.restart();
}

// Get current IP address (STA or AP)
String WiFiHandler::getIP() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  } else if (isConfigMode) {
    return WiFi.softAPIP().toString();
  }
  return "Not connected";
}

// Check if connected to WiFi
bool WiFiHandler::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// Get current SSID (only if connected)
String WiFiHandler::getSSID() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  }
  return "";
}

// Handle DNS and WiFi reconnect logic
void WiFiHandler::handleEvents() {
  // Handle DNS for captive portal
  if (isConfigMode) {
    dnsServer.processNextRequest();
  }
  
  // Attempt reconnect if disconnected
  if (WiFi.status() != WL_CONNECTED && !isConfigMode && ssid.length() > 0) {
    Serial.println("WiFi connection lost, attempting to reconnect...");
    if (!connectToWiFi()) {
      Serial.println("Failed to reconnect. Starting configuration portal...");
      startAP();
    }
  }
}
