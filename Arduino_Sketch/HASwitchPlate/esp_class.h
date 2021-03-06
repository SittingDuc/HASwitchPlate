// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// esp_class.h : A class and support works to support low-level ESP8266 functions
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

#include "settings.h"
#include <Arduino.h>
#include <WiFiManager.h>

// so EspClass and espClass collide with existing classes. So we need something more unique
// let us prefix "our", because that is oh-so original.

class ourEspClass
{
private:
public:
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // constructor
  ourEspClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // destructor
  ~ourEspClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void begin();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void loop();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void reset();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void wiFiSetup();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void wiFiReconnect();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void setupOta();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void startOta(String espOtaUrl);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool updateCheck();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void motionSetup();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void motionUpdate();

  String getMacHex(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint8_t getMotionPin(void) { return _motionPin; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  const char *getWiFiConfigPass(void) { return _wifiConfigPass; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  const char *getWiFiConfigAP(void) { return _wifiConfigAP; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // Original source: https://arduino.stackexchange.com/a/1237
  String getSubtringField(String data, char separator, int index);

  ////////////////////////////////////////////////////////////////////////////////
  String printHex8(uint8_t *data, uint8_t length);

protected:
  bool _alive;
  const char *_wifiConfigPass         = WIFI_CONFIG_PASSWORD;   // First-time config WPA2 password
  const char *_wifiConfigAP           = WIFI_CONFIG_AP;         // First-time config SSID
  const uint32_t _motionLatchTimeout  = MOTION_LATCH_TIMEOUT;   // Latch time for motion sensor
  const uint32_t _motionBufferTimeout = MOTION_BUFFER_TIMEOUT;  // Latch time for motion sensor
  const uint32_t _connectTimeout      = CONNECTION_TIMEOUT;     // Timeout for WiFi and MQTT connection attempts in seconds
  const uint32_t _reConnectTimeout    = RECONNECT_TIMEOUT;      // Timeout for WiFi reconnection attempts in seconds
  const uint32_t _updateCheckInterval = UPDATE_CHECK_INTERVAL;  // Time in msec between update checks (12 hours)
  uint32_t _updateCheckTimer;                                   // Timer for update check
  uint8_t  _espMac[6];                                          // Byte array to store our MAC address
  uint8_t  _motionPin;                                          // GPIO input pin for motion sensor if connected and enabled
  bool     _motionActive;                                       // Motion is being detected

};
