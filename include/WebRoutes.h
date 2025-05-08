#ifndef WEBROUTES_H
#define WEBROUTES_H

#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include "WiFiHandler.h"

class WebRoutes {
public:
    WebRoutes(AsyncWebServer& server, WiFiHandler& wifiHandler);
    void initialize();    
private:
    AsyncWebServer& server;
    WiFiHandler& wifiHandler;
    
    // Helper functions
    static void RemoveCsvFile();
    
    // File paths
    static const char* csvFilePath;
};

#endif // WEBROUTES_H