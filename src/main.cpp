#include <Arduino.h>
#include <dht.h>
#include <WiFi.h>
#include <time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_log.h>

#define STATUS_OK 0
#define STATUS_NOT_OK 1

struct __app_state {
    u_char global = STATUS_NOT_OK;
    u_char wireless = STATUS_NOT_OK;
    u_char mqtt = STATUS_NOT_OK;
} app_state;

struct __app_config {
    struct global {
        /*
         * Miliseconds to sleep if all connectivity is ok
         */
        uint32_t sleep_time = 60000;
    } global;

    struct serial {
        unsigned long baud = 115200;

        //Wait for these ms for serial to become available
        uint32_t initial_wait = 500;
    } serial;

    struct wlan {
        const char * ssid = __WIFI_SSID__;
        const char * pass = __WIFI_PASS__;

        WiFiClass * h;
    } wlan;

    struct ntp {
        const char * ntpServer = "pool.ntp.org";
        const long   gmtOffset_sec = 0;
        const int    daylightOffset_sec = 0;    
    } ntp;

    struct mqtt {
        const char * server_ip = __MQTT_SERVER__;
        const char * auth_user = __MQTT_USER__;
        const char * auth_pass = __MQTT_PASS__;

        IPAddress h;
    } mqtt;
} app;

TaskHandle_t comm_t;

void _init_serial() {
    Serial.begin(app.serial.baud);
    Serial.println();

    delay(app.serial.initial_wait);
}

void _setup_wifi() {
    static WiFiClass * wifi_h = NULL;

    if ( wifi_h != NULL) {
        wifi_h = new WiFiClass();
    }

    log_i("attempting to connect to wireless network %s", app.wlan.ssid);
    wifi_h->begin(app.wlan.ssid, app.wlan.pass);
        
    while (wifi_h->status() != WL_CONNECTED) {
      log_i("still nothing. sleeping for half a second ...");
      delay(500);
    }
    log_i("connected to %s", app.wlan.ssid);

    log_d("setting wireless status to ok");
    app_state.wireless = STATUS_OK;

    log_d("saving wireless handler");
    app.wlan.h = wifi_h;
}

void _init_ntp() {
    log_d("sleeping for 2 seconds before calling NTP server");
    delay(2000);

    log_i("getting time from ntp server %s", app.ntp.ntpServer);
    configTime(app.ntp.gmtOffset_sec, app.ntp.daylightOffset_sec, app.ntp.ntpServer);
    log_d("we've got the time ! (note: configTime() has void return type so we don't actually know if call succeeded or not");
}

void _start_comm(void * nothing) {
    _setup_wifi();
    _init_ntp();

    while (1) {
        if (app.wlan.h->status() != WL_CONNECTED) {
            log_i("wireless status is not Connected. Initiating connect");            
            _setup_wifi();

            //we don't sync time again because time is supposed to flow with or without Internet access
        }

        delay(app.global.sleep_time);
    }
}

void setup() {
    _init_serial();

    log_d("creating communications task");
    BaseType_t xReturned = xTaskCreatePinnedToCore(_start_comm, "communications_task", 10000, NULL, 0, &comm_t, 0);
    if (xReturned != pdPASS) {
        log_e("something went wrong while creating the parallelization task. halting ...");
        while (1) {
            delay(1000000);
        }
    }
    
}
/*
void setup3() { 

    WiFi.onEvent(WiFiEvent);

    WiFi.begin(ssid, pass);
    Serial.print("Connecting to wireless network ");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println();
    Serial.println("Waiting 2 seconds for wireless to settle before getting time from NTP server");
    delay(2000);

    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    Serial.print("LIBRARY VERSION: ");
    Serial.println(DHT_LIB_VERSION);
    Serial.println();
    Serial.println("Type,\tstatus,\tHumidity (%),\tTemperature (C)\tTime (us)");

//    mqtt_client.connect("esp32_Client", mqtt_user, mqtt_pass);
}
*/

const IPAddress mqtt_server(18, 195, 106, 0);

#define DHT22_PIN 13

WiFiClient wifi_client;
PubSubClient mqtt_client(mqtt_server, 1883, NULL, wifi_client);

StaticJsonDocument<256> temp_reading;

typedef StaticJsonDocument<256> j_dht;


const char * read_dht_to_json() {
    dht DHT;
    uint32_t start, stop;
    int op_code;
    j_dht reading;
    static String json_doc;

    memset(&json_doc, 0, 256);

    start = micros();
    op_code = DHT.read22(DHT22_PIN);
    stop = micros();

    reading["read_duration"] = stop - start;
    reading["humidity"] = DHT.humidity;
    reading["temperature"] = DHT.temperature;
    reading["timestamp"] = "test";
    reading["error_code"] = op_code;

    serializeJson(reading, json_doc);

    return json_doc.c_str();
}

void printLocalTime()
{
  struct tm timeinfo;

  if(!getLocalTime(&timeinfo)){
    Serial.print("failed to obtain time");
    return;
  }

  Serial.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void WiFiEvent(WiFiEvent_t event)
{
    printLocalTime();
    Serial.printf(" [WiFi-event] event_id: %d - ", event);

    switch (event) {
        case SYSTEM_EVENT_WIFI_READY:
            Serial.println("WiFi interface ready");
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            Serial.println("Completed scan for access points");
            break;
        case SYSTEM_EVENT_STA_START:
            Serial.println("WiFi client started");
            break;
        case SYSTEM_EVENT_STA_STOP:
            Serial.println("WiFi clients stopped");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to access point");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("Disconnected from WiFi access point");
            break;
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("Authentication mode of access point has changed");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.print("Obtained IP address: ");
            Serial.println(WiFi.localIP());
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            Serial.println("Lost IP address and IP address is reset to 0");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case SYSTEM_EVENT_AP_START:
            Serial.println("WiFi access point started");
            break;
        case SYSTEM_EVENT_AP_STOP:
            Serial.println("WiFi access point  stopped");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("Client connected");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.println("Client disconnected");
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            Serial.println("Assigned IP address to client");
            break;
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            Serial.println("Received probe request");
            break;
        case SYSTEM_EVENT_GOT_IP6:
            Serial.println("IPv6 is preferred");
            break;
        case SYSTEM_EVENT_ETH_START:
            Serial.println("Ethernet started");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("Ethernet stopped");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("Ethernet connected");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("Ethernet disconnected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.println("Obtained IP address");
            break;
        default: break;
    }}

void setup2() {
    Serial.begin(115200);
    Serial.println();
    delay(1000);

    WiFi.onEvent(WiFiEvent);

    WiFi.begin(ssid, pass);
    Serial.print("Connecting to wireless network ");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println();
    Serial.println("Waiting 2 seconds for wireless to settle before getting time from NTP server");
    delay(2000);

    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    Serial.print("LIBRARY VERSION: ");
    Serial.println(DHT_LIB_VERSION);
    Serial.println();
    Serial.println("Type,\tstatus,\tHumidity (%),\tTemperature (C)\tTime (us)");

//    mqtt_client.connect("esp32_Client", mqtt_user, mqtt_pass);
}

void loop() {
 //   mqtt_client.publish("99/s01", read_dht_to_json());

    delay(1000);
}