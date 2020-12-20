// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// debug_class.cpp : Class internals to support debug messages on the serial console
//
// ----------------------------------------------------------------------------------------------------------------- //


#include "common.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
void debugClass::printLn(String debugText)
{
  // Debug output line of text to our debug targets
  String debugTimeText = "[+" + String(float(millis()) / 1000, 3) + "s] " + debugText;
  Serial.println(debugTimeText);
  if (_serialEnabled)
  {
    SoftwareSerial debugSerial(-1, 1); // -1==nc for RX, 1==TX pin
    debugSerial.begin(115200);
    debugSerial.println(debugTimeText);
    debugSerial.flush();
  }
  web.telnetPrintLn(_telnetEnabled, debugTimeText);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void debugClass::print(String debugText)
{ // Debug output single character to our debug targets (DON'T USE THIS!)
  // Try to avoid using this function if at all possible.  When connected to telnet, printing each
  // character requires a full TCP round-trip + acknowledgement back and execution halts while this
  // happens.  Far better to put everything into a line and send it all out in one packet using
  // debugPrintln.
  Serial.print(debugText);
  if (_serialEnabled)
  {
    SoftwareSerial debugSerial(-1, 1); // -1==nc for RX, 1==TX pin
    debugSerial.begin(115200);
    debugSerial.print(debugText);
    debugSerial.flush();
  }
  web.telnetPrint(_telnetEnabled, debugText);
}
