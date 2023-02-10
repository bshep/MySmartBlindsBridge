#include "blind.h"

blind::blind(char *mac_addr, byte *key)
{
    strncpy(this->_mac, mac_addr, 18);
    this->_pClient = NULL;
    this->_newAngle = 255;
    this->_angle = 254;
    strcpy(this->_name, "UNKNOWN");
    this->_needUnlock = true;
    this->sensors = new blindSensors();

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
    // WebSerial.println("blind::connect() - Entered Function");

    if (this->_pClient == NULL)
    {
        this->_pClient = BLEDevice::createClient();
    }

    if (this->_pClient->isConnected() == false)
    {
        // WebSerial.print("blind::connect() - attempting to connect to: mac - ");
        // WebSerial.println(this->_mac);
        BLEAddress myAddr = BLEAddress(std::string(this->_mac));

        this->_pClient->connect(myAddr, BLE_ADDR_TYPE_RANDOM);
        this->_needUnlock = true;
    }
    // WebSerial.println("blind::connect() - Exit Function");

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
    if (!this->_needUnlock)
    {
        // WebSerial.println("blind::unlock() - already unlocked");
        return;
    }

    if (!this->connect())
    {
        // WebSerial.println("blind::unlock() - Error - Unable to connect");
    }

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID);
    if (tmpService == NULL)
    {
        // WebSerial.println("blind::unlock() - Could not get service");
    }

    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_KEY_UUID);

    if (tmpCharact == NULL)
    {
        // WebSerial.println("blind::unlock() - Could not get charecteristic");
    }

    tmpCharact->writeValue(this->_key, 6, true);
    this->_needUnlock = false;
}

char *blind::mac()
{
    return this->_mac;
}

byte *blind::key()
{
    return this->_key;
}

char *blind::name()
{
    return this->_name;
}

void blind::refresh()
{
    // WebSerial.println("blind::refresh() - Entered Function");

    this->unlock();

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID);
    if (tmpService == NULL)
    {
        // WebSerial.println("blind::refresh() - Could not get service");
        return;
    }

    // Refresh the Name of the Blind
    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_NAME_UUID);
    if (tmpCharact == NULL)
    {
        // WebSerial.println("blind::refresh() - Could not get charecteristic for NAME");
        return;
    }

    std::string tmpName = tmpCharact->readValue();

    strcpy(this->_name, tmpName.c_str() + 4);
    // WebSerial.print("NAME = ");
    // WebSerial.println(this->_name);

    // Refresh the Angle of the Blind
    tmpCharact = tmpService->getCharacteristic(this->_ANGLE_UUID);
    if (tmpCharact == NULL)
    {
        // WebSerial.println("blind::refresh() - Could not get charecteristic for ANGLE");
        return;
    }

    this->_angle = tmpCharact->readUInt8();
    // WebSerial.println("ANGLE = " + String(this->_angle));
    // WebSerial.println(" -- oldAngle = " + String(this->_angle) + " - newAngle - " + String(this->_newAngle));

    if (this->_angle != this->_newAngle)
    {
        // Need to write newAngle to blind
        this->_writeAngle();
    }

    // WebSerial.println("blind::refresh() - Exit Function");
}

void blind::_writeAngle()
{
    // WebSerial.println("blind::_writeAngle() - Entered Function");
    // WebSerial.println(" -- oldAngle = " + String(this->_angle) + " - newAngle - " + String(this->_newAngle));

    if (this->_newAngle == 255)
    {
        this->_newAngle = this->_angle;
        // WebSerial.println("blind::_writeAngle() - First refresh - NOT SETTING ANGLE");

        return;
    }

    this->unlock();

    BLERemoteService *tmpService = this->_pClient->getService(this->_SERVICE_UUID);
    if (tmpService == NULL)
    {
        // WebSerial.println("blind::_writeAngle() - Could not get service");
    }

    BLERemoteCharacteristic *tmpCharact = tmpService->getCharacteristic(this->_ANGLE_UUID);
    if (tmpCharact == NULL)
    {
        // WebSerial.println("blind::_writeAngle() - Could not get charecteristic for ANGLE");
    }
    tmpCharact->writeValue((uint8_t)this->_newAngle);
    this->_angle = _newAngle;

    // WebSerial.println("blind::_writeAngle() - Exit Function");
}

void blind::refreshSensors()
{
    this->unlock();

    std::string sensorValue = this->_pClient->getValue(this->_SERVICE_UUID, this->_SENSORS_UUID);

    this->sensors->parseSensorData((byte *)sensorValue.c_str());

    // WebSerial.println("Sensors: ");
    // WebSerial.println(" - batteryPercentage=" + String(this->sensors->getBatteryPercentage()));
    // WebSerial.println(" - _batteryVoltage=" + String(this->sensors->getBatteryVoltage()));
    // WebSerial.println(" - _batteryCharge=" + String(this->sensors->getBatteryCharge()));
    // WebSerial.println(" - _solarPanelVoltage=" + String(this->sensors->getSolarPanelVoltage()));
    // WebSerial.println(" - _interiorTemp=" + String(this->sensors->getInteriorTemp()));
    // WebSerial.println(" - _batteryTemp=" + String(this->sensors->getBatteryTemp()));
    // WebSerial.println(" - _rawLightValue=" + String(this->sensors->getRawLightValue()));
}