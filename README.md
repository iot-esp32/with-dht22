# About
This project is a small code base that can be used for reading DHT22 sensors on ESP32 boards and sending the results to a MQTT server in JSON format.

## How to build
I used PlatformIO as IDE. Why ? Because I like it. Steps to build (without UI):
1. install libraries
```bash
platformio lib install ArduinoJson
platformio lib install DHTStable
platformio lib install PubSubClient
```
2. adapt build_flags from [platformio.ini](https://github.com/iot-esp32/with-dht22/blob/main/platformio.ini)
3. update [sample-env.sh](https://github.com/iot-esp32/with-dht22/blob/main/sample-env.sh)
4. build and run the project
```bash
source sample-env.sh
platformio run -t upload && platformio device monitor --baud 115200
```
