
#include <config.h>
#ifndef MSB_BASE64
#define MSB_BASE64
#include <base64.hpp>
#endif

#include "blind.h"

// #define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define DEBUG_PRINT(x) WebSerial.print(x)
#define DEBUG_PRINTLN(x) WebSerial.println(x)
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
extern char hostName[];
extern char ssid[];
extern char passphrase[];
extern bool BlindsRefreshNow;

String decode_base64(String input)
{
    unsigned char tmpString[200] = "";
    decode_base64((unsigned char *)input.c_str(), tmpString);

    return String((char *)tmpString);
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

    strncpy(decMac, decodedMac.c_str(), HA_MACLENGTH);
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
        Serial.println("readBlindsConfig(): file blinds.cfg does not exist");
        return;
    }
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

        char decMac[HA_MACLENGTH];
        decodeBlindsMac(encMac, decMac);
        byte decPasskey[6];
        decode_base64((byte *)encPasskey.c_str(), decPasskey);

        Serial.print(" -- Decoded MAC: ");
        Serial.println(decMac);
        Serial.print(" -- Decoded Passkey: ");
        {
            String decPasskeyText;
            passkeyToString(decPasskey, decPasskeyText);
            Serial.println(decPasskeyText);
        }

        blindsList[blindCount] = new blind(decMac, decPasskey);
        blindsList[blindCount]->refresh();

        char *deviceID = coverID[blindCount];
        sprintf(deviceID, "msb_Cover_%i", blindCount);

        Serial.println("Creating new HACover with name - \"" + String(deviceID) + "\"");
        coverList[blindCount] = new HACover(deviceID, HACover::PositionFeature);

        coverList[blindCount]->onCommand(onCoverCommand);
        coverList[blindCount]->onPosition(onCoverPosition);

        coverList[blindCount]->setIcon("mdi:blinds-horizontal");
        coverList[blindCount]->setName(blindsList[blindCount]->name());
        coverList[blindCount]->setAvailability(false);
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
        chargingSensorList[blindCount]->setDeviceClass("battery_charging");

        blindCount++;
    }

    blindsConfigFile.close();
}