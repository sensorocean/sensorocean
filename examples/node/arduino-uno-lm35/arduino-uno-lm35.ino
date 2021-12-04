/*
 *
 *  LM35 sensor
 *  Arduino Uno
 *  Based on MySensors library (http://www.mysensors.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 */
 
// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24
#define MY_RF24_CHANNEL 124

#define MY_RF24_PA_LEVEL RF24_PA_LOW

// Define Node ID
#define MY_NODE_ID AUTO

#include <MySensors.h>
#include <TimeLib.h> 


bool gotTime = false;
uint32_t WAIT_TIME = 5000;    // Sleep time between reads (in milliseconds)

// Temperature LM35
#define CHILD_ID_TEMP 1
float temp;
float tempF;
float tempC;
int reading;
int tempPin = 0;

bool metric = true;

MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);

void before() {
    //
}

void setup() {  
  analogReference(INTERNAL);
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

  sendSketchInfo("uno-lm35", "1.0");

  // Register all sensors to gw (they will be created as child devices)
  present(CHILD_ID_TEMP, S_TEMP, "Temperature LM35");

  metric = getControllerConfig().isMetric;
  if(metric) {
    Serial.println("Measurements: METRIC");
    } else {
    Serial.println("Measurements: IMPERIAL");  
    } 
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
    Serial.println(buf);
  #endif

  measureTemperature();
  send(msgTemp.set(temp, 1));
    
  // delay until next measurement (msec)
  wait(WAIT_TIME);
}



void measureTemperature() {
  reading = analogRead(tempPin);
  tempC = reading / 9.31;
  tempF = tempC *9 / 5;
  tempF = tempF + 32;

  if(metric) {
    temp = tempC;
  } else {
    temp = tempF;
  }
  
  Serial.print("T: ");
  Serial.println(temp);  
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
