#include "main.h"

// #define ENABLE_DEBUG

char hostName[32] = "msb2";

bool BlindsRefreshNow = true;

AsyncWebServer webServer(80);
AsyncWebServer debugServer(88);

blind *blindsList[HA_MAXDEVICES];
HACover *coverList[HA_MAXDEVICES];
HASensorNumber *sensorList[HA_MAXDEVICES];
HABinarySensor *chargingSensorList[HA_MAXDEVICES];

int blindCount = 0;

Timer<10> timer;

String DEBUGTEXT;
String blindsConfig = "";

#define BROKER_ADDR "192.168.2.222"
WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device, HA_MAXDEVICES + 3);
byte deviceMAC[HA_MACLENGTH];

UMS3 ums3;

void setup()
{
  ums3.begin();
  ums3.setPixelPower(true);

  Serial.begin(115200);

  if (!LittleFS.begin())
  {
    Serial.println("ERROR WITH FILESYSTEM");
  }
  else
  {
    Serial.println("LittleFS Initialized");
  }

  WiFi.setHostname(hostName);

  setESPAutoWiFiConfigDebugOut(Serial);
  if (ESPAutoWiFiConfigSetup(-RGB_DATA, true, 0))
  {
    return;
  }

  WebSerial.begin(&debugServer);
  WebSerial.msgCallback(onWebSerial_recvMsg);

  ArduinoOTA.begin();

  ArduinoOTA.onStart([]()
                     { DEBUG_PRINTLN("OTA Begin"); 
                     BlindsRefreshNow = false; });

  ArduinoOTA.onEnd([]()
                   { DEBUG_PRINTLN("OTA Finished"); });

  BLEDevice::init(hostName);

  webServer.on("/", handle_OnConnect);
  webServer.on("/refresh", handle_OnRefreshBlinds);
  webServer.on("/style.css", handle_OnCSS);
  webServer.begin();

  debugServer.begin();

  // set device's details (optional)
  // Unique ID must be set!
  WiFi.macAddress(deviceMAC);
  device.setUniqueId(deviceMAC, sizeof(deviceMAC));
  device.setName("MySmartBlindsBridge MDNS");
  device.setSoftwareVersion("1.0.0");
  device.enableLastWill();
  mqtt.begin(BROKER_ADDR);

  readBlindsConfig();

  timer.every(1000, onRefreshBlinds);
}

void loop()
{
  if (!ESPAutoWiFiConfigLoop())
  {
    timer.tick();
    ArduinoOTA.handle();
    mqtt.loop();
  }
}

void handle_OnRefreshBlinds(AsyncWebServerRequest *request)
{
  request->redirect("/");

  BlindsRefreshNow = true;
}

void handle_HTTPArgs(AsyncWebServerRequest *request)
{
  int numArgs = request->args();

  if (numArgs > 0)
  {
    DEBUG_PRINTLN("Got some args");

    String cmd = request->arg("cmd");

    if (cmd != "")
    {
      // Got a cmd
      if (cmd == "open" || cmd == "close")
      {
        String mac = request->arg("mac"); // Returns empty string if arg does not exist
        if (mac == "")
        {
          return; // return if mac address not specified
        }
        blind *foundBlind = findBlindByMac(mac.c_str());
        if (foundBlind)
        {
          if (cmd == "open")
          {
            foundBlind->setAngle(100);
          }
          else
          {
            foundBlind->setAngle(200);
          }
        }
      }

      if (cmd == "openAll" || cmd == "closeAll")
      {
        for (int i = 0; i < blindCount; i++)
        {
          if (cmd == "openAll")
          {
            blindsList[i]->setAngle(100);
          }
          else
          {
            blindsList[i]->setAngle(200);
          }
        }
      }
    }
  }
  else
  {
    DEBUG_PRINTLN("Got NO args");
  }
}

String handle_OnConnectProcessor(const String &var)
{
  if (var == "HOSTNAME")
  {
    return hostName;
  }

  if (var == "BROKERADDRESS")
  {
    return BROKER_ADDR;
  }

  if (var == "DEVICES")
  {
    String tmpDevices;
    blind *myblind;
    tmpDevices += "<h2>Total Found ";
    tmpDevices += blindCount;
    tmpDevices += " </h2>";
    tmpDevices += "<table class=\"table\">";
    tmpDevices += "<thead><tr><td>Name</td><td>Angle</td><td>Controls</td></tr></thead>";

    for (int i = 0; i < blindCount; i++)
    {
      myblind = blindsList[i];

      tmpDevices += "<tr>";
      tmpDevices += "<td>";
      tmpDevices += myblind->name();
      tmpDevices += "</td>";
      tmpDevices += "<td>";
      tmpDevices += myblind->getAngle();
      tmpDevices += "</td>";
      tmpDevices += "<td><a class=\"button\" href=\"/?cmd=open&mac=";
      tmpDevices += myblind->mac();
      tmpDevices += "\">Open</a>";
      tmpDevices += "<a class=\"button\" href=\"/?cmd=close&mac=";
      tmpDevices += myblind->mac();
      tmpDevices += "\">Close</a></td>";

      tmpDevices += "</tr>";
    }
    tmpDevices += "</table>";

    return F(tmpDevices.c_str());
  }

  if (var == "BLINDSCONFIG")
  {
    return (F(blindsConfig.c_str()));
  }

  if (var == "DEBUGTEXT")
  {
    DEBUGTEXT.clear();
    DEBUGTEXT += "<p>Version:";
    DEBUGTEXT += VERSION;
    DEBUGTEXT += "</p>";

    DEBUGTEXT += "<p>Built on:";
    DEBUGTEXT += BUILD_TIMESTAMP;
    DEBUGTEXT += "</p>";
    return F(DEBUGTEXT.c_str());
  }

  return String();
}

void handle_OnConnect(AsyncWebServerRequest *request)
{
  DEBUG_PRINTLN("HTTP Request for: " + request->url());

  if (request->args() > 0)
  {
    handle_HTTPArgs(request);
    request->redirect("/");
    return;
  }

  request->send(LittleFS, "/index.html", String(), false, handle_OnConnectProcessor);
}

void handle_OnCSS(AsyncWebServerRequest *request)
{
  DEBUG_PRINTLN("HTTP Request for: " + request->url());
  request->send(200, "text/css", readFileIntoString("/style.css"));
}

String readFileIntoString(String filename)
{
  String tmpStr = "";
  File tmpFile = LittleFS.open(filename, "r");

  while (tmpFile.available())
  {
    tmpStr += tmpFile.readString();
  }

  tmpFile.close();

  return tmpStr;
}

void onWebSerial_recvMsg(uint8_t *data, size_t len)
{
  DEBUG_PRINTLN("Received Data...");
  String d = String(data, len);

  WebSerial.println(d);

  d.toUpperCase();

  if (d == "REBOOT")
  {
    DEBUG_PRINTLN("REBOOTING");
    delay(500);
    ESP.restart();
  }

  if (d == "PRINTMAC")
  {
    DEBUG_PRINT("DEVICE MAC : ");
    for (int i = 0; i < HA_MACLENGTH; i++)
    {
      char byteStr[10];
      sprintf(byteStr, "%x", deviceMAC[i]);
      DEBUG_PRINT(byteStr);
    }
    DEBUG_PRINT("\n");
  }

  if (d == "SENSORS")
  {
    for (int i = 0; i < blindCount; i++)
    {
      blindsList[i]->refreshSensors();
    }
  }

  if (d == "STATUS")
  {
    for (int i = 0; i < blindCount; i++)
    {
      blindsList[i]->refreshStatus();

      WebSerial.println("Status - isChargingUSB: " + String(blindsList[i]->status->isChargingUSB()));
      WebSerial.println("Status - isChargingSolar: " + String(blindsList[i]->status->isChargingSolar()));
      WebSerial.println("Status - isPasskeyValid: " + String(blindsList[i]->status->isPasskeyValid()));
    }
  }
}

bool onRefreshBlinds(void *args)
{
  int pos = 0;   // in MSB 100 == open and 200 == closed
  int HApos = 0; // in HA 0 == closed and 100 == open

  if (BlindsRefreshNow)
  {
    // BlindsRefreshNow = false;
    for (int i = 0; i < blindCount; i++)
    {
      blindsList[i]->refresh();
      blindsList[i]->refreshSensors();
      blindsList[i]->refreshStatus();
      // coverList[i]->setName(blindsList[i]->name());
      // sensorList[i]->setName(blindsList[i]->name());
      sensorList[i]->setValue(blindsList[i]->sensors->getBatteryPercentage());

      pos = blindsList[i]->getAngle();

      HApos = 100 - (pos - 100);
      if (pos > 95 && pos < 105)
      {
        coverList[i]->setState(HACover::StateOpen);
      }
      else if (pos > 195 || pos < 5)
      {
        coverList[i]->setState(HACover::StateClosed);
      }

      coverList[i]->setPosition(HApos);
      chargingSensorList[i]->setState(blindsList[i]->status->isChargingSolar() || blindsList[i]->status->isChargingUSB());
    }
  }
  return true;
}

blind *findBlindByMac(const char *mac)
{
  blind *result = NULL;
  DEBUG_PRINTLN("findBlindByMac - Entered Function");

  DEBUG_PRINTLN(" -- searching for mac: " + String(mac) + "of length = " + String(strlen(mac)));

  for (int i = 0; i < blindCount; i++)
  {
    DEBUG_PRINTLN(" -- comparing against mac: " + String(blindsList[i]->mac()) + "of length = " + String(strlen(blindsList[i]->mac())));

    if (strcmp(mac, blindsList[i]->mac()) == 0)
    {
      result = blindsList[i];
      break;
    }
  }

  return result;
}
