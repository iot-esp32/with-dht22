# Home IoT skeleton application

## About
This project is a small code base that can be used for reading DHT22 sensors on ESP32 boards and sending the results to a MQTT server in JSON format.

Included:
1. code for ESP32 that:
   1. reads sensors data periodically
   1. sends that data to a MQTT server over wireless connection using JSON format
1. a Python daemon that reads data from MQTT and sends that data into an InfluxDB database
1. a sample web page with latest sensors reading

## Supported sensors
1. DHT32

## How to build
I used PlatformIO as IDE. Why ? Because I like it. Steps to build (without UI):
1. install libraries
```bash
platformio lib install ArduinoJson
platformio lib install DHTStable
platformio lib install PubSubClient
platformio lib install "Uptime Library"
```
2. adapt build_flags from [platformio.ini](https://github.com/iot-esp32/with-dht22/blob/main/platformio.ini)
3. update [sample-env.sh](https://github.com/iot-esp32/with-dht22/blob/main/sample-env.sh)
4. build and run the project
```bash
source sample-env.sh
platformio run -t upload && platformio device monitor --baud 115200
```

## ToDo
1. implement a single send over multiple readings for saving energy. Ex: 1 minute readings sent every 10 minutes%
