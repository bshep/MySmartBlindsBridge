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

static char hostName[32] = "msb1";
char ssid[32];
char passphrase[64];

static std::string BlindBLEName = "SmartBlind_DFU";

int scanTime = 5; // In seconds
BLEScan *pBLEScan;
BLEScanResults foundDevices;
bool BLERefreshNow = false;
bool BlindsRefreshNow = false;

AsyncWebServer webServer(80);
AsyncWebServer debugServer(88);

blind *blindsList[10];
int blindCount = 0;

bool scanDone = false;

Timer<10> timer;

String DEBUGTEXT;

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
  if (BlindsRefreshNow)
  {
    BlindsRefreshNow = false;
    for (int i = 0; i < blindCount; i++)
    {
      blindsList[i]->refresh();
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

String decode_base64(String input)
{
  unsigned char tmpString[200] = "";
  decode_base64((unsigned char *)input.c_str(), tmpString);

  return String((char *)tmpString);
}

void readWIFIConfig()
{
  File wifiConfigFile = LittleFS.open("/wifi.cfg", "r");
  unsigned char tmpString[200] = "";

  while (wifiConfigFile.available())
  {
    String currLine = wifiConfigFile.readStringUntil('\n');
    if (currLine.startsWith("SSID:"))
    {
      decode_base64((const unsigned char *)currLine.substring(currLine.indexOf(":") + 1).c_str(), (unsigned char *)ssid);
      // ssid = std::string(decode_base64(currLine.substring(currLine.indexOf(":") + 1)).c_str());
      Serial.print("SSID: ");
      Serial.println(ssid);
    }

    if (currLine.startsWith("passphrase:"))
    {
      decode_base64((const unsigned char *)currLine.substring(currLine.indexOf(":") + 1).c_str(), (unsigned char *)passphrase);
      // passphrase = std::string(decode_base64(currLine.substring(currLine.indexOf(":") + 1)).c_str());
      Serial.print("passphrase: ");
      Serial.println(passphrase);
    }

    if (currLine.startsWith("hostname:"))
    {
      strcpy(hostName, currLine.substring(currLine.indexOf(":") + 1).c_str());
      // hostName = std::string(currLine.substring(currLine.indexOf(":") + 1).c_str());
      Serial.print("hostname: ");
      Serial.println(hostName);
    }
  }
  wifiConfigFile.close();
}

void decodeBlindsMac(String encodedMac, char *decMac)
{
  String decodedMac;
  String decodedMacBinary;
  char tmpStr[20] = "";
  decodedMacBinary = decode_base64(encodedMac);

  // Serial.println(decodedMac);
  for (int i = decodedMacBinary.length() - 1; i >= 0; i--)
  {
    sprintf(tmpStr, "%x", decodedMacBinary[i]);

    decodedMac += String(tmpStr);
    if (i != 0)
      decodedMac += ":";
  }

  strncpy(decMac, decodedMac.c_str(), 18);
}

String passkeyToString(byte *passkey)
{
  char tmpStr[10] = "";
  String passkeyString = "";

  for (int i = 0; i < 6; i++)
  {
    sprintf(tmpStr, "%x", passkey[i]);
    passkeyString += String(tmpStr);
  }

  return passkeyString;
}

void readBlindsConfig()
{
  File blindsConfigFile = LittleFS.open("/blinds.cfg", "r");
  unsigned char tmpString[200] = "";

  while (blindsConfigFile.available())
  {
    String currLine = blindsConfigFile.readStringUntil('\n');
    String encMac = currLine.substring(0, currLine.indexOf(':'));
    // char currMac[17];
    // strncpy(currMac,currLine.substring(0,currLine.indexOf(':')).c_str(),8);
    String encPasskey = currLine.substring(currLine.indexOf(':') + 1);

    Serial.println("BlindsConfig - mac - " + encMac + " - passkey - " + encPasskey);

    char decMac[18];
    decodeBlindsMac(encMac, decMac);
    byte decPasskey[6];
    decode_base64((byte *)encPasskey.c_str(), decPasskey);

    Serial.print(" -- Decoded MAC: ");
    Serial.println(decMac);
    Serial.println(" -- Decoded Passkey: " + passkeyToString(decPasskey));

    blindsList[blindCount] = new blind(decMac, decPasskey);
    blindCount++;
  }
  blindsConfigFile.close();
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
                     { WebSerial.println("OTA Begin"); });

  // ArduinoOTA.onProgress([](int totalWritten, int size)
  //                       {
  //   static int lastPercent = 0;
  //   int percent = totalWritten*100 / size;

  //   if (percent > lastPercent) {
  //       lastPercent = percent;

  //       WebSerial.println("OTA Progress - Written " + String(totalWritten) + " of " + String(size) + " - " + String(lastPercent) + "\% Done");
  //   } });

  ArduinoOTA.onEnd([]()
                   { WebSerial.println("OTA Finished"); });

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

  timer.every(1000, onRefreshBLEScan);
  readBlindsConfig();
  timer.every(1000, onRefreshBlinds);
}

void loop()
{
  timer.tick();
  ArduinoOTA.handle();
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

  BLERefreshNow = true;
}

void handle_Args(AsyncWebServerRequest *request)
{
  int numArgs = request->args();

  if (numArgs > 0)
  {
    Serial.println("Got some args");
    for (int i = 0; i < numArgs; i++)
    {
      String argName = request->argName(i);
      String argValue = request->arg(i);
      WebSerial.println("ArgName = " + argName + " : ArgValue = " + argValue);

      if (argName == "cmd")
      {
        String mac = request->arg(i + 1);
        if (argValue == "open")
        {
          for (int j = 0; j < blindCount; j++)
          {
            if (strcmp(blindsList[i]->mac(), mac.c_str()) == 0)
            {
              blindsList[i]->setAngle(100);
            }
          }
        }
        else if (argValue == "close")
        {
          for (int j = 0; j < blindCount; j++)
          {
            if (strcmp(blindsList[i]->mac(), mac.c_str()) == 0)
            {
              blindsList[i]->setAngle(200);
            }
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

  DEBUGTEXT = "<h2>SSID: " + String(ssid) + "</h2>";
  DEBUGTEXT += "<h2>PASSPHRASE: " + String(passphrase) + "</h2>";

  tmpDevices += "<h2>Total Found " + String(blindCount) + "</h2>";

  for (int i = 0; i < blindCount; i++)
  {
    myblind = blindsList[i];
    String isConnected = "NA";
    //   if (myblind->isConnected())
    //   {
    //     isConnected = "Yes";
    //   }
    //   else
    //   {
    //     isConnected = "No";
    //   }

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