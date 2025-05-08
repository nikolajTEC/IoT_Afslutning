# ESP32 Temperaturmåling

Dette projekt drejer sig om en ESP32-baseret temperaturmåler, som registrerer og viser data gennem en brugergrænseflade. Måledata vises i realtid via WebSocket-forbindelse.

# Funktionalitet 

- Temperaturmålmåling med DS18B20 sensor
- Kommunikation med websocket, med data i realtid
- Wifi konfiguration og lagring ved brug af LittleFS
- Brugergrænseflade som indeholder faner, der tilgås via burgermenu:
  * Graf over de 100 sidste punkter
     - Indeholder også knap til hentning af CSV fil
  * Nuværende IP adresse, SSID og reset af netværk
  * Log ind på nyt netværk
  * Sletning af CSV fil med historisk data  

# Strukturering af projekt
 - data
  * delete_csv.html // har html + css + js for at håndtere sletning af csv fil
  * index.html // bliver åbnet når man søger på ip adressen. indeholder graf og download csv fil
  * navbar.js // burger menu, bliver implementeret i alle sider
  * reset_wifi.html // har html + css + js for at håndtere nulstilling af wifi indstillinger + viser ip adresse og navn på netværk
  * script.js // indeholder logik for at generere graf, og håndtere websocket data
  * style.css // indeholder css for index side
  * wifi_config.html // har html + css + js for at håndtere skiftning af netværk
- dokumentation
  * Dokumentation.md // denne side
  * Opgave_sketch_bb.png // diagram fra Fritzing over det fysiske setup
- include
  * WebRoutes.h // kode for at initialize front end endpoints
  * WiFiHandler.h // kode for at håndtere oprettelse/ændring af wifi indstillinger
- lib
- src
  * main.cpp // kode for at starte programmet, kalder alle ting der skal initializes fra start. deruodver håndterer den også: knap der wiper oprettede littlefs filer, måling af temperatur, opdatering af tid til systemet.
  * WebRoutes.cpp // kode for at initialize front end endpoints
  * WiFiHandler.cpp // kode for at håndtere oprettelse/ændring af wifi indstillinger
- test

# Opsætning af projekt og krav
 - Software krav
   * Installation af Platformio
   - libraries
      * paulstoffregen/OneWire@^2.3.8
      * milesburton/DallasTemperature@^4.0.4
      * https://github.com/me-no-dev/AsyncTCP.git
      * https://github.com/me-no-dev/ESPAsyncWebServer.git
      * adafruit/Adafruit BME280 Library@^2.3.0
      * arduino-libraries/Arduino_JSON@^0.2.0
 - Hardware 
   * ESP Development Board
   * Pushbutton
   * DS18B20 temperature sensor
   * 4.7 ohm resistors
   * Breadboard
   * Wires
   * Micro usb cable

 ## Opsætning af fysisk setup
  - Se Opgave_sketch_bb.png i dokumentationsmappen

# Upload af kode til esp32 board
- Åben PlatformIO
- I Platform mappen, tryk "Build Filesystem Image"
- Tryk derefter på "Upload Filesystem Image"

# Start af program
- I PlatformIO, åbn mappen General
- Tryk på "Upload and Monitor"
- Programmet er nu startet
- Programmet aflæser nu data fra temperatur måleren, og gemmer i CSV fil, hvis den har adgang til NTP

# Forbindelse til WiFiet
- Åbn din internetadgange
- Vælg NikoCars
- Log på med dit WiFi
- Terminal viser nu, at du er forbundet, og hvad ip adressen er.
- Programmet henter aktuel tid


# Aflæs af graf
- Tjek at du er på samme netværk som enheden
- I din browser, skriv ip addressen på netværket
- Du vil komme ind på forsiden med grafen, som opdatere med nye temperaturer løbende

# Features på hjemmesiden fra Burger menuen
- Dashboard
  * Forsiden. Indeholder graf med målinger og knap til hentning af CSV fil
- WiFi Configuration
  * Giver mulighed for at enheden logger på et andet netværk
- Reset Wifi
  * Resetter WiFi'et på enheden
- Delete CSV
  * Sletter CSV filen med målinger, og resetter derfor grafen.





