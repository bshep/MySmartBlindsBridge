
#include <config.h>
#ifndef MSB_BASE64
#define MSB_BASE64
#include <base64.hpp>
#endif

#include "blind.h"

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define DEBUG_PRINT(x)  \
    WebSerial.print(x); \
    Serial.print(x)
#define DEBUG_PRINTLN(x)  \
    WebSerial.println(x); \
    Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

extern blind *blindsList[];
extern HACover *coverList[];
extern HASensorNumber *sensorList[];
extern HABinarySensor *chargingSensorList[];

char coverID[HA_MAXDEVICES][ID_MAXLEN];
char batterySensorID[HA_MAXDEVICES][ID_MAXLEN];
char chargingSensorID[HA_MAXDEVICES][ID_MAXLEN];

extern int blindCount;
extern String blindsConfig;
extern char hostName[];
extern char ssid[];
extern char passphrase[];
extern bool BlindsRefreshNow;

void decode_base64(String input, String &output)
{
    char tmpString[200] = "";
    decode_base64((unsigned char *)input.c_str(), (unsigned char *)tmpString);

    output += tmpString;
    // return String((char *)tmpString);
}

void decodeBlindsMac(const String encodedMac, char *decMac)
{
    String decodedMac;
    String decodedMacBinary;
    char tmpStr[20] = "";
    decode_base64(encodedMac, decodedMacBinary);

    // Serial.println(decodedMac);
    for (int i = decodedMacBinary.length() - 1; i >= 0; i--)
    {
        sprintf(tmpStr, "%x", decodedMacBinary[i]);

        decodedMac += String(tmpStr);
        if (i != 0)
            decodedMac += ":";
    }

    decodedMac.toCharArray(decMac, HA_MACLENGTH);
    // strncpy(decMac, decodedMac.c_str(), HA_MACLENGTH);
}

void passkeyToString(byte *passkey, String &passkeyString)
{
    passkeyString = "";

    for (int i = 0; i < 6; i++)
    {
        passkeyString += String(passkey[i], 16);
    }
}

int findBlindIndexFromHACover(HACover *cover)
{
    for (int i = 0; i < blindCount; i++)
    {
        if (cover == coverList[i])
        {
            return i;
        }
    }
    return -1;
}

void onCoverCommand(HACover::CoverCommand cmd, HACover *sender)
{
    int blindIndex = findBlindIndexFromHACover(sender);
    DEBUG_PRINTLN("Blind index " + String(blindIndex));

    if (blindIndex >= 0)
    {

        if (cmd == HACover::CommandOpen)
        {
            DEBUG_PRINTLN("Command: Open");
            sender->setState(HACover::StateOpen);    // report state back to the HA
            sender->setState(HACover::StateStopped); // report state back to the HA
            blindsList[blindIndex]->setAngle(100);
            BlindsRefreshNow = true;
        }
        else if (cmd == HACover::CommandClose)
        {
            DEBUG_PRINTLN("Command: Close");
            sender->setState(HACover::StateClosed);  // report state back to the HA
            sender->setState(HACover::StateStopped); // report state back to the HA
            blindsList[blindIndex]->setAngle(200);
            BlindsRefreshNow = true;
        }
        else if (cmd == HACover::CommandStop)
        {
            DEBUG_PRINTLN("Command: Stop");
            sender->setState(HACover::StateStopped); // report state back to the HA
        }
    }
    else
    {
        DEBUG_PRINTLN("Could not find a blind that matches that HACover");
    }
    // Available states:
    // HACover::StateClosed
    // HACover::StateClosing
    // HACover::StateOpen
    // HACover::StateOpening
    // HACover::StateStopped

    // You can also report position using setPosition() method
}

void onCoverPosition(HANumeric position, HACover *sender)
{
    int blindIndex = findBlindIndexFromHACover(sender);
    DEBUG_PRINTLN("Blind index " + String(blindIndex));
    DEBUG_PRINT("Set Position to: ");
    DEBUG_PRINTLN(position.toInt8());

    int blindPos = (100 - position.toInt8()) + 100;

    blindsList[blindIndex]->setAngle(blindPos);
}

void readBlindsConfig()
{
    if (!LittleFS.exists("/blinds.cfg"))
    {
        DEBUG_PRINTLN("readBlindsConfig(): file blinds.cfg does not exist");
        return;
    }
    File blindsConfigFile = LittleFS.open("/blinds.cfg", "r");
    unsigned char tmpString[200] = "";
    blindsConfig = "";

    while (blindsConfigFile.available())
    {
        String currLine = blindsConfigFile.readStringUntil('\n');
        if (currLine != "")
        {
            unsigned int indexOfColon = currLine.indexOf(':');

            if (indexOfColon >= 0)
            {

                blindsConfig += currLine + "\n";
                String encMac = currLine.substring(0, indexOfColon);
                String encPasskey = currLine.substring(indexOfColon + 1);

                DEBUG_PRINTLN("BlindsConfig - mac - " + encMac + " - passkey - " + encPasskey);

                char decMac[HA_MACLENGTH];
                decodeBlindsMac(encMac, decMac);
                byte decPasskey[6];
                decode_base64((byte *)encPasskey.c_str(), decPasskey);

                DEBUG_PRINT(" -- Decoded MAC: ");
                DEBUG_PRINTLN(decMac);
                DEBUG_PRINT(" -- Decoded Passkey: ");

#ifdef ENABLE_DEBUG
                {
                    String decPasskeyText;
                    passkeyToString(decPasskey, decPasskeyText);
                    DEBUG_PRINTLN(decPasskeyText);
                }
#endif

                blindsList[blindCount] = new blind(decMac, decPasskey);
                blindsList[blindCount]->refresh();

                char *deviceID = coverID[blindCount];
                sprintf(deviceID, "msb_Cover_%i", blindCount);

                DEBUG_PRINTLN("Creating new HACover with name - \"" + String(deviceID) + "\"");
                coverList[blindCount] = new HACover(deviceID, HACover::PositionFeature);

                coverList[blindCount]->onCommand(onCoverCommand);
                // coverList[blindCount]->onPosition(onCoverPosition);

                coverList[blindCount]->setIcon("mdi:blinds-horizontal");
                coverList[blindCount]->setName(blindsList[blindCount]->name());
                // coverList[blindCount]->setAvailability(false);
                coverList[blindCount]->setAvailability(true);
                coverList[blindCount]->setState(HACover::StateStopped); // report state back to the HA

                deviceID = batterySensorID[blindCount];
                sprintf(deviceID, "%s_batt", blindsList[blindCount]->name(), blindCount);
                sensorList[blindCount] = new HASensorNumber(deviceID);
                sensorList[blindCount]->setDeviceClass("battery");
                sensorList[blindCount]->setUnitOfMeasurement("%");
                sensorList[blindCount]->setValue(0);
                sensorList[blindCount]->setName(blindsList[blindCount]->name());

                deviceID = chargingSensorID[blindCount];
                sprintf(deviceID, "%s_charge", blindsList[blindCount]->name(), blindCount);
                chargingSensorList[blindCount] = new HABinarySensor(deviceID);
                chargingSensorList[blindCount]->setName(blindsList[blindCount]->name());
                chargingSensorList[blindCount]->setState(false);
                chargingSensorList[blindCount]->setDeviceClass("battery_charging");

                blindCount++;
            }
            else
            {
                Serial.println("readBlindsConfig(): line has no separator - " + currLine);
            }
        }
    }

    blindsConfigFile.close();
}

void readWifiConfig(String &wifiSSID, String &wifiPassphrase, String &wifiHostname, String &brokerAddress)
{
    Serial.println("readWifiConfig(): enter function");
    if (!LittleFS.exists("/wifi.cfg"))
    {
        Serial.println("readWifiConfig(): file wifi.cfg does not exist");
        return;
    }

    File wifiConfigFile = LittleFS.open("/wifi.cfg", "r");
    unsigned char tmpString[200] = "";

    while (wifiConfigFile.available())
    {
        String currLine = wifiConfigFile.readStringUntil('\n');

        if (currLine != "")
        {
            unsigned int indexOfColon = currLine.indexOf(':');

            if (indexOfColon >= 0)
            {
                String key = currLine.substring(0, indexOfColon);
                String encValue = currLine.substring(indexOfColon + 1);
                String decValue;
                decode_base64(encValue, decValue);

                if (key == "SSID")
                {
                    wifiSSID = decValue;
                    Serial.println("readWifiConfig(): key = " + key + " - value = " + decValue);
                }

                if (key == "passphrase")
                {
                    wifiPassphrase = decValue;
                    Serial.println("readWifiConfig(): key = " + key + " - value = " + decValue);
                }

                if (key == "hostname")
                {
                    wifiHostname = encValue;
                    Serial.println("readWifiConfig(): key = " + key + " - value = " + encValue);
                }

                if (key == "brokerAddress")
                {
                    brokerAddress = encValue;
                    Serial.println("readWifiConfig(): key = " + key + " - value = " + encValue);
                }
            }
            else
            {
                Serial.println("readWifiConfig(): line has no separator - " + currLine);
            }
        }
    }

    wifiConfigFile.close();
}