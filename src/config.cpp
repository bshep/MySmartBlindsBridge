
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
extern HASensorNumber *sensorList[HA_MAXDEVICES];
extern HABinarySensor *chargingSensorList[HA_MAXDEVICES];

char coverID[HA_MAXDEVICES][ID_MAXLEN];
char batterySensorID[HA_MAXDEVICES][ID_MAXLEN];
char chargingSensorID[HA_MAXDEVICES][ID_MAXLEN];

extern int blindCount;
extern char hostName[];
extern char ssid[];
extern char passphrase[];
extern bool BlindsRefreshNow;

void decode_base64(String &input, String &output)
{
    unsigned char tmpString[200];

    decode_base64((unsigned char *)input.c_str(), tmpString);

    output = F(tmpString);
}

void decodeBlindsMac(String &encodedMac, SafeString &decMac)
{
    String decodedMac;
    String decodedMacBinary;
    decode_base64(encodedMac, decodedMacBinary);

    for (int i = decodedMacBinary.length() - 1; i >= 0; i--)
    {
        decMac.printf("%x", decodedMacBinary[i]);
        if (i != 0)
            decMac += ":";
    }
}

void passkeyToString(byte *passkey, SafeString &retVal)
{
    retVal.clear();

    for (int i = 0; i < 6; i++)
    {
        retVal.printf("%x", passkey[i]);
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
    DEBUG_PRINTLN("Blind index " + blindIndex);

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
    DEBUG_PRINTLN("Blind index " + blindIndex);
    DEBUG_PRINT("Set Position to: ");
    DEBUG_PRINTLN(position.toInt8());

    int blindPos = (100 - position.toInt8()) + 100;

    blindsList[blindIndex]->setAngle(blindPos);
}

void readBlindsConfig()
{
    cSF(decMac, 17, "");
    byte decPasskey[6];

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
        String encPasskey = currLine.substring(currLine.indexOf(':') + 1);

        Serial.println("BlindsConfig - mac - " + encMac + " - passkey - " + encPasskey);

        decMac.clear();
        decodeBlindsMac(encMac, decMac);
        decode_base64((byte *)encPasskey.c_str(), decPasskey);

        Serial.print(" -- Decoded MAC: ");
        Serial.println(decMac);
        Serial.print(" -- Decoded Passkey: ");
        {
            cSF(decPasskeyString, 20);
            passkeyToString(decPasskey, decPasskeyString);
            Serial.println(decPasskeyString.c_str());
        }

        blindsList[blindCount] = new blind(decMac.c_str(), decPasskey);
        blindsList[blindCount]->refresh();
        // blindCount++;
        // continue;

        char *deviceID = coverID[blindCount];
        cSFPS(sfDeviceID, deviceID, ID_MAXLEN);
        sfDeviceID.printf("msb_Cover_%i", blindCount);

        Serial.print("Creating new HACover with name - \"" + String(deviceID));
        Serial.println("\"");
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