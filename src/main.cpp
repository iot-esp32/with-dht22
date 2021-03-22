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
         * Miliseconds to sleep if all connectivity is ok
         */
        uint32_t sleep_time = 2000;

        /*
         * Senzor name
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

        WiFiClass * h;
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

        PubSubClient * h;
    } mqtt;
} app;

time_t time_get_unix() {
    time_t rawtime;

    time(&rawtime);
    return rawtime;
}

void printLocalTime() {
    time_t rawtime;
    struct tm * t;

    time(&rawtime);
    t = localtime(&rawtime);
    Serial.println(asctime(t));
}

void _init_mqtt() {    
    static PubSubClient * h;
    static WiFiClient wifi_client;

    h = new PubSubClient();

    h->setServer(app.mqtt.server, app.mqtt.server_port);
    h->setClient(wifi_client);

    app.mqtt.h = h;
}

void _connect_to_mqtt() {
    boolean connected;

    connected = app.mqtt.h->connect(
                              app.global.sensor_name,
                              app.mqtt.auth_user,
                              app.mqtt.auth_pass
    );

    if (connected == false) {
        log_e("could not connect to MQTT server %s:%u", app.mqtt.server, app.mqtt.server_port);
    }
    else {
        log_i("connected to MQTT server");
    }
}

void _init_serial() {
    Serial.begin(app.serial.baud);
    Serial.println();

    delay(app.serial.initial_wait);
}

void wifi_connect() {
    static WiFiClass * wifi_h = NULL;

    if ( wifi_h != NULL) {
        wifi_h = new WiFiClass();
        app.wlan.h = wifi_h;
    }

    log_i("attempting to connect to wireless network %s", app.wlan.ssid);
    wifi_h->begin(app.wlan.ssid, app.wlan.pass);
        
    while (wifi_h->status() != WL_CONNECTED) {
      log_i("still nothing. sleeping for half a second ...");
      delay(500);
    }

    log_i("connected to %s", app.wlan.ssid);
}

void wifi_disconnect() {
    app.wlan.h->disconnect();
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
    wifi_disconnect();
}

void loop() {
    Serial.println("###############################################################################");
    printLocalTime();
    delay(1000 * 10);
    return;


    wifi_connect();
    Serial.println("sleeping 1s");
    delay(1000);
    Serial.println("done");
    wifi_disconnect();

    delay(1000 * 60 * 2);
}