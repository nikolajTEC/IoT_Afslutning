#include "WiFiHandler.h"

// HTML for the WiFi configuration page
const char* configPortalHTML = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32 WiFi Configuration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 20px;
      background-color: #f7f7f7;
    }
    .container {
      max-width: 400px;
      margin: 0 auto;
      background-color: #fff;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
    }
    h1 {
      color: #0066cc;
      font-size: 24px;
      margin-bottom: 20px;
    }
    label {
      display: block;
      margin-bottom: 8px;
      font-weight: bold;
    }
    input {
      width: 100%;
      padding: 10px;
      margin-bottom: 15px;
      border: 1px solid #ddd;
      border-radius: 4px;
      box-sizing: border-box;
    }
    button {
      background-color: #0066cc;
      color: white;
      border: none;
      padding: 12px 20px;
      border-radius: 4px;
      cursor: pointer;
      font-size: 16px;
      width: 100%;
    }
    button:hover {
      background-color: #0055aa;
    }
    .networks {
      margin-top: 20px;
      max-height: 200px;
      overflow-y: auto;
    }
    .network {
      padding: 8px;
      cursor: pointer;
      border-bottom: 1px solid #eee;
    }
    .network:hover {
      background-color: #f5f5f5;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Temperature Sensor</h1>
    <p>Please configure your WiFi connection:</p>
    <form action="/save-config" method="post">
      <label for="ssid">Network Name (SSID):</label>
      <input type="text" id="ssid" name="ssid" required>
      
      <label for="password">Password:</label>
      <input type="password" id="password" name="password">
      
      <button type="submit">Connect</button>
    </form>
    
    <div class="networks">
      <p>Available Networks:</p>
      <div id="network-list">
        <!-- Networks will be populated here -->
      </div>
    </div>
  </div>
  
  <script>
    // Function to populate networks (would be used if scanning was implemented)
    function populateNetworks(networks) {
      const list = document.getElementById('network-list');
      networks.forEach(network => {
        const div = document.createElement('div');
        div.className = 'network';
        div.textContent = network;
        div.onclick = function() {
          document.getElementById('ssid').value = network;
        };
        list.appendChild(div);
      });
    }
    
    // For demo, populate with placeholder networks
    populateNetworks(['Loading networks...']);
    
    // Actual network scanning would be implemented server-side
    fetch('/scan-networks')
      .then(response => response.json())
      .then(networks => {
        document.getElementById('network-list').innerHTML = '';
        populateNetworks(networks);
      })
      .catch(error => {
        console.error('Error scanning networks:', error);
      });
  </script>
</body>
</html>
)rawliteral";

const char* successHTML = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Configuration Successful</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 20px;
      background-color: #f7f7f7;
      text-align: center;
    }
    .container {
      max-width: 400px;
      margin: 0 auto;
      background-color: #fff;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
    }
    h1 {
      color: #0066cc;
    }
    .success {
      color: #00aa00;
      font-size: 18px;
      margin: 20px 0;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>WiFi Configuration Saved</h1>
    <p class="success">Your settings have been saved successfully!</p>
    <p>The device will now attempt to connect to your WiFi network.</p>
    <p>If connection is successful, this access point will close.</p>
    <p>Device will restart in 5 seconds...</p>
  </div>
</body>
</html>
)rawliteral";

WiFiHandler::WiFiHandler() {
  configServer = nullptr;
  apIP = IPAddress(192, 168, 4, 1);
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
    // Credentials found, try to connect
    Serial.println("WiFi credentials found, attempting to connect...");
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
      return true;
    } else {
      Serial.println("\nFailed to connect with saved credentials. Starting configuration portal...");
    }
  } else {
    Serial.println("No WiFi credentials found.");
  }
  
  // Start configuration portal
  startAP();
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
  
  // Root route - serve the configuration page
  configServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", configPortalHTML);
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
      saveWiFiConfig(newSSID, newPassword);
      
      // Send success response
      request->send(200, "text/html", successHTML);
      
      // Schedule a restart
      delay(5000);
      ESP.restart();
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
  
  // Start AP mode
  startAP();
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
    processDNS();
  }
  
  // Check connection and reconnect if needed
  if (WiFi.status() != WL_CONNECTED && !isConfigMode && ssid.length() > 0) {
    Serial.println("WiFi connection lost, attempting to reconnect...");
    WiFi.reconnect();
    
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < connectionTimeout) {
      Serial.print(".");
      delay(500);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to reconnect. Starting configuration portal...");
      startAP();
    }
  }
}

void WiFiHandler::processDNS() {
  // Process DNS requests when in AP mode (captive portal)
  dnsServer.processNextRequest();
}