/*
 *  Simple ESP8266 gateway 
 *  with connection parameters in the code
 *
 *  Based on MySensors library (http://www.mysensors.org)
 */

#define MY_DEBUG
#define MY_BAUD_RATE 115200

#define MY_RADIO_RF24
#define MY_RF24_CHANNEL 64

// WIFI SSID and password
#define MY_WIFI_SSID "your-wifi-ssid"
#define MY_WIFI_PASSWORD "your-wifi-password"

// MQTT broker
#define MY_GATEWAY_MQTT_CLIENT
#define MY_GATEWAY_ESP8266
#define MY_CONTROLLER_URL_ADDRESS "mqtt.sensorocean.com"
#define MY_PORT 1883

#define SENSOROCEAN_GATEWAY_ID "sensorocean-gateway-id"
#define MY_MQTT_PUBLISH_TOPIC_PREFIX SENSOROCEAN_GATEWAY_ID "-out"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX SENSOROCEAN_GATEWAY_ID "-in"
#define MY_MQTT_CLIENT_ID SENSOROCEAN_GATEWAY_ID

#define MY_MQTT_USER "your-sensorocean-username"
#define MY_MQTT_PASSWORD "your-sensorocean-password"

#include <ESP8266WiFi.h>
#include <MySensors.h>

void setup()
{
	// Setup locally attached sensors
}

void presentation()
{
	// Present locally attached sensors here
}

void loop()
{
	// Send locally attached sensors data here
}
