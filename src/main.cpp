#define ENABLE_DEBUG

#include "main.h"

static AsyncWebServer webServer(80);
static AsyncWebServer debugServer(88);

blind *blindsList[HA_MAXDEVICES];
HACover *coverList[HA_MAXDEVICES];
HASensorNumber *sensorList[HA_MAXDEVICES];
HABinarySensor *chargingSensorList[HA_MAXDEVICES];

int blindCount = 0;

Timer<> timer;

String DEBUGTEXT;
String blindsConfig = "";

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device, HA_MAXDEVICES + 3);
byte deviceMAC[HA_MACLENGTH];

bool BlindsRefreshNow = false;
bool BLEScanNow = true;
scanner *myBLEScanner;

UMS3 ums3;

// These settings are brought in by reading wifi.cfg
String wifiSSID, wifiPassphrase, wifiHostname, brokerAddress;
char cstr_brokerAddress[64];
char cstr_hostname[32];

bool onTurnOffLED(void *args)
{
  ums3.setPixelPower(false);
  return false;
}

void setup()
{
  ums3.begin();
  ums3.setPixelPower(true);
  ums3.setPixelColor(255, 0, 0);

  // Initialize debugging
  Serial.begin(115200);
  WebSerial.begin(&debugServer);
  WebSerial.msgCallback(onWebSerial_recvMsg);

  // InitializeFS
  if (!LittleFS.begin())
  {
    Serial.println("ERROR WITH FILESYSTEM");
  }
  else
  {
    Serial.println("LittleFS Initialized");
  }

  // sleep(2);
  readWifiConfig(wifiSSID, wifiPassphrase, wifiHostname, brokerAddress);
  brokerAddress.toCharArray(cstr_brokerAddress, 64);
  wifiHostname.toCharArray(cstr_hostname, 32);

  Serial.println("ssid = " + wifiSSID + " - passphrase = " + wifiPassphrase + " - hostname = " + wifiHostname);
  WiFi.setHostname(cstr_hostname);
  WiFi.begin(wifiSSID.c_str(), wifiPassphrase.c_str());

  uint8_t pos = 0;
  while (!WiFi.isConnected())
  {
  }

  ums3.setPixelColor(0, 255, 0);

  ArduinoOTA.begin();

  ArduinoOTA.onStart([]()
                     { DEBUG_PRINTLN("OTA Begin"); 
                     BlindsRefreshNow = false; });

  ArduinoOTA.onEnd([]()
                   { DEBUG_PRINTLN("OTA Finished"); });

  BLEDevice::init(cstr_hostname);

  webServer.on("/", handle_OnConnect);
  // webServer.on("/style.css", handle_OnReturnFile);
  // webServer.on("/script.js", handle_OnReturnFile);
  webServer.on("/cmd/refresh", handle_Command);
  webServer.on("/cmd/scan", handle_Command);
  webServer.on("/cmd/closeAll", handle_Command);
  webServer.on("/cmd/openAll", handle_Command);
  webServer.on("/cmd/open", handle_Command);
  webServer.on("/cmd/close", handle_Command);

  webServer.onNotFound(handle_OnReturnFile);
  webServer.begin();

  debugServer.begin();

  // set device's details (optional)
  // Unique ID must be set!
  WiFi.macAddress(deviceMAC);
  device.setUniqueId(deviceMAC, sizeof(deviceMAC));
  device.setName("MySmartBlindsBridge MDNS");
  device.setManufacturer("Sheplan Development");
  device.setModel("ESP2MSB");
  device.setSoftwareVersion(VERSION);
  device.enableLastWill();
  mqtt.begin(cstr_brokerAddress);

  DEBUG_PRINTLN(WiFi.localIP().toString());

  DEBUG_PRINTLN("Will now scan for BLE Devices.");
  myBLEScanner = new scanner();
  myBLEScanner->RefreshBLEScan();
  DEBUG_PRINT(" - found ");
  DEBUG_PRINT(myBLEScanner->foundDevices.getCount());
  DEBUG_PRINTLN(" devices");

  DEBUG_PRINTLN("Will now read config and connenct to blinds.");
  readBlindsConfig();

  timer.every(1000 * 10, onRefreshBlinds);
  timer.every(1000 * 60 * 60 * 24, onReboot);
  timer.every(1000 * 60, onRefreshBLEScan);
  timer.in(5000, onTurnOffLED);
}

void loop()
{
  timer.tick();
  ArduinoOTA.handle();
  mqtt.loop();
}

bool onReboot(void *args)
{
  ESP.restart();
  return true;
}

void handle_OnRefreshBlinds(AsyncWebServerRequest *request)
{
  DEBUG_PRINTLN("HTTP Request for: " + request->url());
  BlindsRefreshNow = true;
}

// void handle_HTTPArgs(AsyncWebServerRequest *request)
// {
//   int numArgs = request->args();

//   if (numArgs > 0)
//   {
//     DEBUG_PRINTLN("Got some args");

//     String cmd = request->arg("cmd");

//     if (cmd != "")
//     {
// Got a cmd
// if (cmd == "open" || cmd == "close")
// {
//   String mac = request->arg("mac"); // Returns empty string if arg does not exist
//   if (mac == "")
//   {
//     return; // return if mac address not specified
//   }
//   blind *foundBlind = findBlindByMac(mac.c_str());
//   if (foundBlind)
//   {
//     if (cmd == "open")
//     {
//       foundBlind->setAngle(100);
//     }
//     else
//     {
//       foundBlind->setAngle(200);
//     }
//   }
// }

// if (cmd == "openAll" || cmd == "closeAll")
// {
//   for (int i = 0; i < blindCount; i++)
//   {
//     if (cmd == "openAll")
//     {
//       blindsList[i]->setAngle(100);
//     }
//     else
//     {
//       blindsList[i]->setAngle(200);
//     }
//   }
// }
//     }
//   }
//   else
//   {
//     DEBUG_PRINTLN("Got NO args");
//   }
// }

void handle_Command(AsyncWebServerRequest *request)
{
  DEBUG_PRINTLN("HTTP Request for: " + request->url());

  if (request->url() == "/cmd/closeAll")
  {
    for (int i = 0; i < blindCount; i++)
    {
      blindsList[i]->setAngle(200);
    }
  }

  if (request->url() == "/cmd/openAll")
  {
    for (int i = 0; i < blindCount; i++)
    {
      blindsList[i]->setAngle(100);
    }
  }

  if (request->url() == "/cmd/scan")
  {
    BLEScanNow = true;
  }

  if (request->url() == "/cmd/refresh")
  {
    BlindsRefreshNow = true;
  }

  if (request->url() == "/cmd/open" || request->url() == "/cmd/close")
  {
    String mac = request->arg("mac"); // Returns empty string if arg does not exist
    if (mac == "")
    {
      return; // return if mac address not specified
    }
    blind *foundBlind = findBlindByMac(mac.c_str());
    if (foundBlind)
    {
      if (request->url() == "/cmd/open")
      {
        foundBlind->setAngle(100);
      }
      else
      {
        foundBlind->setAngle(200);
      }
    }
  }
}

String handle_OnConnectProcessor(const String &var)
{
  if (var == "HOSTNAME")
  {
    return wifiHostname;
  }

  if (var == "BROKERADDRESS")
  {
    return brokerAddress;
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
      tmpDevices += "<td><div class=\"button\" onclick=\"buttonClick(\'/cmd/open?mac=";
      tmpDevices += myblind->mac();
      tmpDevices += "')\">Open</div>";
      tmpDevices += "<div class=\"button\" onclick=\"buttonClick(\'/cmd/close?mac=";
      tmpDevices += myblind->mac();
      tmpDevices += "')\">Close</div></td>";

      tmpDevices += "</tr>";
    }
    tmpDevices += "</table>";

    return F(tmpDevices.c_str());
  }

  if (var == "BLINDSCONFIG")
  {
    return (F(blindsConfig.c_str()));
  }

  if (var == "BLESCANRESULTS")
  {
    String tmpDevices;

    tmpDevices += "<h2>Nearby Devices - ";
    tmpDevices += myBLEScanner->foundDevices.getCount();
    tmpDevices += "</h2>";

    tmpDevices += "<table class=\"table\">";
    tmpDevices += "<thead><tr>";
    tmpDevices += "<td>Address</td>";
    tmpDevices += "<td>Name</td>";
    tmpDevices += "<td>RSSI</td>";
    tmpDevices += "</tr></thead>";
    tmpDevices += "";

    for (int a = 0; a < myBLEScanner->foundDevices.getCount(); a++)
    {
      BLEAdvertisedDevice tmpDevice = myBLEScanner->foundDevices.getDevice(a);
      tmpDevices += "<tr>";
      tmpDevices += "<td>";
      tmpDevices += tmpDevice.getAddress().toString().c_str();
      tmpDevices += "</td>";
      tmpDevices += "<td>";
      tmpDevices += tmpDevice.getName().c_str();
      tmpDevices += "</td>";
      tmpDevices += "<td>";
      tmpDevices += tmpDevice.getRSSI();
      tmpDevices += "</td>";
      tmpDevices += "</tr>";
    }
    tmpDevices += "</table>";

    return F(tmpDevices.c_str());
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
  // DEBUG_PRINTLN("HTTP Request for: " + request->url());

  // if (request->args() > 0)
  // {
  //   handle_HTTPArgs(request);
  //   request->redirect("/");
  //   return;
  // }
  request->send(LittleFS, "/index.html", String(), false, handle_OnConnectProcessor);
}

void handle_OnReturnFile(AsyncWebServerRequest *request)
{
  DEBUG_PRINTLN("HTTP Request for: " + request->url());

  if (LittleFS.exists(request->url()))
  {
    request->send(LittleFS, request->url());
  }
  else
  {
    request->send(404, "text/plain", "404 - File Not found" + request->url());
  }
}

// String readFileIntoString(String filename)
// {
//   String tmpStr = "";
//   File tmpFile = LittleFS.open(filename, "r");

//   while (tmpFile.available())
//   {
//     tmpStr += tmpFile.readString();
//   }

//   tmpFile.close();

//   return tmpStr;
// }

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
      WebSerial.println("Status - chargeStatus: " + String(blindsList[i]->sensors->getBatteryPercentage()));
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

bool onRefreshBLEScan(void *args)
{
  if (BLEScanNow)
  {
    BLEScanNow = false;
    myBLEScanner->RefreshBLEScan();
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
