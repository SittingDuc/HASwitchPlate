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
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>


// Settings (initial values) now live in a separate file
// So we do not leave secrets here and upload to github accidentally

// TODO: Move some/all of these into the config class!

const uint32_t updateCheckInterval = UPDATE_CHECK_INTERVAL; // Time in msec between update checks (12 hours)
uint32_t updateCheckTimer = 0;                      // Timer for update check


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

  web.begin();
  mqtt.begin();

  beep.begin();


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

  if ((millis() - updateCheckTimer) >= updateCheckInterval)
  { // Run periodic update check
    updateCheckTimer = millis();
    if (esp.updateCheck())
    { // Send a status update if the update check worked
      mqtt.statusUpdate();
    }
  }


  beep.loop();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
