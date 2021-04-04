#include <Arduino.h>
#include <dht.h>
#include <WiFi.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <lwip/apps/sntp.h>
#include <uptime.h>
#include <uptime_formatter.h>

#ifndef __MQTT_SERVER_PORT__
#define __MQTT_SERVER_PORT__ 1883
#endif

struct __app_config {
    struct global {
        /*
         *  Miliseconds to sleep between loop() iterations
         */
        uint32_t sleep_time = 1000 * 60 * 1;
    } global;

    struct sensor {
        const char * name = "s01";
        const char * family = "dht22";
        const char * location = "geam";
    } sensor;

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

        const uint16_t delay_between_connects = 500;
        const uint8_t connect_max_retries = 60;
    } wlan;

    struct ntp {
        const char * ntp_server = "pool.ntp.org";

        //In seconds
        const uint8_t initial_sleep = 10;
    } ntp;

    struct mqtt {
        const char * server        = __MQTT_SERVER__;
        const uint16_t server_port = __MQTT_SERVER_PORT__;
        const char * auth_user     = __MQTT_USER__;
        const char * auth_pass     = __MQTT_PASS__;

        const char * mqtt_topic    = __MQTT_TOPIC__;
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
              app.sensor.name,
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

void time_init() {
    char buffer[128];

    bzero(&buffer[0], sizeof(buffer));
    strncpy(&buffer[0], app.ntp.ntp_server, sizeof(buffer) - 1);

    log_i("initializing SNTP library with %s as server", app.ntp.ntp_server);
    sntp_setservername(0, &buffer[0]);
    sntp_init();

    log_i("waiting %d seconds to leave SNTP enough time to read time", (app.ntp.initial_sleep));
    delay(app.ntp.initial_sleep * 1000);
    log_i("sleeping time is over. time is now %s", time_get_ascii());
}

void serial_init() {
    Serial.begin(app.serial.baud);
    Serial.println();

    delay(app.serial.initial_wait);
}

boolean wifi_connect() {
    uint8_t counter = 0;    

    log_i("attempting to connect to wireless network %s", app.wlan.ssid);
    WiFi.begin(app.wlan.ssid, app.wlan.pass);
        
    while (WiFi.status() != WL_CONNECTED) {
      if (counter++ >= app.wlan.connect_max_retries) {
          log_i("still can't connect. bailing out ...");
          return false;
      }

      log_i("still nothing. sleeping for %d miliseconds", app.wlan.delay_between_connects);
      delay(app.wlan.delay_between_connects);
    }

    log_i("connected to %s", app.wlan.ssid);
    return true;
}

void wifi_disconnect() {
    WiFi.disconnect();
}

dht * dht_read_sensor() {
    static dht * sensor = new dht();

    switch (sensor->read22(app.dht.pin)) {
        case DHTLIB_ERROR_CHECKSUM:
            log_e("checksum error reading DHT22 on pin %u", app.dht.pin);
            return NULL;

        case DHTLIB_ERROR_TIMEOUT:
            log_e("timeout reading DHT22 on pin %u", app.dht.pin);
            return NULL;
    
        default:
            break;
    }

    return sensor;
}

char * dht_readings_to_json(dht * sensor) {
    static char buffer[255];
    StaticJsonDocument<255> json;
    JsonObject j_nested;

    if (sensor == NULL) {
        return NULL;
    }

    log_v("preparing json document");

    j_nested = json.createNestedObject("sensor");
    j_nested["name"] = app.sensor.name;
    j_nested["family"] = app.sensor.family;
    j_nested["location"] = app.sensor.location;

    j_nested = json.createNestedObject("uptime");
    j_nested["seconds"] = uptime::getDays() * 86400 + uptime::getHours() * 3600 + uptime::getMinutes() * 60 + uptime::getSeconds();
    j_nested["formatted"] = uptime_formatter::getUptime();

    log_v("humidity is %f", sensor->humidity);
    json["humidity"]      = sensor->humidity;
    
    log_v("temperature is %f", sensor->temperature);
    json["temperature"]   = sensor->temperature;

    json["time_read"] = time_get_unix();

    log_v("serializing json");
    memset(&buffer[0], 0, sizeof(buffer) - 1);
    serializeJson(json, &buffer[0], sizeof(buffer) - 1);

    return &buffer[0];
}

char * dht_read_json() {
    return dht_readings_to_json(dht_read_sensor());
}

void setup() {
    serial_init();

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

    //if we are in the first year of 1970, we failed to sync time. Let's try again
    if (time_get_unix() < 3600 * 24 * 365) {
        time_init();
    }

    mqtt_connect();
    mqtt_send(dht_read_json());
    mqtt_disconnect();
    delay(2000);


    wifi_disconnect();
    delay(app.global.sleep_time);

    log_i("%s", time_get_ascii());
    Serial.println("##########  END LOOP  ##########");
}
