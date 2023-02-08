#include "blind.h"

blind::blind(String mac_addr, byte *key)
{
    this->_mac = new String(mac_addr);
    // this->_key = key;
    this->_pClient = NULL;
    this->_newAngle = 255;
    this->_angle = 254;
    this->_name = new String("UNKNOWN");

    for (int i = 0; i < 6; i++)
    {
        this->_key[i] = key[i];
    }
}

blind::~blind()
{
}

bool blind::connect()
{
    if (this->_pClient == NULL)
    {
        this->_pClient = BLEDevice::createClient();
    }

    if (this->_pClient->isConnected() == false)
    {
        BLEAddress myAddr = BLEAddress(this->_mac->c_str());

        this->_pClient->connect(myAddr, BLE_ADDR_TYPE_RANDOM);
    }
    return this->_pClient->isConnected();
}

void blind::disconnect()
{
    this->_pClient->disconnect();
    this->_pClient = NULL;
}

bool blind::isConnected()
{
    return this->_pClient->isConnected();
}

void blind::setAngle(int newAngle)
{
    this->_newAngle = newAngle;
}

int blind::getAngle()
{
    return this->_angle;
}

void blind::unlock()
{
    if (!this->connect())
    {
        WebSerial.println("blind::unlock() - Error - Unable to connect");
    }

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID.c_str());
    if (tmpService == NULL)
    {
        WebSerial.println("blind::unlock() - Could not get service");
    }

    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_KEY_UUID.c_str());

    if (tmpCharact == NULL)
    {
        WebSerial.println("blind::unlock() - Could not get charecteristic");
    }

    tmpCharact->writeValue(this->_key, 6, true);
}

String *blind::mac()
{
    return this->_mac;
}

String *blind::key()
{
    return new String((char *)this->_key);
}

String *blind::name()
{
    return this->_name;
}

void blind::refresh()
{
    WebSerial.println("blind::refresh() - Entered Function");

    if (!this->connect())
    {
        WebSerial.println("blind::refresh() - Error - Unable to connect");
    }

    this->unlock();

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID.c_str());
    if (tmpService == NULL)
    {
        WebSerial.println("blind::refresh() - Could not get service");
    }

    // Refresh the Name of the Blind
    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_NAME_UUID.c_str());
    if (tmpCharact == NULL)
    {
        WebSerial.println("blind::refresh() - Could not get charecteristic for NAME");
    }

    String tmpName = String(tmpCharact->readValue().c_str());

    if (tmpName.length() > 4)
    {
        tmpName = tmpName.substring(4);
    }
    delete this->_name;
    this->_name = new String(tmpName);
    WebSerial.println("NAME = " + *(this->_name));

    // Refresh the Angle of the Blind
    this->connect();
    tmpCharact = tmpService->getCharacteristic(this->_ANGLE_UUID.c_str());
    if (tmpCharact == NULL)
    {
        WebSerial.println("blind::refresh() - Could not get charecteristic for ANGLE");
    }

    this->_angle = tmpCharact->readUInt8();
    WebSerial.println("ANGLE = " + String(this->_angle));

    if (this->_angle != this->_newAngle)
    {
        // Need to write newAngle to blind
        this->_writeAngle();
    }

    this->disconnect();

    WebSerial.println("blind::refresh() - Exit Function");
}

void blind::_writeAngle()
{
    WebSerial.println("blind::_writeAngle() - Entered Function");

    if (this->_newAngle == 255)
    {
        this->_newAngle = this->_angle;
        WebSerial.println("blind::_writeAngle() - First refresh - NOT SETTING ANGLE");

        return;
    }

    if (!this->connect())
    {
        WebSerial.println("blind::_writeAngle() - Error - Unable to connect");
    }

    this->unlock();

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID.c_str());
    if (tmpService == NULL)
    {
        WebSerial.println("blind::_writeAngle() - Could not get service");
    }

    // Refresh the Name of the Blind
    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_ANGLE_UUID.c_str());
    if (tmpCharact == NULL)
    {
        WebSerial.println("blind::_writeAngle() - Could not get charecteristic for ANGLE");
    }
    tmpCharact->writeValue(this->_newAngle);
    this->_angle = _newAngle;
    
    this->disconnect();

    WebSerial.println("blind::_writeAngle() - Exit Function");
}