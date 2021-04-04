# Home IoT skeleton application

This application's purpose is to provide a working skeleton for those who want to build a home IoT <something> to play with. This application includes:
1. code for ESP32 that:
   1. reads sensors data periodically
   1. sends that data to a MQTT server over wireless connection using JSON format
1. a Python daemon that reads data from MQTT and sends that data into an InfluxDB database
1. a sample web page with latest sensors reading

### Supported sensors
1. DHT32


### ToDo
1. implement a single send over multiple readings for saving energy. Ex: 1 minute readings sent every 10 minutes