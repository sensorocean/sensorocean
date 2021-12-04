/*
 *  Soil Moisture Sensor
 *  Arduino Nano
 *  Based on MySensors library (http://www.mysensors.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 */

#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24
#define MY_RF24_CHANNEL 124

// Timeout (in ms) to re-establish link if node is put to sleep and transport is not ready.
#define MY_SLEEP_TRANSPORT_RECONNECT_TIMEOUT_MS 5000

// The wait period (in ms) before going to sleep when using smartSleep-functions.
#define MY_SMART_SLEEP_WAIT_DURATION_MS 5000

// Define Node ID
// #define MY_NODE_ID AUTO
#define MY_NODE_ID 1

// Soil Moisture Sensor
#define SOILMOISTURE_CHILD_ID 1
#define SOILMOISTURE_ANALOG_PIN  (0)

#include <MySensors.h>
#include <TimeLib.h>

MyMessage msg_soilmoisture(SOILMOISTURE_CHILD_ID, V_LEVEL);

bool gotTime = false;
uint32_t SLEEP_TIME = 600000; // Sleep time between reads (in milliseconds)

float soilmoisture = 0;

void setup() {
  // Setup locally attached sensors

  //Sync time with the server
  requestTime();
  #ifdef MY_DEBUG
    Serial.println(F("Requesting time"));
  #endif
}

void presentation() {
  Serial.print("Node: ");
  Serial.println(getNodeId());

  // Present locally attached sensors here
  sendSketchInfo("Soil Moisture Sensor", "1.0");
  present(SOILMOISTURE_CHILD_ID, S_MOISTURE, "Soil Moisture 01 (%)");
}

void loop() {
  // request time from controller
  if (gotTime == false) {
    requestTime();
    #ifdef MY_DEBUG
      Serial.println(F("Requesting time"));
    #endif
  }

  #ifdef MY_DEBUG
    time_t t = now();
    char buf[50];
    sprintf(buf, "%4d-%02d-%02d %02d:%02d:%02d ", year(t), month(t), day(t), hour(t), minute(t), second(t));
    Serial.print(buf);
    Serial.print(" ");
  #endif

  // Send locally attached sensors data here
  soilmoisture = analogRead(SOILMOISTURE_ANALOG_PIN);
  Serial.print("\n\nMoisture Analogue = ");
  Serial.println(soilmoisture);

  int moisture_percentage;

  // For a sensor from the keyestudio
  // Modify the following calculation based on your particular sensor
  if(soilmoisture >= 700) {
    moisture_percentage = 100;
  } else {
    moisture_percentage = (soilmoisture/700) * 100;
  }

  Serial.print("Moisture Percentage = ");
  Serial.print(moisture_percentage);
  Serial.print("%\n\n");

  // Send the values to gateway
  send(msg_soilmoisture.set(moisture_percentage, 2));

  // Sleep until next measurement (msec)
  smartSleep(SLEEP_TIME);
}


void receive(const MyMessage &message) {
  // callback from controller
  #ifdef MY_DEBUG
    Serial.print("Message received: ");
    Serial.println(message.type);
  #endif

}


void receiveTime(unsigned long newTime) {
  setTime(newTime);
  gotTime = true;
  #ifdef MY_DEBUG
    char buf[50];
    sprintf(buf, "%4d-%02d-%02d %02d:%02d:%02d ", year(newTime), month(newTime), day(newTime), hour(newTime), minute(newTime), second(newTime));
    Serial.println(buf);
  #endif
}
