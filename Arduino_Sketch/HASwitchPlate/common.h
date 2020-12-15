// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// common.h : includes and externs for all our classes
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

// if the flag "COMMON_IS_LOCAL" is defined, then these classes are Actual, not Extern
// note that if two files declare COMMON_IS_LOCAL, you will get linker errors about duplicated symbols.
#ifdef COMMON_IS_LOCAL
#define COMMON_EXTERN
#else
#define COMMON_EXTERN extern
#endif

#include "settings.h"
#include <Arduino.h>

#include "debug.h"
COMMON_EXTERN debugClass debug;  // our debug Object, USB Serial and/or Telnet

#include "esp_class.h"
COMMON_EXTERN ourEspClass esp;  // our ESP8266 Micro/SoC

#include "config_class.h"
COMMON_EXTERN ConfigClass config;  // our Configuration Container

#include "hmi_nextion.h"
COMMON_EXTERN hmiNextionClass nextion;  // our LCD Object

#include "mqtt_class.h"
COMMON_EXTERN MQTTClass mqtt;  // our MQTT Object

#include "web_class.h"
COMMON_EXTERN WebClass web;  // our HTTP Server Object

#include "speaker_class.h"
COMMON_EXTERN SpeakerClass beep;  // our Speaker Object
