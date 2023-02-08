// #include <Arduino.h>

// void setup() {
//   // put your setup code here, to run once:
// }

// void loop() {
//   // put your main code here, to run repeatedly:
// }

/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#define ENABLE_OTA

#include "main.h"
#include "blind.h"
#include "config.h"

char hostName[32] = "msb1";
char ssid[32];
char passphrase[64];

static std::string BlindBLEName = "SmartBlind_DFU";

int scanTime = 5; // In seconds
BLEScan *pBLEScan;
BLEScanResults foundDevices;
bool BLERefreshNow = false;
bool BlindsRefreshNow = true;

AsyncWebServer webServer(80);
AsyncWebServer debugServer(88);

blind *blindsList[10];
HACover *coverList[10];
int blindCount = 0;

bool scanDone = false;

Timer<10> timer;

String DEBUGTEXT;

#define BROKER_ADDR "192.168.2.222"
WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
// HACover *coverDevice = new HACover("MSB_Cover1", HACover::PositionFeature);

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
  }
};

void recvMsg(uint8_t *data, size_t len)
{
  WebSerial.println("Received Data...");
  String d = "";
  for (int i = 0; i < len; i++)
  {
    d += char(data[i]);
  }
  WebSerial.println(d);
  if (d == "REBOOT")
  {
    WebSerial.println("REBOOTING");
    delay(500);
    ESP.restart();
  }
}

bool onRefreshBLEScan(void *args)
{
  if (BLERefreshNow)
  {
    BLERefreshNow = false;
    WebSerial.println("Updating Scan Results....");
    foundDevices = pBLEScan->start(scanTime, true);
    WebSerial.println("Done!");
  }
  return true;
}

bool onRefreshBlinds(void *args)
{
  int pos = 0;

  if (BlindsRefreshNow)
  {
    // BlindsRefreshNow = false;
    for (int i = 0; i < blindCount; i++)
    {
      blindsList[i]->refresh();
      coverList[i]->setName(blindsList[i]->name());

      pos = blindsList[i]->getAngle();
      pos -= 100; // Angle goes from 100(open) to 200(closed)

      coverList[i]->setPosition(pos);
    }
  }
  return true;
}

#ifdef ENABLE_OTA
bool onHandleOTA(void *args)
{
  ArduinoOTA.handle();
  return true;
}
#endif

blind *findBlindByMac(const char *mac)
{
  blind *result = NULL;
  WebSerial.println("findBlindByMac - Entered Function");

  WebSerial.println(" -- searching for mac: " + String(mac) + "of length = " + String(strlen(mac)));

  for (int i = 0; i < blindCount; i++)
  {
    WebSerial.println(" -- comparing against mac: " + String(blindsList[i]->mac()) + "of length = " + String(strlen(blindsList[i]->mac())));

    if (strcmp(mac, blindsList[i]->mac()) == 0)
    {
      result = blindsList[i];
      break;
    }
  }

  return result;
}

void setup()
{
  Serial.begin(115200);

  if (!LittleFS.begin())
  {
    Serial.println("ERROR WITH FILESYSTEM");
  }
  else
  {
    Serial.println("LittleFS Initialized");
  }

  readWIFIConfig();
  WiFi.setHostname(hostName);
  WiFi.begin(ssid, passphrase);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    // Serial.println("Wifi Connecting..." + ssid + " " + passphrase);
    Serial.println("Wifi Connecting...");
  }

  WebSerial.begin(&debugServer);
  WebSerial.msgCallback(recvMsg);

#ifdef ENABLE_OTA
  ArduinoOTA.begin();

  ArduinoOTA.onStart([]()
                     { WebSerial.println("OTA Begin"); 
                     BlindsRefreshNow = false; });

  // ArduinoOTA.onProgress([](int totalWritten, int size)
  //                       {
  //   static int lastPercent = 0;
  //   int percent = totalWritten*100 / size;

  //   if (percent > lastPercent) {
  //       lastPercent = percent;

  //       WebSerial.println("OTA Progress - Written " + String(totalWritten) + " of " + String(size) + " - " + String(lastPercent) + "\% Done");
  //   } });

  ArduinoOTA.onEnd([]()
                   {
                     WebSerial.println("OTA Finished");
                     // WiFi.disconnect(true,false);
                   });

#endif

  BLEDevice::init(hostName);
  // pBLEScan = BLEDevice::getScan(); // create new scan
  // pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  // pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  // pBLEScan->setInterval(1000);
  // pBLEScan->setWindow(200); // less or equal setInterval value

  webServer.on("/", handle_OnConnect);
  webServer.on("/scan", handle_OnScan);
  webServer.on("/refresh", handle_OnRefreshBlinds);
  webServer.on("/style.css", handle_OnCSS);
  webServer.begin();

  debugServer.begin();

  // set device's details (optional)
  // Unique ID must be set!
  byte mac[18];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));
  device.setName("MySmartBlindsBridge MDNS");
  device.setSoftwareVersion("1.0.0");
  mqtt.begin(BROKER_ADDR);

  readBlindsConfig();

  timer.every(1000, onRefreshBLEScan);
  timer.every(1000, onRefreshBlinds);
}

void loop()
{
  timer.tick();
  ArduinoOTA.handle();
  mqtt.loop();
  // Serial.print(".");
}

void handle_OnRefreshBlinds(AsyncWebServerRequest *request)
{
  request->redirect("/");

  BlindsRefreshNow = true;
}

void handle_OnScan(AsyncWebServerRequest *request)
{
  WebSerial.println("HTTP Request for: " + request->url());

  request->redirect("/");

  BLERefreshNow = !BLERefreshNow;
}

void handle_Args(AsyncWebServerRequest *request)
{
  int numArgs = request->args();

  if (numArgs > 0)
  {
    WebSerial.println("Got some args");

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
    WebSerial.println("Got NO args");
  }
}

void handle_OnConnect(AsyncWebServerRequest *request)
{
  WebSerial.println("HTTP Request for: " + request->url());

  if (request->args() > 0)
  {
    handle_Args(request);
    request->redirect("/");
    return;
  }

  String tmp = readFileIntoString("/index.html");
  String tmpDevices = "";
  blind *myblind;

  // DEBUGTEXT = "<h2>SSID: " + String(ssid) + "</h2>";
  // DEBUGTEXT += "<h2>PASSPHRASE: " + String(passphrase) + "</h2>";

  tmpDevices += "<h2>Total Found " + String(blindCount) + "</h2>";

  for (int i = 0; i < blindCount; i++)
  {
    myblind = blindsList[i];
    String isConnected = "NA";
    // if (myblind->isConnected())
    // {
    //   isConnected = "Yes";
    // }
    // else
    // {
    //   isConnected = "No";
    // }

    tmpDevices += "<li>";
    // tmpDevices += " Address: " + myblind->mac();
    // tmpDevices += " - Key: " + myblind->key();
    tmpDevices += " Name: " + String(myblind->name());
    tmpDevices += " - Connected: " + isConnected;
    tmpDevices += " - Angle: " + String(myblind->getAngle());
    tmpDevices += "<a class=\"button\" href=\"/?cmd=open&mac=" + String(myblind->mac()) + "\">Open</a>";
    tmpDevices += "<a class=\"button\" href=\"/?cmd=close&mac=" + String(myblind->mac()) + "\">Close</a>";

    tmpDevices += "</li>";
  }
  tmp.replace("<!-- DEVICES -->", tmpDevices);
  tmp.replace("<!-- DEBUGTEXT -->", DEBUGTEXT);

  request->send(200, "text/html", tmp);
}

void handle_OnCSS(AsyncWebServerRequest *request)
{
  WebSerial.println("HTTP Request for: " + request->url());
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