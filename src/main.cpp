#include <Arduino.h>
#include <dht.h>
#include <WiFi.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <lwip/apps/sntp.h>

#ifndef __MQTT_SERVER_PORT__
#define __MQTT_SERVER_PORT__ 1883
#endif

struct __app_config {
    struct global {
        /*
         *  Miliseconds to sleep if all connectivity is ok
         */
        uint32_t sleep_time = 2000;

        /*
         *  Sensor name
         */
        const char * sensor_name = "dht22_s01";
    } global;

    struct serial {
        unsigned long baud = 115200;

        //Wait for these ms for serial to become available
        uint32_t initial_wait = 500;
    } serial;

    struct dht {
        const uint8_t pin = 13;
    } dht;

    struct wlan {
        const char * ssid = __WIFI_SSID__;
        const char * pass = __WIFI_PASS__;
    } wlan;

    struct ntp {
        const char * ntpServer = "pool.ntp.org";

        //In seconds
        const uint8_t initial_sleep = 10;
    } ntp;

    struct mqtt {
        const char * server        = __MQTT_SERVER__;
        const uint16_t server_port = __MQTT_SERVER_PORT__;
        const char * auth_user     = __MQTT_USER__;
        const char * auth_pass     = __MQTT_PASS__;

        const char * mqtt_topic    = "s99/dht22_s01";
    } mqtt;
} app;

PubSubClient h_mqtt;
WiFiClient h_wifi;

void mqtt_init() {
    log_d("setting mqtt server and port");
    h_mqtt.setServer(app.mqtt.server, app.mqtt.server_port);

    log_d("setting buffer size to 0 to disable whatever library set as max_size");
    h_mqtt.setBufferSize(0);

    log_d("setting networking client to WiFi");
    h_mqtt.setClient(h_wifi);
}

boolean mqtt_connect() {    
    boolean c_res;

    log_d("connecting to mqtt server %s:%u", app.mqtt.server, app.mqtt.server_port);
    c_res = h_mqtt.connect(
              app.global.sensor_name,
              app.mqtt.auth_user,
              app.mqtt.auth_pass
    );

    if (c_res == false) {
        log_e("could not connect to MQTT server %s:%u", app.mqtt.server, app.mqtt.server_port);
    }
    else {
        log_i("connected to MQTT server");
    }

    return c_res;
}

boolean mqtt_send(char * buffer) {
    boolean c_res;

    c_res = h_mqtt.publish(app.mqtt.mqtt_topic, buffer);

    if (c_res == true) {
        log_d("sent %s to mqtt server", buffer);
    }
    else {
        log_e("could not send %s to mqtt server", buffer);
    }

    return c_res;
}

void mqtt_disconnect() {
    log_d("closing connection to mqtt server");
    h_mqtt.disconnect();
}

time_t time_get_unix() {
    time_t rawtime;

    time(&rawtime);
    return rawtime;
}

char * time_get_ascii() {
    static char buffer[128];
    time_t rawtime;
    struct tm * time_structure;

    bzero(&buffer[0], 128);
    time(&rawtime);
    time_structure = localtime(&rawtime);
    asctime_r(time_structure, &buffer[0]);

    //remove \n from the end
    buffer[strlen(buffer) - 1] = '\0';
    return &buffer[0];
}

void printLocalTime() {
    time_t rawtime;
    struct tm * t;

    time(&rawtime);
    t = localtime(&rawtime);
    Serial.println(asctime(t));
}

void _init_serial() {
    Serial.begin(app.serial.baud);
    Serial.println();

    delay(app.serial.initial_wait);
}

boolean wifi_connect() {
    uint8_t max_retries = 60;
    uint8_t counter = 0;    

    log_i("attempting to connect to wireless network %s", app.wlan.ssid);
    WiFi.begin(app.wlan.ssid, app.wlan.pass);
        
    while (WiFi.status() != WL_CONNECTED) {
      log_i("still nothing. sleeping for half a second ...");

      if (counter++ >= max_retries) {
          log_i("still can't connect. bailing out ...");
          return false;
      }
      delay(500);
    }

    log_i("connected to %s", app.wlan.ssid);
    return true;
}

void wifi_disconnect() {
    WiFi.disconnect();
}

dht * _read_sensor() {
    static dht * sensor = new dht();

    switch (sensor->read22(app.dht.pin)) {
        case DHTLIB_ERROR_CHECKSUM:
            log_e("checksum error reading DHT22 on pin %u", app.dht.pin);
            return NULL;
            break;

        case DHTLIB_ERROR_TIMEOUT:
            log_e("timeout reading DHT22 on pin %u", app.dht.pin);
            return NULL;
            break;
    
        default:
            break;
    }

    return sensor;
}

char * readings_to_json(dht * sensor) {
    StaticJsonDocument<100> json;
    static char buffer[255];

    if (sensor == NULL) {
        return NULL;
    }

    log_v("preparing json document");

    log_v("humidity is %f", sensor->humidity);
    json["humidity"]      = sensor->humidity;
    
    log_v("temperature is %f", sensor->temperature);
    json["temperature"]   = sensor->temperature;

    json["time_read"] = time_get_unix();

    log_v("serializing json");
    memset(&buffer[0], 0, sizeof(buffer) - 1);
    serializeJson(json, &buffer[0], 240);

    return &buffer[0];
}

void time_init() {
    log_i("initializing sntp library");
    sntp_setservername(0, (char *)app.ntp.ntpServer);
    sntp_init();

    log_i("waiting %d seconds to get sntp time to read time", (app.ntp.initial_sleep));
    delay(app.ntp.initial_sleep * 1000);
    log_i("sleeping time is over");
}

void setup() {
    _init_serial();

    wifi_connect();
    time_init();
    mqtt_init();    
    wifi_disconnect();
}

void loop() {
    Serial.println("########## BEGIN LOOP ##########");
    log_i("%s", time_get_ascii());

    if (wifi_connect() == false) {
        return;
    }

    mqtt_connect();
    mqtt_send(readings_to_json(_read_sensor()));
    mqtt_disconnect();
    delay(2000);


    wifi_disconnect();
    delay(1000 * 10);

    log_i("%s", time_get_ascii());
    Serial.println("##########  END LOOP  ##########");
}