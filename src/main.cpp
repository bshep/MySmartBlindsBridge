#define ENABLE_OTA

#include "main.h"
#include "blind.h"
#include "config.h"

char hostName[32] = "msb1";
char ssid[32];
char passphrase[64];

bool BlindsRefreshNow = true;

AsyncWebServer webServer(80);
AsyncWebServer debugServer(88);

blind *blindsList[10];
HACover *coverList[10];
int blindCount = 0;

Timer<10> timer;

String DEBUGTEXT;

#define BROKER_ADDR "192.168.2.222"
WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
byte deviceMAC[18];

void onWebSerial_recvMsg(uint8_t *data, size_t len)
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

  if (d == "PRINTMAC")
  {
    WebSerial.print("DEVICE MAC : ");
    for (int i = 0; i < 18; i++)
    {
      char byteStr[10];
      sprintf(byteStr, "%x", deviceMAC[i]);
      WebSerial.print(byteStr);
    }
    WebSerial.print("\n");
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
      coverList[i]->setName(blindsList[i]->name());

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
    Serial.println("Wifi Connecting...");
  }

  WebSerial.begin(&debugServer);
  WebSerial.msgCallback(onWebSerial_recvMsg);

#ifdef ENABLE_OTA
  ArduinoOTA.begin();

  ArduinoOTA.onStart([]()
                     { WebSerial.println("OTA Begin"); 
                     BlindsRefreshNow = false; });

  ArduinoOTA.onEnd([]()
                   { WebSerial.println("OTA Finished"); });

#endif

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
  mqtt.begin(BROKER_ADDR);

  readBlindsConfig();

  timer.every(1000, onRefreshBlinds);
}

void loop()
{
  timer.tick();
  ArduinoOTA.handle();
  mqtt.loop();
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
    handle_HTTPArgs(request);
    request->redirect("/");
    return;
  }

  String tmp = readFileIntoString("/index.html");
  String tmpDevices = "";
  blind *myblind;

  // DEBUGTEXT = "<h2>SSID: " + String(ssid) + "</h2>";
  // DEBUGTEXT += "<h2>PASSPHRASE: " + String(passphrase) + "</h2>";

  tmpDevices += "<h2>Total Found " + String(blindCount) + "</h2>";
  tmpDevices += "<table class=\"table\">";
  tmpDevices += "<thead><tr><td>Name</td><td>Angle</td><td>Controls</td></tr></thead>";

  for (int i = 0; i < blindCount; i++)
  {
    myblind = blindsList[i];

    tmpDevices += "<tr>";
    tmpDevices += "<td>" + String(myblind->name()) + "</td>";
    tmpDevices += "<td>" + String(myblind->getAngle()) + "</td>";
    tmpDevices += "<td><a class=\"button\" href=\"/?cmd=open&mac=" + String(myblind->mac()) + "\">Open</a>";
    tmpDevices += "<a class=\"button\" href=\"/?cmd=close&mac=" + String(myblind->mac()) + "\">Close</a></td>";

    tmpDevices += "</tr>";
  }
  tmpDevices += "</table>";

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