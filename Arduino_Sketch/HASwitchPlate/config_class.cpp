// -*- C++ -*-
// HASwitchPlate Forked
//
// Inherits MIT license from HASwitchPlate.ino
// most Copyright (c) 2019 Allen Derusha allen@derusha.org
// little changes Copyright (C) 2020 Gerard Sharp (find me on GitHub)
//
//
// config_class.cpp : Class internals to support our configuration controls and JSON file on a SPIFFS Partition
//
// ----------------------------------------------------------------------------------------------------------------- //

// NB: after ESPCore 2.6.3, SPIFFS is deprecated in favour of LittleFS

#include "common.h"
#include <ArduinoJson.h>
#include <FS.h> // spiffs is deprecated after 2.6.3, littleFS is new
#include <EEPROM.h>
#include <WiFiManager.h>


////////////////////////////////////////////////////////////////////////////////////////////////////
void ConfigClass::begin()
{ // called in the main code setup, handles our initialisation
  _alive=true;
#if DISABLE_CONFIG_READ != (true)
  readFile();
#endif
}

void ConfigClass::loop()
{ // called in the main code loop, handles our periodic code

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConfigClass::readFile()
{ // Read saved config.json from SPIFFS
  debug.printLn(F("SPIFFS: mounting SPIFFS"));
  if (SPIFFS.begin())
  {
    if (SPIFFS.exists("/config.json"))
    { // File exists, reading and loading
      debug.printLn(F("SPIFFS: reading /config.json"));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        size_t configFileSize = configFile.size(); // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize]);
        configFile.readBytes(buf.get(), configFileSize);

        DynamicJsonDocument configJson(1024);
        DeserializationError jsonError = deserializeJson(configJson, buf.get());
        if (jsonError)
        { // Couldn't parse the saved config
          debug.printLn(String(F("SPIFFS: [ERROR] Failed to parse /config.json: ")) + String(jsonError.c_str()));
        }
        else
        {
          if (!configJson["mqttServer"].isNull())
          {
            setMQTTServer(configJson["mqttServer"]);
          }
          if (!configJson["mqttPort"].isNull())
          {
            setMQTTPort(configJson["mqttPort"]);
          }
          if (!configJson["mqttUser"].isNull())
          {
            setMQTTUser(configJson["mqttUser"]);
          }
          if (!configJson["mqttPassword"].isNull())
          {
            setMQTTPassword(configJson["mqttPassword"]);
          }
          if (!configJson["haspNode"].isNull())
          {
            setHaspNode(configJson["haspNode"]);
          }
          if (!configJson["groupName"].isNull())
          {
            setGroupName(configJson["groupName"]);
          }
          if (!configJson["configUser"].isNull())
          {
            web.setUser(configJson["configUser"]);
          }
          if (!configJson["configPassword"].isNull())
          {
            web.setPassword(configJson["configPassword"]);
          }
          if (!configJson["motionPinConfig"].isNull())
          {
            setMotionPin(configJson["motionPinConfig"]);
          }
          if (!configJson["debugSerialEnabled"].isNull())
          {
            debug.enableSerial(configJson["debugSerialEnabled"]); // debug, config, or both?
          }
          if (!configJson["debugTelnetEnabled"].isNull())
          {
            debug.enableTelnet(configJson["debugTelnetEnabled"]); // debug, config, or both?
          }
          if (!configJson["mdnsEnabled"].isNull())
          {
            setMDSNEnabled(configJson["mdnsEnabled"]);
          }
          if (!configJson["beepEnabled"].isNull())
          {
            beep.enable(configJson["beepEnabled"]);
          }
          String configJsonStr;
          serializeJson(configJson, configJsonStr);
          debug.printLn(String(F("SPIFFS: parsed json:")) + configJsonStr);
        }
      }
      else
      {
        debug.printLn(F("SPIFFS: [ERROR] Failed to read /config.json"));
      }
    }
    else
    {
      debug.printLn(F("SPIFFS: [WARNING] /config.json not found, will be created on first config save"));
    }
  }
  else
  {
    debug.printLn(F("SPIFFS: [ERROR] Failed to mount FS"));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConfigClass::saveCallback()
{ // Callback notifying us of the need to save config
  debug.printLn(F("SPIFFS: Configuration changed, flagging for save"));
  _shouldSaveConfig = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConfigClass::saveFileIfNeeded()
{ // if we should save, do save
  if( _shouldSaveConfig )
  {
    saveFile();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConfigClass::saveFile()
{ // Save the custom parameters to config.json
  nextion.setAttr("p[0].b[1].txt", "\"Saving\\rconfig\"");
  debug.printLn(F("SPIFFS: Saving config"));
  DynamicJsonDocument jsonConfigValues(1024);
  jsonConfigValues["mqttServer"] = _mqttServer;
  jsonConfigValues["mqttPort"] = _mqttPort;
  jsonConfigValues["mqttUser"] = _mqttUser;
  jsonConfigValues["mqttPassword"] = _mqttPassword;
  jsonConfigValues["haspNode"] = _haspNode;
  jsonConfigValues["groupName"] = _groupName;
  jsonConfigValues["configUser"] = web.getUser();
  jsonConfigValues["configPassword"] = web.getPassword();
  jsonConfigValues["motionPinConfig"] = _motionPin;
  jsonConfigValues["debugSerialEnabled"] = debug.getSerialEnabled();
  jsonConfigValues["debugTelnetEnabled"] = debug.getTelnetEnabled();
  jsonConfigValues["mdnsEnabled"] = _mdnsEnabled;
  jsonConfigValues["beepEnabled"] = beep.getEnable();

  debug.printLn(String(F("SPIFFS: mqttServer = ")) + String(_mqttServer));
  debug.printLn(String(F("SPIFFS: mqttPort = ")) + String(_mqttPort));
  debug.printLn(String(F("SPIFFS: mqttUser = ")) + String(_mqttUser));
  debug.printLn(String(F("SPIFFS: mqttPassword = ")) + String(_mqttPassword));
  debug.printLn(String(F("SPIFFS: haspNode = ")) + String(_haspNode));
  debug.printLn(String(F("SPIFFS: groupName = ")) + String(_groupName));
  debug.printLn(String(F("SPIFFS: configUser = ")) + String(web.getUser()));
  debug.printLn(String(F("SPIFFS: configPassword = ")) + String(web.getPassword()));
  debug.printLn(String(F("SPIFFS: motionPinConfig = ")) + String(_motionPin));
  debug.printLn(String(F("SPIFFS: debugSerialEnabled = ")) + String(debug.getSerialEnabled()));
  debug.printLn(String(F("SPIFFS: debugTelnetEnabled = ")) + String(debug.getTelnetEnabled()));
  debug.printLn(String(F("SPIFFS: mdnsEnabled = ")) + String(_mdnsEnabled));
  debug.printLn(String(F("SPIFFS: beepEnabled = ")) + String(beep.getEnable()));

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    debug.printLn(F("SPIFFS: Failed to open config file for writing"));
  }
  else
  {
    serializeJson(jsonConfigValues, configFile);
    configFile.close();
  }
  _shouldSaveConfig = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConfigClass::clearFileSystem()
{ // Clear out all local storage
  nextion.setAttr("dims", "100");
  nextion.sendCmd("page 0");
  nextion.setAttr("p[0].b[1].txt", "\"Resetting\\rsystem...\"");
  debug.printLn(F("RESET: Formatting SPIFFS"));
  SPIFFS.format();
  debug.printLn(F("RESET: Clearing WiFiManager settings..."));
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  EEPROM.begin(512);
  debug.printLn(F("Clearing EEPROM..."));
  for (uint16_t i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
  nextion.setAttr("p[0].b[1].txt", "\"Rebooting\\rsystem...\"");
  debug.printLn(F("RESET: Rebooting device"));
  esp.reset();
}
