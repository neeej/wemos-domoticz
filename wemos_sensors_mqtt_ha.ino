#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Settings
#define wifi_ssid ""
#define wifi_password ""
#define mqtt_server ""
#define mqtt_user ""
#define mqtt_password ""

// will be for example: sensor/67/temperature
const char* mqtt_topic = "sensor/";
const char* clientID = "67";

// version 3.0 2022-08-21

// works for:
// SHT31 temperature and humidity sensor, connected to D1, D2
// SHT21 temperature and humidity sensor, connected to D1, D2
// For light sensor like lx1972
//
// on Wemos D1 mini. Connection according to
// http://www.esp8266learning.com/wemos-and-sht31.php

// Example on server side for obtaining data from mqtt broker:
// mosquitto_sub -h localhost -t '#' -v



// The following parameters are only required for fixed IP,
// make sure WiFi.config() is uncommented as well for this to work
IPAddress ip(172, 16, 0, 67);
IPAddress gateway(172, 16, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(172, 16, 0, 18);

// How long in seconds to sleep until next check
// MQTT has 15 seconds timeout as default, so if >14, please set timeout for MQTT client (or be aware that it will reconnect every time)
int sleepDelay = 10;

// analog sensor should are connected to Analog 0
int sensor_pin = A0;
 
Adafruit_SHT31 sht31 = Adafruit_SHT31(); 
WiFiClient espClient;
PubSubClient client(espClient);


/*
 * Functions etc
 */
void setup_wifi()
{
  delay(10);
  Serial.println("WiFi setup started");
  Serial.print("Connecting to WiFi ");
  Serial.println(wifi_ssid);

  // Connect to WiFi network
  //WiFi.config(ip, gateway, subnet, dns); // Only required for fixed IP
  WiFi.begin(wifi_ssid, wifi_password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address:");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

void setup_sht()
{
  // Connect to SHT31
  if (! sht31.begin(0x44)) 
  {
    char msg[50] = "Error: Couldn't find SHT31.. ID";
    strcat(msg, clientID);
    Serial.println(msg);
    while (1) delay(1);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // create identifier for mqtt
    char mqtt_ID[20] = "ESP8266Client-";
    strcat(mqtt_ID, clientID);
    // Attempt to connect
    if (client.connect(mqtt_ID, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // ... and resubscribe
      //client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 10 seconds");
      // Wait 10 seconds before retrying
      delay(10000);
    }
  }
}

void setup()
{
  //pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
  setup_sht();
}

// moist sensor
//  float newMoist = analogRead(sensor_pin);

//  if (checkBound(newMoist, moist, diff)) {
//    moist = newMoist;
//    Serial.println("Moisture Sensor Value " + String(moist));
//    float topic = String(mqtt_topic) + "moisture";
//    client.publish(topic, String(moist).c_str(), true);
//  }

// light sensor
//  float newLux = analogRead(sensor_pin);

//  if (checkBound(newLux, lux, diff)) {
//    lux = newLux;
//    Serial.println("Light Sensor Value " + String(lux));
//    float topic = String(mqtt_topic) + "lux";
//    client.publish(topic, String(lux).c_str(), true);
//  } 


bool checkBound(float newValue, float prevValue, float maxDiff) {
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}
long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
// how much should it differ between checks to report new values
float tempDiff = 0.1;
float humDiff = 0.3;

void loop()
{
  // Check if MQTT client is connected, otherwise reconnect
  if (!client.connected()) {
    reconnect();
  }

  // maintain the MQTT connection and check for incoming messages
  client.loop();

  // temp sensors
  float newTemp = sht31.readTemperature();
  float newHum = sht31.readHumidity();

  char topic[100];

  if (checkBound(newTemp, temp, tempDiff)) {
    temp = newTemp;
    Serial.print("New temperature: ");
    Serial.println(String(temp).c_str());
    strcpy(topic, mqtt_topic);
    strcat(topic, clientID);
    strcat(topic, "/temperature");
    client.publish(topic, String(temp).c_str(), true);
  }
  if (checkBound(newHum, hum, humDiff)) {
    hum = newHum;
    Serial.print("New humidity: ");
    Serial.println(String(hum).c_str());
    strcpy(topic, mqtt_topic);
    strcat(topic, clientID);
    strcat(topic, "/humidity");
    client.publish(topic, String(hum).c_str(), true);
  }

  delay(sleepDelay * 1000);
}
