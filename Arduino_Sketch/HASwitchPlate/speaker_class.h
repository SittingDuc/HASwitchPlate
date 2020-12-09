// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// speaker_class.h : A class and support works to interact with a sound making device
//
// ----------------------------------------------------------------------------------------------------------------- //

// This file is only #included once, mmkay
#pragma once

#include "settings.h"
#include <Arduino.h>

class SpeakerClass {
private:
public:
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // constructor
  SpeakerClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // destructor
  ~SpeakerClass(void) { _alive = false; }

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void begin();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void loop();

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  void enable(bool newState=true);

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  inline bool getEnable() { return _beepEnabled; }

  void playSound(uint32_t onTime, uint32_t offTime, uint32_t count);

protected:
  bool     _alive;
  bool     _beepEnabled;                               // if true, enable the Speaker on BEEP_PIN
  uint32_t _beepOnTime = BEEP_DEFAULT_TIME;            // milliseconds of on-time for beep
  uint32_t _beepOffTime = BEEP_DEFAULT_TIME;           // milliseconds of off-time for beep
  bool     _beepState;                                 // beep currently engaged
  uint32_t _beepCounter;                               // Count the number of beeps
  uint8_t  _beepPin;                                   // define beep pin output
  uint32_t _beepTimer;                                 // will store last time beep was updated

};