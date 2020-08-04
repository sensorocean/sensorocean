/*
 *  Sonoff ESP8266
 *
 *  Sonoff (ESP) gateway with WiFiManager, configuration of WiFi credentials and custom parameters:
 *
 *    Controller Username (MQTT)
 *    Controller Password (MQTT)
 *    SensorOcean Gateway ID
 *    Controller Address  (mqtt.sensorocean.com)
 *    Controller Port     (1883)
 *
 *  When the ESP starts up, it sets it up in Station mode and tries to connect to a previously saved Access Point
 *  if this is unsuccessful (or no previous network saved) it moves the ESP into Access Point mode and spins up
 *  a DNS and WebServer (default ip 192.168.4.1) using any wifi enabled device with a browser connect to the
 *  newly created Access Point and configure all necessary parameters.
 *  If successful, it relinquishes control back to the app. If not, reconnect to AP and reconfigure.
 *  (more here https://github.com/tzapu/WiFiManager)
 *
 *  The Buton on the device can be used as the following:
 *    - Short Click: toggle relay on/off
 *    – Hold the button for 5 seconds: reset module
 *    – Hold the button for 20 seconds: reset the configs, call the configuration mode Access Point.
 *
 */

#include <EEPROM.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

#define NAME "SensorOcean Sonoff 01"
#define FORM_HEADER "<br/><p>SensorOcean Sonoff 01</p>"
#define SKETCHNAME "SensorOcean Sonoff"
#define SKETCHVERSION "0.02"

// Configuration of WiFi and MQTT
const char wifi_ssid[] = "";
const char wifi_password[] = "";

#define USERNAME_LENGTH 32
#define PASSWORD_LENGTH 33
#define GATEWAYID_LENGTH 37
#define CONTROLLER_HOST_LENGTH 33
#define CONTROLLER_PORT_LENGTH 7
#define BOOTSTATE_LENGTH 3

char mqtt_username[USERNAME_LENGTH];
char mqtt_pass[PASSWORD_LENGTH];
char mqtt_gatewayid[GATEWAYID_LENGTH];
char controller_host[CONTROLLER_HOST_LENGTH];
char controller_port[CONTROLLER_PORT_LENGTH];
int controller_port_int;
char bootState[BOOTSTATE_LENGTH];

// The index (start position) of the params in EEPROM
#define EEPROM_USERNAME_INDEX 1
#define EEPROM_PASSWORD_INDEX 33
#define EEPROM_GATEWAYID_INDEX 65
#define EEPROM_CONTROLLER_HOST_INDEX 102
#define EEPROM_CONTROLLER_PORT_INDEX 135
#define EEPROM_BOOTSTATE_INDEX 142

char mqtt_client_id[50] = "";
char mqtt_publish_topic[50] = "";
char mqtt_subscribe_topic[50] = "";

//MySensors Configuration
#define MY_DEBUG
#define MY_BAUD_RATE 115200
#define MY_GATEWAY_MQTT_CLIENT
#define MY_GATEWAY_ESP8266
#define MY_WIFI_SSID wifi_ssid
#define MY_WIFI_PASSWORD wifi_password
#define MY_CONTROLLER_URL_ADDRESS controller_host
#define MY_PORT controller_port_int
#define MY_MQTT_USER mqtt_username
#define MY_MQTT_PASSWORD mqtt_pass
#define MY_MQTT_CLIENT_ID mqtt_client_id
#define MY_MQTT_PUBLISH_TOPIC_PREFIX mqtt_publish_topic
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX mqtt_subscribe_topic

const char pub_prefix[] = "-out";
const char sub_prefix[] = "-in";

#include <MySensors.h>

#define CHILD_ID 0
MyMessage msg(CHILD_ID,V_STATUS);

// Sonoff
#define   SONOFF_BUTTON             0
#define   SONOFF_INPUT              14
#define   SONOFF_LED                13
#define   SONOFF_AVAILABLE_CHANNELS 1

const int SONOFF_RELAY_PINS[4] =    {12, 12, 12, 12};

// If this is false, led is used to signal startup state, then always on
// If it s true, it is used to signal startup state, then mirrors relay state
#define SONOFF_LED_RELAY_STATE      true

// For LED status
Ticker ticker;

const int CMD_WAIT = 0;
const int CMD_BUTTON_CHANGE = 1;
int cmd = CMD_WAIT;
int buttonState = HIGH;
static long startPress = 0;

bool shouldSaveConfig = false;

// Set up heartbeat so we know the gateway is online regardless of whether it has sensors attached
bool shouldSendHeartbeat = false;
int heartBeatFrequency = 60; // seconds
Ticker heartBeat;

// Set up a ticker to send state to controller
bool shouldSendState = false;
int stateFrequency = 5; // seconds
Ticker stateBeat;

// Tick the LED
void tick()
{
  //toggle state
  int state = digitalRead(SONOFF_LED);  // get the current state of GPIO1 pin
  digitalWrite(SONOFF_LED, !state);     // set pin to the opposite state
}

// Set state of the Relay
void setState(int state, int channel) {
  //relay
  digitalWrite(SONOFF_RELAY_PINS[channel], state);
  //led
  if (SONOFF_LED_RELAY_STATE) {
    digitalWrite(SONOFF_LED, (state + 1) % 2); // led is active low
  }
  // save to EEPROM
  saveBootStateToEEPROM(state);
}

// Turn On the relay for the channel
void turnOn(int channel = 0) {
  int relayState = HIGH;
  setState(relayState, channel);
}

// Turn Off the relay for the channel
void turnOff(int channel = 0) {
  int relayState = LOW;
  setState(relayState, channel);
}

//
ICACHE_RAM_ATTR void toggleState() {
  cmd = CMD_BUTTON_CHANGE;
}

//
void toggle(int channel = 0) {
  Serial.println("toggle state");
  Serial.println(digitalRead(SONOFF_RELAY_PINS[channel]));
  int relayState = digitalRead(SONOFF_RELAY_PINS[channel]) == HIGH ? LOW : HIGH;
  setState(relayState, channel);
}

// Restart the board
void restart() {
  ESP.reset();
  delay(1000);
}

// Reset the board
void reset() {
  //reset wifi credentials
  WiFi.disconnect();
  delay(1000);
  ESP.reset();
  delay(1000);
}

// Flag to send the heartbeat
void setSendHeartbeatFlag(){
  shouldSendHeartbeat = true;
}

// Flag to send the data
void setSendStateFlag(){
  shouldSendState = true;
}

// Save bootState parameters to EEPROM
void saveBootStateToEEPROM(int state){

  if(state == HIGH) {
    strcpy(bootState, "off");
  } else {
    strcpy(bootState, "on");
  }

  for(int i = 0; i < BOOTSTATE_LENGTH; i++){
    saveState(i + EEPROM_BOOTSTATE_INDEX, bootState[i]);
  }
}

// Save custom parameters to EEPROM
void saveParametersToEEPROM(){

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

  for(int i = 0; i < BOOTSTATE_LENGTH; i++){
    bootState[i] = loadState(i + EEPROM_BOOTSTATE_INDEX);
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

}

void before() {
  //set led pin as output and start with 0.5 because we start in AP mode and try to connect
  pinMode(SONOFF_LED, OUTPUT);
  ticker.attach(0.6, tick);

  // Load the config params
  loadParametersFromEEPROM();

  WiFiManagerParameter so_form_header(FORM_HEADER);

  // The method attributes: id/name, placeholder/prompt, default, length
  WiFiManagerParameter so_mqtt_username("mqtt_username", "username", mqtt_username, USERNAME_LENGTH);
  WiFiManagerParameter so_mqtt_pass("mqtt_pass", "password", mqtt_pass, PASSWORD_LENGTH);
  WiFiManagerParameter so_mqtt_gatewayid("mqtt_gatewayid", "gateway ID", mqtt_gatewayid, GATEWAYID_LENGTH);
  WiFiManagerParameter so_controller_host("controller_host", "192.168.1.100", controller_host, CONTROLLER_HOST_LENGTH);
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

  if (!wifiManager.autoConnect(NAME)) {
    Serial.println("Failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
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

  // define client id
  strcpy(mqtt_client_id, mqtt_gatewayid);

  // Define MQTT topics
  strcpy(mqtt_publish_topic, mqtt_gatewayid);
  strcat(mqtt_publish_topic, pub_prefix);
  strcpy(mqtt_subscribe_topic, mqtt_gatewayid);
  strcat(mqtt_subscribe_topic, sub_prefix);

  // Save the params from the AP to EEPROM
  if (shouldSaveConfig) {
    saveParametersToEEPROM();
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

  Serial.println("\nRelay State:");
  Serial.println(bootState);

  // Stop the ticker
  ticker.detach();

  // Setup button
  pinMode(SONOFF_BUTTON, INPUT);
  attachInterrupt(SONOFF_BUTTON, toggleState, CHANGE);

  // Setup relay[0]
  // TODO: setup multiple relays

  pinMode(SONOFF_RELAY_PINS[0], OUTPUT);

  if (strcmp(bootState, "off") == 0) {
    turnOn();
  } else {
    turnOff();
  }

  // Setup LED
  if (!SONOFF_LED_RELAY_STATE) {
    digitalWrite(SONOFF_LED, LOW);
  }

}


void setup() {
  heartBeat.attach(heartBeatFrequency, setSendHeartbeatFlag);
  stateBeat.attach(stateFrequency, setSendStateFlag);

}


void presentation() {
  sendSketchInfo(SKETCHNAME, SKETCHVERSION);

  // Register sensor
  present(CHILD_ID, S_BINARY, "Sonoff Switch");

}


void loop() {

  // Send heartBeat
  if(shouldSendHeartbeat) {
    shouldSendHeartbeat = false;
    sendHeartbeat();
  }

  // send state of the relay
  if(shouldSendState) {
    shouldSendState = false;
    int currentState = digitalRead(SONOFF_RELAY_PINS[0]);
    send(msg.set(currentState));
  }

  // Look for button
  switch (cmd) {
    case CMD_WAIT:
      break;
    case CMD_BUTTON_CHANGE:
      int currentState = digitalRead(SONOFF_BUTTON);
      if (currentState != buttonState) {
        if (buttonState == LOW && currentState == HIGH) {
          long duration = millis() - startPress;
          if (duration < 1000) {
            Serial.println("Short press - toggle relay");
            toggle();
          } else if (duration < 5000) {
            Serial.println("Medium press - reset");
            restart();
          } else if (duration < 60000) {
            Serial.println("Long press - reset settings");
            reset();
          }
        } else if (buttonState == HIGH && currentState == LOW) {
          startPress = millis();
        }
        buttonState = currentState;
      }
      break;
  }

}

void receive(const MyMessage &message)
{
  // We only react on status messages from the controller to this CHILD_ID.
  if (message.type==V_STATUS  && message.sensor==CHILD_ID) {

    int state = message.getBool();

    if ( state == 1) {
      Serial.println("ON");
      turnOn();
      send(msg.set(state));

    } else if (state == 0) {
      Serial.println("OFF");
      turnOff();
      send(msg.set(state));
    }
  }
}
