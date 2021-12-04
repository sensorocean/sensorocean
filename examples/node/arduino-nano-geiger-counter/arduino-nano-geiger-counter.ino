/*
 *  Geiger Counter
 *  Arduino Nano
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
//#define MY_RADIO_NRF5_ESB
//#define MY_RADIO_RFM69
//#define MY_RADIO_RFM95

// Timeout (in ms) to re-establish link if node is put to sleep and transport is not ready.
#define MY_SLEEP_TRANSPORT_RECONNECT_TIMEOUT_MS 5000

// The wait period (in ms) before going to sleep when using smartSleep-functions.
#define MY_SMART_SLEEP_WAIT_DURATION_MS 5000

// Define Node ID
// #define MY_NODE_ID AUTO
#define MY_NODE_ID 20
//#define MY_PARENT_NODE_ID 0
//#define MY_PARENT_NODE_IS_STATIC

// Sensor
#define GEIGER_CHILD_ID 1

#include <MySensors.h>
#include <TimeLib.h>

MyMessage msg_geiger(GEIGER_CHILD_ID, V_CUSTOM);

#define GEIGER_PIN  2

bool gotTime = false;
uint32_t WAIT_TIME = 10000; // Sleep time between reads (in milliseconds)

#include <SPI.h>
#define LOG_PERIOD 60000  //Logging period in milliseconds, recommended value 15000-60000.
#define MAX_PERIOD 60000  //Maximum logging period without modifying this sketch

// Conversion Factor used for conversion from CPM to uSv/h units
#define CONV_FACTOR 0.008120370

unsigned long counts;     //variable for GM Tube events
unsigned long cpm;        //variable for CPM
float uSvh;        //variable for uSv/h

unsigned int multiplier;  //variable for calculation CPM in this sketch
unsigned long previousMillis;  //variable for time measurement

void tube_impulse(){       //subprocedure for capturing events from Geiger Kit
  counts++;
}


void setup()
{
  // Setup locally attached sensors
  counts = 0;
  cpm = 0;
  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  Serial.begin(115200);
  pinMode(GEIGER_PIN, INPUT);
  attachInterrupt(0, tube_impulse, FALLING); //define external interrupts
  Serial.println("Start counter");

  //Sync time with the server
  requestTime();
  #ifdef MY_DEBUG
    Serial.println(F("Requesting time"));
  #endif
}


void presentation()
{
  Serial.print("Node: ");
  Serial.println(getNodeId());

  // Present locally attached sensors here
  sendSketchInfo("Geiger Counter", "0.1");
  present(GEIGER_CHILD_ID, S_CUSTOM, "Geiger (uSv/h)");

}


void loop()
{
  // if we don't have time , request it from controller
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

  // Geiger
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > LOG_PERIOD){
    previousMillis = currentMillis;
    cpm = counts * multiplier;
    uSvh = cpm * CONV_FACTOR;

    Serial.print("Counts: ");
    Serial.print(counts);
    Serial.print("   CPM: ");
    Serial.print(cpm);
    Serial.print("   uSv/h: ");
    Serial.println(uSvh);

    counts = 0;

    send(msg_geiger.set(uSvh, V_CUSTOM));
  }

  // delay until next measurement (msec)
  wait(WAIT_TIME);
}


void receive(const MyMessage &message) {
  // callback from controller
  #ifdef MY_DEBUG
    Serial.print("Message received: ");
    Serial.println(message.type);
  #endif
}


void receiveTime(unsigned long newTime)
{
  setTime(newTime);
  gotTime = true;
  #ifdef MY_DEBUG
    char buf[50];
    sprintf(buf, "%4d-%02d-%02d %02d:%02d:%02d ", year(newTime), month(newTime), day(newTime), hour(newTime), minute(newTime), second(newTime));
    Serial.println(buf);
  #endif
}
