
/*
 *
 *  LCD Display (1602 LCD)
 *
 *  DHT11 Temperature and Humidity Sensor
 *  Library DHT from Adafruit
 *
 *  Arduino Nano
 *  v 0.01
 *  Based on MySensors library (http://www.mysensors.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 */

#include <TimeLib.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>

// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24
#define MY_RF24_CHANNEL 124

#define MY_NODE_ID 5

#define TEMP_CHILD_ID 0
#define HUM_CHILD_ID 1
#define DISPLAY_CHILD_ID 10

#include <MySensors.h>

MyMessage msgTemp(TEMP_CHILD_ID, V_TEMP);
MyMessage msgHum(HUM_CHILD_ID, V_HUM);
MyMessage msgDisplay(DISPLAY_CHILD_ID, V_TEXT);

bool gotTime = false;

// DHT
#define DHTTYPE DHT11
#define DHTPIN 04
DHT dht(DHTPIN, DHTTYPE);

// LCD
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define READ_PERIOD 5000    // LCD and read sensors period
#define SEND_PERIOD 60000   // how often we send the data to gateway

unsigned long currentMillis;
unsigned long previousMillis;  //variables for time measurement

// Custom Text to display
String LCD_TEXT = "";

void setup (void) {
  dht.begin();
  Serial.begin(115200);

  // initialize the LCD,
  lcd.begin();
  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.clear();
  lcd.print("SensorOcean");
  lcd.setCursor (0,1); // go to start of 2nd line
  lcd.print("DHT11 ");
  if(getControllerConfig().isMetric) lcd.print("(Metric)");
  else lcd.print("(Imperial)");

  wait(3000);
  lcd.clear();

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

  sendSketchInfo("LCD and DHT11 Temp/Hum", "0.01");

  // Present locally attached sensors here
  present(DISPLAY_CHILD_ID, S_INFO, "LCD Display");

  if(getControllerConfig().isMetric) present(TEMP_CHILD_ID, S_TEMP, "Temp °C");
  else present(TEMP_CHILD_ID, S_TEMP, "Temp °F");

  present(HUM_CHILD_ID, S_HUM, "Humidity %");
}


void loop (void) {
  // request current time from controller
  if (not gotTime) {
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

  // DHT11
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float tc = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float tf = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(tc) || isnan(tf)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    wait(2000);
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(tf, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(tc, h, false);

  Serial.println("\n");

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.println(F("%"));

  Serial.print(F("Temp Celsius: "));
  Serial.print(tc);
  Serial.println(F("°C "));

  Serial.print(F("Temp Farenheit: "));
  Serial.print(tf);
  Serial.println(F("°F"));

  Serial.print(F("Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
  Serial.println("");

  // Display on the LCD
  lcd.clear();
  lcd.print("Temp: ");

  if(getControllerConfig().isMetric) lcd.print(tc);
  else lcd.print(tf);

  lcd.print(String((char)223));

  if(getControllerConfig().isMetric) lcd.print("C");
  else lcd.print("F");

  lcd.setCursor (0,1); // go to start of 2nd line
  lcd.print("Hum:  ");
  lcd.print(h);
  lcd.print("%");


  // Send data to the MySensors Gateway once a SEND_PERIOD
  currentMillis = millis();
  if(currentMillis - previousMillis > SEND_PERIOD) {
      previousMillis = currentMillis;

      Serial.println("Sending to Gateway... ");

      if(getControllerConfig().isMetric) send(msgTemp.set(tc, V_TEMP));
      else send(msgTemp.set(tf, V_TEMP));

      send(msgHum.set(h, V_HUM));

      Serial.println("Done!");
      Serial.println("");
  }

  wait(READ_PERIOD);

  // Display Time if set
  if(gotTime) {
    time_t t = now();
    char buf[50];
    sprintf(buf, "%4d-%02d-%02d      %02d:%02d:%02d ", year(t), month(t), day(t), hour(t), minute(t), second(t));
    displayCustomText(String(buf));
    wait(READ_PERIOD);
  }

  // Display CustomText if set
  if(LCD_TEXT != "") {
    displayCustomText(LCD_TEXT);
  }
}


void displayCustomText(String str) {
    Serial.println(str);
    String str1 = "";
    String str2 = "";
    if(str.length() <= 16) {
      str1 = str;
    } else {
      str1 = str.substring(0,16);
      str2 = str.substring(16);
    }

    Serial.println(str1);
    Serial.println(str2);

    lcd.clear();
    lcd.print(str1);
    lcd.setCursor (0,1);
    lcd.print(str2);

    wait(READ_PERIOD);
  }


void receive(const MyMessage &message) {
  // callback from controller

  if ((message.destination == getNodeId()) and (message.sensor == DISPLAY_CHILD_ID)) {
    LCD_TEXT = message.getString();
    Serial.print("D: ");
    Serial.println(LCD_TEXT);
    if(LCD_TEXT != "") {
      displayCustomText(LCD_TEXT);
      // send(msgDisplay.set(LCD_VAL, V_TEXT));
    }
  }

} // End receive


void receiveTime(unsigned long newTime)
{
  setTime(newTime);
  gotTime = true;

  char buf[50];
  sprintf(buf, "%4d-%02d-%02d %02d:%02d:%02d ", year(newTime), month(newTime), day(newTime), hour(newTime), minute(newTime), second(newTime));
  Serial.println(buf);

} // End receiveTime
