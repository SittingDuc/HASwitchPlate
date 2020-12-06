// Split settings.h file out
// so I can store secrets here
// -- Gerard, 2020

// This file is only #included once, mmkay
#pragma once

// OPTIONAL: Assign default values here.
char wifiSSID[32] = ""; // Leave unset for wireless autoconfig. Note that these values will be lost
char wifiPass[64] = ""; // when updating, but that's probably OK because they will be saved in EEPROM.

////////////////////////////////////////////////////////////////////////////////////////////////////
// These defaults may be overwritten with values saved by the web interface
char mqttServer[64] = "";
char mqttPort[6] = "1883";
char mqttUser[32] = "";
char mqttPassword[32] = "";
char haspNode[16] = "plate01";
char groupName[16] = "plates";
char configUser[32] = "admin";
char configPassword[32] = "";
char motionPinConfig[3] = "0";
////////////////////////////////////////////////////////////////////////////////////////////////////
