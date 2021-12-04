/*
 *
 *  HighTemperature Sensor with Battery measurer
 *  Arduino Mini
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 */

// Enable debug prints
#define MY_DEBUG
#define MY_DEBUG_VERBOSE
#define MY_REGISTRATION_FEATURE 1

// Enable and select radio type attached
#define MY_RADIO_NRF24
#define MY_RF24_CHANNEL 124

#define MY_NODE_ID 9
#define MY_MQTT_CLIENT_PUBLISH_RETAIN
//#define MY_PARENT_NODE_ID 100

#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MySensors.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 5

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

#define CHILD_ID_TEMP 0
#define VOLTAGE_CHILD_ID 5

#define SKETCH_NAME "Customized Dallas Sketch"
#define SKETCH_VERSION "2.0"

boolean metric = true;
unsigned long SLEEP_TIME = 5000;                    // Sleep time between reads (in milliseconds)

MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgVoltage(VOLTAGE_CHILD_ID, V_VOLTAGE);  // Node voltage


#define VBAT_PER_BITS 0.003363075
#define VMIN 2.2                                    //  Vmin (radio Min Volt)=1.9V (564v)
#define VMAX 4.5                                    //  Vmax = (3xAA bat)=4.5V (892v)
int BATTERY_SENSE_PIN = A0;


void setup()
{
  analogReference(INTERNAL);      // For battery sensing

  delay(500);                     // Allow time for radio if power used as reset

  // start serial port
  Serial.println("Dallas Temperature IC Control Library");

  // locate devices on the bus
  Serial.println("Locating devices...");
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  //Battery inital calc
  Serial.print("With Battery VMax (100%) = ");
  Serial.print(VMAX);
  Serial.print("volts and Vmin (0%) = ");
  Serial.print(VMIN);
  Serial.println(" volts");

  Serial.print("Battery Percent 25%/50%/75% calculates to: ");
  Serial.print(((VMAX - VMIN) / 4) + VMIN);
  Serial.print("/");
  Serial.print(((VMAX - VMIN) / 2) + VMIN);
  Serial.print("/");
  Serial.println(VMAX - ((VMAX - VMIN) / 4));
  delay(1000);

  int sensorValue = analogRead(BATTERY_SENSE_PIN);
  delay(50);
  float Vbat  = sensorValue * VBAT_PER_BITS;
  int batteryPcnt = static_cast<int>(((Vbat - VMIN) / (VMAX - VMIN)) * 100.);

  Serial.print("Current battery are measured to (please confirm!): ");
  Serial.print(batteryPcnt);
  Serial.print(" % - Or ");
  Serial.print(Vbat);
  Serial.println(" Volts");
}

void presentation()
{
  sendSketchInfo(SKETCH_NAME, SKETCH_VERSION);
  present(CHILD_ID_TEMP, S_TEMP, "High Temperature");
  present(VOLTAGE_CHILD_ID, S_MULTIMETER, "Node Battery" );
}

void loop()
{
  delay(500);

  // call sensors.requestTemperatures() to issue a global temperature
  #ifdef MY_DEBUG
    Serial.print("Requesting temperatures...");
  #endif
    sensors.requestTemperatures();    // Send the command to get temperatures

  #ifdef MY_DEBUG
    Serial.print("\nTemp: ");
    Serial.println(sensors.getTempCByIndex(0));
  #endif

  send(msgTemp.set(sensors.getTempCByIndex(0), V_TEMP));

  //  MeasureBattery();
  int sensorValue = analogRead(BATTERY_SENSE_PIN);
  delay(500);
  float voltage  = sensorValue * VBAT_PER_BITS;

  int batteryPcnt = static_cast<int>(((voltage - VMIN) / (VMAX - VMIN)) * 100.);

  #ifdef MY_DEBUG
    Serial.print("Battery Voltage: ");
    Serial.print(voltage);
    Serial.println(" Volts");

    Serial.print("Battery percent: ");
    Serial.print(batteryPcnt);
    Serial.println(" %\n");
  #endif

  //send battery in Volt as sensor data
  send(msgVoltage.set(voltage, V_VOLTAGE));

  // send battery level in % as node parameter!
  sendBatteryLevel(batteryPcnt);

  wait(SLEEP_TIME); //sleep a bit
}
