#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <WebSerial.h>

extern String DEBUGTEXT;

class blind
{
private:
    // Blind Data
    String *_mac;
    byte _key[6];
    String *_name;
    int _angle;
    int _newAngle; //Used when updating blind


    // UUIDs to control the blinds
    String _KEY_UUID =     "00001409-1212-efde-1600-785feabcd123";
    String _ANGLE_UUID =   "00001403-1212-efde-1600-785feabcd123";
    String _SERVICE_UUID = "00001400-1212-efde-1600-785feabcd123";
    String _NAME_UUID =    "00001401-1212-efde-1600-785feabcd123";

    BLEClient*  _pClient;

    void _writeAngle();
    bool connect();
    void disconnect();
    bool isConnected();
    void unlock();
public:
    blind(String mac_addr, byte*key);
    ~blind();
    String *mac();
    String *key();
    String *name();
    void refresh();

    void setAngle(int newAngle);
    int getAngle();

};