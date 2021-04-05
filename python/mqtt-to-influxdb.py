#!/usr/bin/env python3

"""
For this to work you need influxdb-python and paho-mqtt. On Ubuntu,
```bash
sudo apt-get install python3-influxdb
sudo pip3 install paho-mqtt
```
"""

from influxdb import InfluxDBClient
import paho.mqtt.client as mqtt
import json, os

def sensor_callback(client, userdata, msg):
    print(f"The following message just arrived on {msg.topic}: {str(msg.payload.decode('utf-8'))}")
  
    try:
        j_doc = json.loads(msg.payload.decode("utf-8"))
    except json.decoder.JSONDecodeError:
        print("something is wrong with this JSON ", msg.payload)
        return

    influx_msg = {
          'time'       : j_doc['time_read'] * 1000000000,
          'measurement': 'sensors',
          'fields'     : {
            'temp'        : float(j_doc['temperature']),
            'humidity'    : float(j_doc["humidity"]),
            'uptime'      : int(j_doc['uptime']['seconds']),
            'time_of_read': int(j_doc['time_read'])
          },
          'tags': j_doc['sensor']
    }

    client = InfluxDBClient(
                 os.environ['INFLUXDB_HOST'],
                 os.environ['INFLUXDB_PORT'],
                 os.environ['INFLUXDB_USER'],
                 os.environ['INFLUXDB_PASS'],
                 os.environ['INFLUXDB_DBNAME']
    )
    client.write_points([influx_msg])

def main():
    mqtt_env_vars = ['MQTT_USER', 'MQTT_PASS', 'MQTT_SERVER', 'MQTT_TOPIC']
    influx_env_vars = ['INFLUXDB_HOST', 'INFLUXDB_PORT', 'INFLUXDB_USER', 'INFLUXDB_PASS', 'INFLUXDB_DBNAME']

    for p_name in mqtt_env_vars:
        try:
            os.environ[p_name]
        except KeyError:
            print(f"Error while trying to get environment variable {p_name}. The following variables must be defined: {' '.join(mqtt_env_vars)}")
            os.sys.exit(1)

    for p_name in influx_env_vars:
        try:
            os.environ[p_name]
        except KeyError:
            print(f"Error while trying to get environment variable {p_name}. The following variables must be defined: {' '.join(influx_env_vars)}")
            os.sys.exit(1)
    
    mqtt_h = mqtt.Client(client_id = "mqtt-to-influxdb")
    mqtt_h.on_message = sensor_callback

    mqtt_h.username_pw_set(username=os.environ['MQTT_USER'], password=os.environ['MQTT_PASS'])
    mqtt_h.connect(os.environ['MQTT_SERVER'], 1883, 60)
    mqtt_h.subscribe(os.environ['MQTT_TOPIC'], 0)
    mqtt_h.loop_forever()
main()