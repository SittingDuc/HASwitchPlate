// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// speaker_class.cpp : Class internals to interact with a sound making device
//
// ----------------------------------------------------------------------------------------------------------------- //


#include "settings.h"
#include <Arduino.h>
#include "speaker_class.h"

#include "debug.h"
extern debugClass debug; // our serial debug interface

////////////////////////////////////////////////////////////////////////////////////////////////////
void SpeakerClass::begin()
{ // called in the main code setup, handles our initialisation
  _alive=true;
  _beepOnTime = BEEP_DEFAULT_TIME;
  _beepOffTime = BEEP_DEFAULT_TIME;
  _beepState = false;
  _beepCounter = 0;
  _beepTimer = 0;
  _beepEnabled = BEEP_ENABLED;
  if( _beepEnabled )
  {
    _beepPin = BEEP_PIN;
    pinMode(_beepPin, OUTPUT);
  }
  else
  {
    _beepPin = 0; // or -1?
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SpeakerClass::loop()
{ // called in the main code loop, handles our periodic code
  if (!_alive )
  {
    begin();
  }
  if ( _beepEnabled )
  { // Process Beeps
    if ((_beepState == true) && (millis() - _beepTimer >= _beepOnTime) && ((_beepCounter > 0)))
    {
      _beepState = false;         // Turn it off
      _beepTimer = millis(); // Remember the time
      analogWrite(_beepPin, 254); // start beep for beepOnTime
      if (_beepCounter > 0)
      { // Update the beep counter.
        _beepCounter--;
      }
    }
    else if ((_beepState == false) && (millis() - _beepTimer >= _beepOffTime) && ((_beepCounter >= 0)))
    {
      _beepState = true;          // turn it on
      _beepTimer = millis(); // Remember the time
      analogWrite(_beepPin, 0);   // stop beep for beepOffTime
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SpeakerClass::enable(bool newState)
{ // Enable or disable the speaker
  if( newState && !_beepState )
  {
    _beepPin = BEEP_PIN;
    pinMode(_beepPin, OUTPUT);
  }
  if( !newState && _beepState )
  {
    _beepPin = 0; // or -1?
    // we could flip the pin to input?
  }
  _beepEnabled=newState;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SpeakerClass::playSound(uint32_t onTime, uint32_t offTime, uint32_t count)
{ // play a given count of beeps with given onTime and given offTime
    if( !_beepEnabled || onTime == 0 || offTime == 0 || count == 0 )
    { // do nothing
        return;
    }

  _beepOnTime = onTime;
  _beepOffTime = offTime;
  _beepCounter = count;
  // do we need to set Timer now?
}

