// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// config_class.h : A class and support works to support our configuration controls
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

#include "settings.h"
#include <Arduino.h>

// Settings (initial values) now live in a separate file
// So we do not leave secrets here and upload to github accidentally


class ConfigClass {
private:
public:
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // constructor
  ConfigClass(void) 
  { 
    _alive = false;
    setWIFISSID(DEFAULT_WIFI_SSID);
    setWIFIPass(DEFAULT_WIFI_PASS);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // These defaults may be overwritten with values saved by the web interface
    // Note that MQTT prefers dotted quad address, but MQTTS prefers fully qualified domain names (fqdn)
    // Note that MQTTS works best using NTP to obtain Time
    setMQTTServer(DEFAULT_MQTT_SERVER);
    setMQTTPort(DEFAULT_MQTT_PORT);
    setMQTTUser(DEFAULT_MQTT_USER);
    setMQTTPassword(DEFAULT_MQTT_PASS);
    setHaspNode(DEFAULT_HASP_NODE);
    setGroupName(DEFAULT_GROUP_NAME);
    setConfigUser(DEFAULT_CONFIG_USER);
    setConfigPassword(DEFAULT_CONFIG_PASS);
    setMotionPin(DEFAULT_MOTION_PIN);
  }


  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // destructor
  ~ConfigClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void begin();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void loop();

  // we are going to have quite a count of getters and setters
  // can we streamline them?

  char *getWIFISSID(void) { return _wifiSSID; }
  void setWIFISSID(const char *value) { strncpy(_wifiSSID, value, 32); _wifiSSID[31]='\0'; }

  char *getWIFIPass(void) { return _wifiPass; }
  void setWIFIPass(const char *value) { strncpy(_wifiPass, value, 64); _wifiPass[63]='\0'; }

  char *getMQTTServer(void) { return _mqttServer; }
  void setMQTTServer(const char *value) { strncpy(_mqttServer, value, 64); _mqttServer[63]='\0'; }

  char *getMQTTPort(void) { return _mqttPort; }
  void setMQTTPort(const char *value) { strncpy(_mqttPort, value, 6); _mqttPort[5]='\0'; }

  char *getMQTTUser(void) { return _mqttUser; }
  void setMQTTUser(const char *value) { strncpy(_mqttUser, value, 32); _mqttUser[31]='\0'; }

  char *getMQTTPassword(void) { return _mqttPassword; }
  void setMQTTPassword(const char *value) { strncpy(_mqttPassword, value, 32); _mqttPassword[31]='\0'; }
  
  char *getHaspNode(void) { return _haspNode; }
  void setHaspNode(const char *value) { strncpy(_haspNode, value, 16); _haspNode[15]='\0'; }

  char *getGroupName(void) { return _groupName; }
  void setGroupName(const char *value) { strncpy(_groupName, value, 16); _groupName[15]='\0'; }

  char *getConfigUser(void) { return _configUser; }
  void setConfigUser(const char *value) { strncpy(_configUser, value, 32); _configUser[31]='\0'; }

  char *getConfigPassword(void) { return _configPassword; }
  void setConfigPassword(const char *value) { strncpy(_configPassword, value, 32); _configPassword[31]='\0'; }

  char *getMotionPin(void) { return _motionPin; }
  void setMotionPin(const char *value) { strncpy(_motionPin, value, 3); _motionPin[2]='\0'; }

protected:
  bool     _alive;

// initial values would be useful?
    // "in class initialisation is only legal since C++11"
    char _wifiSSID[32]; // Leave unset for wireless autoconfig. Note that these values will be lost
    char _wifiPass[64]; // when updating, but that's probably OK because they will be saved in EEPROM.

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // These defaults may be overwritten with values saved by the web interface
    // Note that MQTT prefers dotted quad address, but MQTTS prefers fully qualified domain names (fqdn)
    // Note that MQTTS works best using NTP to obtain Time
    char _mqttServer[64];
    char _mqttPort[6];
    char _mqttUser[32];
    char _mqttPassword[32];
    char _haspNode[16];
    char _groupName[16];
    char _configUser[32]; // these two might belong in WebClass
    char _configPassword[32]; // these two might belong in WebClass
    char _motionPin[3];

};