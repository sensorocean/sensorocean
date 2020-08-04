/*
 *  ESP8266 gateway with WiFiManager, configuration of WiFi credentials and custom parameters:
 *
 *    Controller Username (MQTT)
 *    Controller Password (MQTT)
 *    SensorOcean Gateway ID
 *    Controller Address (mqtt.sensorocean.com)
 *    Controller Port (1883)
 *
 *  When the ESP starts up, it sets it up in Station mode and tries to connect to a previously saved Access Point
 *  if this is unsuccessful (or no previous network saved) it moves the ESP into Access Point mode and spins up
 *  a DNS and WebServer (default ip 192.168.4.1) using any wifi enabled device with a browser connect to the
 *  newly created Access Point and configure all necessary parameters.
 *  If successful, it relinquishes control back to the app. If not, reconnect to AP and reconfigure.
 *  (more here https://github.com/tzapu/WiFiManager)
 *
 *  The Config Access Point can be called be a double click of the Reset button of the ESP
 *  (more here https://github.com/datacute/DoubleResetDetector).
 *
 */
#include <EEPROM.h>
#include <SPI.h>
#include <DoubleResetDetector.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

#define NAME "ESP Gateway"
#define FORM_HEADER "<br/><p>ESP Gateway</p>"
#define SKETCHNAME "ESP Gateway"
#define SKETCHVERSION "0.01"

// Configuration of WiFi and MQTT
const char wifi_ssid[] = "";
const char wifi_password[] = "";

#define USERNAME_LENGTH 32
#define PASSWORD_LENGTH 33
#define GATEWAYID_LENGTH 37
#define CONTROLLER_HOST_LENGTH 33
#define CONTROLLER_PORT_LENGTH 7

char mqtt_username[USERNAME_LENGTH];
char mqtt_pass[PASSWORD_LENGTH];
char mqtt_gatewayid[GATEWAYID_LENGTH];
char controller_host[CONTROLLER_HOST_LENGTH];
char controller_port[CONTROLLER_PORT_LENGTH];
int controller_port_int;

// The index (start position) of the params in EEPROM
#define EEPROM_USERNAME_INDEX 1
#define EEPROM_PASSWORD_INDEX 33
#define EEPROM_GATEWAYID_INDEX 65
#define EEPROM_CONTROLLER_HOST_INDEX 102
#define EEPROM_CONTROLLER_PORT_INDEX 135

char mqtt_publish_topic[50] = "";
char mqtt_subscribe_topic[50] = "";

//MySensors Configuration
#define MY_DEBUG
#define MY_BAUD_RATE 115200
#define MY_RADIO_RF24
#define MY_RF24_CHANNEL 124
#define MY_GATEWAY_MQTT_CLIENT
#define MY_GATEWAY_ESP8266
#define MY_WIFI_SSID wifi_ssid
#define MY_WIFI_PASSWORD wifi_password
#define MY_CONTROLLER_URL_ADDRESS controller_host
#define MY_PORT controller_port_int
#define MY_MQTT_USER mqtt_username
#define MY_MQTT_PASSWORD mqtt_pass
#define MY_MQTT_CLIENT_ID mqtt_gatewayid
#define MY_MQTT_PUBLISH_TOPIC_PREFIX mqtt_publish_topic
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX mqtt_subscribe_topic

const char pub_prefix[] = "-out";
const char sub_prefix[] = "-in";

#include <MySensors.h>

//  Setup of the DoubleResetDetector
//  in seconds
#define DRD_TIMEOUT 10
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

// Ticker to handle the builtin LED behavior
Ticker ticker;

// Set up heartbeat so we know the gateway is online regardless of whether it has sensors attached
bool shouldSaveConfig = false;
bool shouldSendHeartbeat = false;
int heartBeatFrequency = 60; // seconds
Ticker heartBeat;


void setSendHeartbeatFlag(){
  shouldSendHeartbeat = true;
}

// Save custom parameters to EEPROM
void shouldSaveConfigToEEPROM(){

  for(int i = 0; i < USERNAME_LENGTH; i++){
    saveState(i + EEPROM_USERNAME_INDEX, mqtt_username[i]);
  }

  for(int i = 0; i < PASSWORD_LENGTH; i++){
    saveState(i + EEPROM_PASSWORD_INDEX, mqtt_pass[i]);
  }

  for(int i = 0; i < GATEWAYID_LENGTH; i++){
    saveState(i + EEPROM_GATEWAYID_INDEX, mqtt_gatewayid[i]);
  }

  for(int i = 0; i < CONTROLLER_HOST_LENGTH; i++){
    saveState(i + EEPROM_CONTROLLER_HOST_INDEX, controller_host[i]);
  }

  for(int i = 0; i < CONTROLLER_PORT_LENGTH; i++){
    saveState(i + EEPROM_CONTROLLER_PORT_INDEX, controller_port[i]);
  }
}

// Load the custom parameters from EEPROM
void loadParametersFromEEPROM(){

  for(int i = 0; i < USERNAME_LENGTH; i++){
    mqtt_username[i] = loadState(i + EEPROM_USERNAME_INDEX);
  }

  for(int i = 0; i < PASSWORD_LENGTH; i++){
    mqtt_pass[i] = loadState(i + EEPROM_PASSWORD_INDEX);
  }

  for(int i = 0; i < GATEWAYID_LENGTH; i++){
    mqtt_gatewayid[i] = loadState(i + EEPROM_GATEWAYID_INDEX);
  }

  for(int i = 0; i < CONTROLLER_HOST_LENGTH; i++){
    controller_host[i] = loadState(i + EEPROM_CONTROLLER_HOST_INDEX);
  }

  for(int i = 0; i < CONTROLLER_PORT_LENGTH; i++){
    controller_port[i] = loadState(i + EEPROM_CONTROLLER_PORT_INDEX);
  }
}

// Callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("shouldSaveConfig: true");
  shouldSaveConfig = true;
}

// Gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());

  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);

  // We don't want the next time the board resets to be considered a double reset so we remove the flag
  drd.stop();
}


// Handle the Builtin LED togggle state
void tick() {
  // Toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}


void before() {
  // Set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);

  // Start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  loadParametersFromEEPROM();

  WiFiManagerParameter so_form_header(FORM_HEADER);

  // The method attributes: id/name, placeholder/prompt, default, length
  WiFiManagerParameter so_mqtt_username("mqtt_username", "username", mqtt_username, USERNAME_LENGTH);
  WiFiManagerParameter so_mqtt_pass("mqtt_pass", "password", mqtt_pass, PASSWORD_LENGTH);
  WiFiManagerParameter so_mqtt_gatewayid("mqtt_gatewayid", "gateway ID", mqtt_gatewayid, GATEWAYID_LENGTH);
  WiFiManagerParameter so_controller_host("controller_host", "controller.sensorocean.com", controller_host, CONTROLLER_HOST_LENGTH);
  WiFiManagerParameter so_controller_port("controller_port", "1883", controller_port, CONTROLLER_PORT_LENGTH);

  WiFiManager wifiManager;

  // Set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  wifiManager.addParameter(&so_form_header);
  wifiManager.addParameter(&so_mqtt_username);
  wifiManager.addParameter(&so_mqtt_pass);
  wifiManager.addParameter(&so_mqtt_gatewayid);
  wifiManager.addParameter(&so_controller_host);
  wifiManager.addParameter(&so_controller_port);

  // Reset settings - for testing
  // wifiManager.resetSettings();

 if (drd.detectDoubleReset()) {
    // Starting the Config Portal Access Point
    wifiManager.startConfigPortal(NAME);

  } else {
    // Continue to autoConnect
    wifiManager.autoConnect(NAME);
  }

  // If you get here you have connected to the WiFi
  Serial.println("\nConnected to WiFi");
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());

  // Read updated parameters
  strcpy(mqtt_username, so_mqtt_username.getValue());
  strcpy(mqtt_pass, so_mqtt_pass.getValue());
  strcpy(mqtt_gatewayid, so_mqtt_gatewayid.getValue());
  strcpy(controller_host, so_controller_host.getValue());
  strcpy(controller_port, so_controller_port.getValue());
  controller_port_int = atoi(controller_port);

  // Define MQTT topics
  strcpy(mqtt_publish_topic, mqtt_gatewayid);
  strcat(mqtt_publish_topic, pub_prefix);
  strcpy(mqtt_subscribe_topic, mqtt_gatewayid);
  strcat(mqtt_subscribe_topic, sub_prefix);

  if (shouldSaveConfig) {
    shouldSaveConfigToEEPROM();
  }

  Serial.println("\nParameters:");
  Serial.println(MY_MQTT_USER);
  Serial.println(MY_MQTT_PASSWORD);
  Serial.println(MY_MQTT_CLIENT_ID);
  Serial.println(MY_CONTROLLER_URL_ADDRESS);
  Serial.println(MY_PORT);

  Serial.println("\nMQTT Topics:");
  Serial.println(MY_MQTT_PUBLISH_TOPIC_PREFIX);
  Serial.println(MY_MQTT_SUBSCRIBE_TOPIC_PREFIX);
  Serial.println("\n");

  ticker.detach();
  // Keep LED on
  digitalWrite(BUILTIN_LED, LOW);

  drd.stop();
}


void setup() {
  heartBeat.attach(heartBeatFrequency, setSendHeartbeatFlag);
}


void presentation() {
  sendSketchInfo(SKETCHNAME, SKETCHVERSION);

  // Present locally attached sensors here
}


void loop() {

  if(shouldSendHeartbeat){
    shouldSendHeartbeat = false;
    Serial.println("Sending heartbeat");

    sendHeartbeat();
  }


}

void receive(const MyMessage &message) {
}
