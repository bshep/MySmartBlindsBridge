#include <Arduino.h>
#include <arduino-timer.h>

#ifdef ENABLE_OTA
#include <ArduinoOTA.h>
#endif

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <LittleFS.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <base64.hpp>

#ifndef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#warning "Not compatible hardware"
#endif


String readFileIntoString(String filename);
bool onRefreshBLEScan( void *args);
bool onHandleOTA( void *args );

void handle_OnConnect(AsyncWebServerRequest *request);
void handle_OnScan(AsyncWebServerRequest *request);
void handle_OnCSS(AsyncWebServerRequest *request);
void handle_OnRefreshBlinds(AsyncWebServerRequest *request);

