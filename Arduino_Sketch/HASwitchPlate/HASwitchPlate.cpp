////////////////////////////////////////////////////////////////////////////////////////////////////
//           _____ _____ _____ _____
//          |  |  |  _  |   __|  _  |
//          |     |     |__   |   __|
//          |__|__|__|__|_____|__|
//        Home Automation Switch Plate
// https://github.com/aderusha/HASwitchPlate
//
// Copyright (c) 2019 Allen Derusha allen@derusha.org
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this hardware,
// software, and associated documentation files (the "Product"), to deal in the Product without
// restriction, including without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Product, and to permit persons to whom the
// Product is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Product.
//
// THE PRODUCT IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE PRODUCT OR THE USE OR OTHER DEALINGS IN THE PRODUCT.
////////////////////////////////////////////////////////////////////////////////////////////////////

// set a flag before calling common.h to announce that all the class are local, not extern, here.
#define COMMON_IS_LOCAL

#include "common.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>


// Settings (initial values) now live in a separate file
// So we do not leave secrets here and upload to github accidentally

// TODO: Move some/all of these into the config class!
const char wifiConfigPass[9] = WIFI_CONFIG_PASSWORD; // First-time config WPA2 password
const char wifiConfigAP[14] = WIFI_CONFIG_AP;        // First-time config SSID
const uint32_t updateCheckInterval = UPDATE_CHECK_INTERVAL; // Time in msec between update checks (12 hours)
bool updateEspAvailable = false;                    // Flag for update check to report new ESP FW version
float updateEspAvailableVersion;                    // Float to hold the new ESP FW version number
bool updateLcdAvailable = false;                    // Flag for update check to report new LCD FW version
bool debugSerialD8Enabled = true;                   // Enable hardware serial debug output on pin D8
const uint32_t telnetInputMax = 128;                // Size of user input buffer for user telnet session
uint8_t motionPin = 0;                              // GPIO input pin for motion sensor if connected and enabled
bool motionActive = false;                          // Motion is being detected
const uint32_t motionLatchTimeout = MOTION_LATCH_TIMEOUT;          // Latch time for motion sensor
const uint32_t motionBufferTimeout = MOTION_BUFFER_TIMEOUT;        // Latch time for motion sensor
uint32_t updateLcdAvailableVersion;                                // Int to hold the new LCD FW version number
const uint32_t connectTimeout = CONNECTION_TIMEOUT;       // Timeout for WiFi and MQTT connection attempts in seconds
const uint32_t reConnectTimeout = RECONNECT_TIMEOUT;      // Timeout for WiFi reconnection attempts in seconds
uint8_t espMac[6];                                        // Byte array to store our MAC address
uint32_t updateCheckTimer = 0;                      // Timer for update check

WiFiClient wifiClient;                     // client for OTA?
WiFiServer telnetServer(23);               // Server listening for Telnet
WiFiClient telnetClient;
MDNSResponder::hMDNSService hMDNSService;  // Bonjour

// URL for auto-update "version.json"
const char UPDATE_URL[] = DEFAULT_URL_UPDATE;
// Default link to compiled Arduino firmware image
String espFirmwareUrl = DEFAULT_URL_ARDUINO_FW;
// Default link to compiled Nextion firmware images
String lcdFirmwareUrl = DEFAULT_URL_LCD_FW;



////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Class These
void motionSetup();
void motionUpdate();
void handleTelnetClient();


////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{ // System setup
  nextion.initResetPin();
  Serial.begin(115200);  // Serial - LCD RX (after swap), debug TX
  Serial1.begin(115200); // Serial1 - LCD TX, no RX
  Serial.swap();

  debug.begin();
  nextion.begin();

  debug.printLn(SYSTEM,String(F("SYSTEM: Starting HASwitchPlate v")) + String(config.getHaspVersion()));
  debug.printLn(SYSTEM,String(F("SYSTEM: Last reset reason: ")) + String(ESP.getResetInfo()));
  debug.printLn(SYSTEM,String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );
  debug.printLn(SYSTEM,String(F("SYSTEM: espCore: ")) + String(ESP.getCoreVersion()) );

  config.begin();
  esp.begin();

  debug.printLn(SYSTEM,String(F("SYSTEM: Heap Status: ")) + String(ESP.getFreeHeap()) + String(F(" ")) + String(ESP.getHeapFragmentation()) + String(F("%")) );

  if (config.getMDNSEnabled())
  { // Setup mDNS service discovery if enabled
    hMDNSService = MDNS.addService(config.getHaspNode(), "http", "tcp", 80);
    if (debug.getTelnetEnabled())
    {
      MDNS.addService(config.getHaspNode(), "telnet", "tcp", 23);
    }
    MDNS.addServiceTxt(hMDNSService, "app_name", "HASwitchPlate");
    MDNS.addServiceTxt(hMDNSService, "app_version", String(config.getHaspVersion()).c_str());
    MDNS.update();
  }

  web.begin();
  mqtt.begin();

  motionSetup(); // Setup motion sensor if configured

  beep.begin();

  if (debug.getTelnetEnabled())
  { // Setup telnet server for remote debug output
    telnetServer.setNoDelay(true);
    telnetServer.begin();
    debug.printLn(String(F("TELNET: debug server enabled at telnet:")) + WiFi.localIP().toString());
  }

  debug.printLn(SYSTEM,F("SYSTEM: System init complete."));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{ // Main execution loop

  nextion.loop();
  esp.loop();
  mqtt.loop();
  ArduinoOTA.handle();      // Arduino OTA loop
  web.loop();
  if (config.getMDNSEnabled())
  {
    MDNS.update();
  }

  if ((millis() - updateCheckTimer) >= updateCheckInterval)
  { // Run periodic update check
    updateCheckTimer = millis();
    if (esp.updateCheck())
    { // Send a status update if the update check worked
      mqtt.statusUpdate();
    }
  }

  if (config.getMotionEnabled())
  { // Check on our motion sensor
    motionUpdate();
  }

  if (debug.getTelnetEnabled())
  {
    handleTelnetClient(); // telnetClient loop
  }

  beep.loop();
}



////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

////////////////////////////////////////////////////////////////////////////////////////////////////
void motionSetup()
{
  if (strcmp(config.getMotionPin(), "D0") == 0)
  {
    config.setMotionEnabled(true);
    motionPin = D0;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(config.getMotionPin(), "D1") == 0)
  {
    config.setMotionEnabled(true);
    motionPin = D1;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(config.getMotionPin(), "D2") == 0)
  {
    config.setMotionEnabled(true);
    motionPin = D2;
    pinMode(motionPin, INPUT);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void motionUpdate()
{
  static uint32_t motionLatchTimer = 0;         // Timer for motion sensor latch
  static uint32_t motionBufferTimer = millis(); // Timer for motion sensor buffer
  static bool motionActiveBuffer = motionActive;
  bool motionRead = digitalRead(motionPin);

  if (motionRead != motionActiveBuffer)
  { // if we've changed state
    motionBufferTimer = millis();
    motionActiveBuffer = motionRead;
  }
  else if (millis() > (motionBufferTimer + motionBufferTimeout))
  {
    if ((motionActiveBuffer && !motionActive) && (millis() > (motionLatchTimer + motionLatchTimeout)))
    {
      motionLatchTimer = millis();
      mqtt.publishMotionTopic(String(F("ON")));
      motionActive = motionActiveBuffer;
      debug.printLn("MOTION: Active");
    }
    else if ((!motionActiveBuffer && motionActive) && (millis() > (motionLatchTimer + motionLatchTimeout)))
    {
      motionLatchTimer = millis();
      mqtt.publishMotionTopic(String(F("OFF")));

      motionActive = motionActiveBuffer;
      debug.printLn("MOTION: Inactive");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void handleTelnetClient()
{ // Basic telnet client handling code from: https://gist.github.com/tablatronix/4793677ca748f5f584c95ec4a2b10303
  static uint32_t telnetInputIndex = 0;
  if (telnetServer.hasClient())
  { // client is connected
    if (!telnetClient || !telnetClient.connected())
    {
      if (telnetClient)
      {
        telnetClient.stop(); // client disconnected
      }
      telnetClient = telnetServer.available(); // ready for new client
      telnetInputIndex = 0;                    // reset input buffer index
    }
    else
    {
      telnetServer.available().stop(); // have client, block new connections
    }
  }
  // Handle client input from telnet connection.
  if (telnetClient && telnetClient.connected() && telnetClient.available())
  { // client input processing
    static char telnetInputBuffer[telnetInputMax];

    if (telnetClient.available())
    {
      char telnetInputByte = telnetClient.read(); // Read client byte
      // debug.printLn(String("telnet in: 0x") + String(telnetInputByte, HEX));
      if (telnetInputByte == 5)
      { // If the telnet client sent a bunch of control commands on connection (which end in ENQUIRY/0x05), ignore them and restart the buffer
        telnetInputIndex = 0;
      }
      else if (telnetInputByte == 13)
      { // telnet line endings should be CRLF: https://tools.ietf.org/html/rfc5198#appendix-C
        // If we get a CR just ignore it
      }
      else if (telnetInputByte == 10)
      {                                          // We've caught a LF (DEC 10), send buffer contents to the Nextion
        telnetInputBuffer[telnetInputIndex] = 0; // null terminate our char array
        nextion.sendCmd(String(telnetInputBuffer));
        telnetInputIndex = 0;
      }
      else if (telnetInputIndex < telnetInputMax)
      { // If we have room left in our buffer add the current byte
        telnetInputBuffer[telnetInputIndex] = telnetInputByte;
        telnetInputIndex++;
      }
    }
  }
}
