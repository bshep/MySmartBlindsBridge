# MySmartBlindsBridge

## Requirements
- VSCode
- PlatformIO
- Home Assistant insall
- MQTT broker
- ESP32 w bluetooth 5 support 

## Setup
- ~~Edit wifi.cfg and add your base64 encoded ssid and passphrase~~ - No added config with AP mode so you can connect to ssid: *WiFiConfig* with password *12345678* and set your network settings there
- Edit blinds.cfg and add your blinds, you will need your encodedMac and encodedKey as given by running https://github.com/docBliny/smartblinds-client/blob/master/example.py with your credentials
- Edit main.cpp and set your BROKER_ADDR to your MQTT broker

## Usage
- you can browse to http://msb1.local and open/close the blinds manually
- you can browse to http://msb1.local:88/webserial and there is a serial monitor you can look at
- your blinds should show up on HA automatically, ~~although i havent figured out how to make the names show up properly~~ name now updates correctly
- you can also see battery charge level and wether blinds are charging
