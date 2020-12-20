// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// web_class.h : A class and support works to interact with the local HTTP server
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

#include "settings.h"
#include <Arduino.h>

// Additional CSS style to match Hass theme
// too long to fit inside class Protected area?
static const char _haspStyle[] = "<style>button{background-color:#03A9F4;}body{width:60%;margin:auto;}input:invalid{border:1px solid red;}input[type=checkbox]{width:20px;}</style>";

class WebClass {
private:
public:
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // constructor
  WebClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // destructor
  ~WebClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void begin();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void loop();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  const char *getHaspStyle(void) { return _haspStyle; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  char *getUser(void) { return _configUser; }
  void setUser(const char *value) { strncpy(_configUser, value, 32); _configUser[31]='\0'; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  char *getPassword(void) { return _configPassword; }
  void setPassword(const char *value) { strncpy(_configPassword, value, 32); _configPassword[31]='\0'; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleNotFound();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleRoot();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleSaveConfig();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleResetConfig();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleResetBacklight();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleFirmware();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleEspFirmware();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleLcdUpload();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleLcdUpdateSuccess();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleLcdUpdateFailure();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleLcdDownload();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleTftFileSize();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleReboot();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint32_t getTftFileSize() { return this->_tftFileSize; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void telnetPrintLn(bool enabled, String message);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void telnetPrint(bool enabled, String message);

protected:
  bool _alive;
  char _configUser[32]; // these two might belong in WebClass
  char _configPassword[32]; // these two might belong in WebClass
  uint32_t _tftFileSize;

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool _authenticated(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _setupHTTP();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _setupMDNS();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _setupTelnet();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void _handleTelnetClient();

};
