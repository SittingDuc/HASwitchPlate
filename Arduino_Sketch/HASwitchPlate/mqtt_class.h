// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// mqtt_class.h : A class and support works to interact with the distant MQTT Server
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

#include "settings.h"
#include <Arduino.h>


class MQTTClass {
private:
public:
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // constructor
  MQTTClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // destructor
  ~MQTTClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void begin();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void loop();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void connect();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void callback(String &strTopic, String &strPayload);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void statusUpdate();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  bool clientIsConnected();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  String clientReturnCode();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void publishMotionTopic(String msg);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void publishStatusTopic(String msg);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void publishStateTopic(String msg);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void publishButtonEvent(String page, String buttonID, String newState);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void publishButtonJSONEvent(String page, String buttonID, String newState);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void publishStatePage(String page);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void publishStateSubTopic(String subtopic, String newState);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  String getClientID(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  uint16_t getMaxPacketSize(void);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void goodbye();

protected:
  const uint32_t _statusUpdateInterval = MQTT_STATUS_UPDATE_INTERVAL;  // Time in msec between publishing MQTT status updates (5 minutes)
  const uint32_t _mqttConnectTimeout   = CONNECTION_TIMEOUT;           // Timeout for WiFi and MQTT connection attempts in seconds

  bool   _alive;                                   // Flag that data structures are initialised and functions can run without error
  String _clientId;                                // Auto-generated MQTT ClientID
  String _getSubtopic;                             // MQTT subtopic for incoming commands requesting .val
  String _getSubtopicJSON;                         // MQTT object buffer for JSON status when requesting .val
  String _stateTopic;                              // MQTT topic for outgoing panel interactions
  String _stateJSONTopic;                          // MQTT topic for outgoing panel interactions in JSON format
  String _commandTopic;                            // MQTT topic for incoming panel commands
  String _groupCommandTopic;                       // MQTT topic for incoming group panel commands
  String _statusTopic;                             // MQTT topic for publishing device connectivity state
  String _sensorTopic;                             // MQTT topic for publishing device information in JSON format
  String _lightCommandTopic;                       // MQTT topic for incoming panel backlight on/off commands
  String _beepCommandTopic;                        // MQTT topic for error beep
  String _lightStateTopic;                         // MQTT topic for outgoing panel backlight on/off state
  String _lightBrightCommandTopic;                 // MQTT topic for incoming panel backlight dimmer commands
  String _lightBrightStateTopic;                   // MQTT topic for outgoing panel backlight dimmer state
  String _motionStateTopic;                        // MQTT topic for outgoing motion sensor state
  uint32_t _statusUpdateTimer;                     // Timer for update check

};
