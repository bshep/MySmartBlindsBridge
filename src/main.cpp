#define ENABLE_DEBUG

#include "main.h"

cSF(hostName, 32, "");

bool BlindsRefreshNow = true;
bool rebootFlag = false;

AsyncWebServer webServer(80);
AsyncWebServer debugServer(88);

blind *blindsList[HA_MAXDEVICES];
HACover *coverList[HA_MAXDEVICES];
HASensorNumber *sensorList[HA_MAXDEVICES];
HABinarySensor *chargingSensorList[HA_MAXDEVICES];

int blindCount = 0;

Timer<10> timer;

cSF(DEBUGTEXT, 1024, "");
cSF(BROKER_ADDR, 32, "");
cSF(blindsConfig, 1024);

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device, HA_MAXDEVICES + 3);
byte deviceMAC[18];

UMS3 ums3;

void setup()
{
  // Init UMS3 TinyS3 board
  ums3.begin();
  ums3.setPixelPower(true);

  Serial.begin(115200);

  // Init FS
  if (!LittleFS.begin())
  {
    Serial.println("ERROR WITH FILESYSTEM");
  }
  else
  {
    Serial.println("LittleFS Initialized");
  }

  WiFi.macAddress(deviceMAC);
  // Load hostname from config file
  readFileIntoString("/hostname", hostName);
  // IF file empty or does not exist then set hostname to msbXXXXXX where X's are the first 3 bytes of the mac address
  if (hostName == "")
  {
    hostName.printf("msb%X%X%X", deviceMAC[0], deviceMAC[1], deviceMAC[2]);
  }

  Serial.print("Hostname set to:");
  Serial.println(hostName);

  // Init WiFi
  WiFi.setHostname(hostName.c_str());

  setESPAutoWiFiConfigDebugOut(Serial);
  if (ESPAutoWiFiConfigSetup(-RGB_DATA, true, 0))
  {
    return;
  }

  // Init WebSerial Debug Console
  WebSerial.begin(&debugServer);
  WebSerial.msgCallback(onWebSerial_recvMsg);
  debugServer.begin();

  // Setup OTA Update
  ArduinoOTA.begin();

  ArduinoOTA.onStart([]()
                     { DEBUG_PRINTLN("OTA Begin"); 
                     BlindsRefreshNow = false; });

  ArduinoOTA.onEnd([]()
                   { DEBUG_PRINTLN("OTA Finished"); });

  // Init BLE Subsystem
  BLEDevice::init(hostName.c_str());

  // Init WebServer to handle requests
  webServer.on("/", handle_OnConnect);
  webServer.on("/refresh", handle_OnRefreshBlinds);
  webServer.on("/style.css", handle_OnReturnFile);
  webServer.on("/script.js", handle_OnReturnFile);
  webServer.on("/reboot", [](AsyncWebServerRequest *req)
               {
                rebootFlag = true; 
                req->redirect("/"); });
  webServer.begin();

  // Init HA and MQTT services
  device.setUniqueId(deviceMAC, sizeof(deviceMAC));
  device.setName("MySmartBlindsBridge MDNS");
  device.setSoftwareVersion("1.0.0");
  device.enableLastWill();

  // If BROKER_ADDR unset, skip starting the mqtt server
  readFileIntoString("/brokeraddress", BROKER_ADDR);
  Serial.print("BrokerAddress set to:");
  Serial.println(BROKER_ADDR);
  if (BROKER_ADDR == "")
  {
    mqtt.begin(BROKER_ADDR.c_str());
  }

  // Read in the configuration of known blinds and begin refreshing them
  readBlindsConfig();
  timer.every(1000, onRefreshBlinds);
}

void loop()
{
  if (!ESPAutoWiFiConfigLoop()) // Skips below if WiFi not configured
  {
    timer.tick();        // Service timers
    ArduinoOTA.handle(); // Handle OTA if any
    mqtt.loop();         // Handle MQTT
  }
}

// Handles requests to refresh blinds
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

      if (cmd == "hostname")
      {
        DEBUG_PRINTLN("Request to set hostname");
        String myValue = request->arg("value");
        if (myValue != "")
        {
          File myFile = LittleFS.open("/hostname", "w", true);
          myFile.print(myValue);
          myFile.close();
          hostName.clear().printf("%s", myValue.c_str());
        }
      }

      if (cmd == "brokerAddress")
      {
        DEBUG_PRINTLN("Request to set brokerAddress");
        String myValue = request->arg("value");
        if (request->arg("value") != "")
        {
          File myFile = LittleFS.open("/brokeraddress", "w", true);
          myFile.print(myValue);
          myFile.close();
          BROKER_ADDR.clear().printf("%s", myValue.c_str());
        }
      }

      if (cmd == "blindsConfig")
      {
        DEBUG_PRINTLN("Request to set blindsConfig");
        String myValue = request->arg("value");
        if (request->arg("value") != "")
        {
          File myFile = LittleFS.open("/blinds.cfg", "w", true);
          myFile.print(myValue);
          myFile.close();
          blindsConfig.clear().printf("%s", myValue.c_str());
          DEBUG_PRINTLN(blindsConfig.c_str());
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
    return F(hostName.c_str());
  }

  if (var == "BROKERADDRESS")
  {
    return F(BROKER_ADDR.c_str());
  }

  if (var == "DEVICES")
  {
    cSF(tmpDevices, 4096, "");
    blind *myblind;
    tmpDevices.printf("<h2>Total Found %i </h2>", blindCount);
    tmpDevices.print("<table class=\"table\">");
    tmpDevices.print("<thead><tr><td>Name</td><td>Angle</td><td>Controls</td></tr></thead>");

    for (int i = 0; i < blindCount; i++)
    {
      myblind = blindsList[i];

      tmpDevices.print("<tr>");
      tmpDevices.printf("<td>%s</td>", myblind->name());
      tmpDevices.printf("<td>%i</td>", myblind->getAngle());
      tmpDevices.printf("<td><a class=\"button\" href=\"/?cmd=open&mac=%s\">Open</a>", myblind->mac());
      tmpDevices.printf("<a class=\"button\" href=\"/?cmd=close&mac=%s\">Close</a></td>", myblind->mac());

      tmpDevices.print("</tr>");
    }
    tmpDevices.print("</table>");

    return F(tmpDevices.c_str());
  }

  if (var == "BLINDSCONFIG")
  {
    return (F(blindsConfig.c_str()));
  }

  if (var == "DEBUGTEXT")
  {
    DEBUGTEXT.clear();
    DEBUGTEXT.printf("<p>Version: %s</p>", VERSION);
    DEBUGTEXT.printf("<p>Built on: %s</p>", BUILD_TIMESTAMP);
    return F(DEBUGTEXT.c_str());
  }

  return String();
}

void handle_OnConnect(AsyncWebServerRequest *request)
{
  DEBUG_PRINTLN("HTTP Request for: " + request->url());

  // Need to delay reboot until page is refreshed, otherwise can cause us to stay on the /reboot page and cause continous reboots everytime it reloads
  if (rebootFlag)
  {
    ESP.restart();
  }

  if (request->args() > 0)
  {
    handle_HTTPArgs(request);
    request->redirect("/");
    return;
  }

  request->send(LittleFS, "/index.html", String(), false, handle_OnConnectProcessor);
}

void handle_OnReturnFile(AsyncWebServerRequest *request)
{
  DEBUG_PRINTLN("HTTP Request for: " + request->url());
  request->send(LittleFS, request->url());
}

void readFileIntoString(const char *filename, SafeString &result)
{
  File tmpFile;
  result = "";

  if (LittleFS.exists(filename))
  {

    tmpFile = LittleFS.open(filename, "r");

    while (tmpFile.available())
    {
      result.print(tmpFile.readString().c_str());
    }

    tmpFile.close();
  }
  else
  {
    result = "";
  }
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
    for (int i = 0; i < 18; i++)
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
