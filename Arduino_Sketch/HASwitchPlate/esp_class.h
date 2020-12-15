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
  // Original source: https://arduino.stackexchange.com/a/1237
  String getSubtringField(String data, char separator, int index);

  ////////////////////////////////////////////////////////////////////////////////
  String printHex8(uint8_t *data, uint8_t length);

protected:
  bool _alive;

};
