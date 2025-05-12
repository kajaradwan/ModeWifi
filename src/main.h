#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <mcp2515.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <String.h>
#include "ble_interface.h"

// Forward declarations
class CM;
class BLEInterface;

// Function declarations
void sendPost(char* szCommand);
void handleMainCommand(String szCommand);
void handleCommand(const std::string& command);  // For BLE interface
void resetFilters();
bool filterInMsg(long drg);
bool filterOutMsg(long drg);
bool filterInByte0(byte b);
bool filterOutByte0(byte b);
void handlePDMMessage(float t, can_frame m);
void handlePDMShort(can_frame m);
int ssidExists(String search);
void setWebVariable(const char* szVariableName, int nValue);
void setWebVariable(const char* szVariableName, float fValue);
void postCurrentState();
void logData(int nSensorID, const char* szDataType, const char* szDataValue);
void handleFileUpload();
void handleAJAX();
void parseFile(String szFname);

// Global variables
extern WebServer server;
extern MCP2515 mcp2515;
extern can_frame canMsg;
extern CM cm;
extern BLEInterface bleInterface;

#endif // MAIN_H 