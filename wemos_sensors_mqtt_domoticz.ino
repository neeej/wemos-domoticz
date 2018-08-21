#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* version = "mqtt_domoticz 2.1";
const char* date_name = "2018-02-17 mike";

// works for:
// SHT31 temperature and humidity sensor, connected to D1, D2
// SHT21 temperature and humidity sensor, connected to D1, D2
// For light sensor like lx1972
//
// on Wemos D1 mini. Connection according to
// http://www.esp8266learning.com/wemos-and-sht31.php

// Example on server side for obtaining data from mqtt broker:
// mosquitto_sub -h localhost -t '#' -v

// This code will post directly onto a domoticz queue (mqtt), for the specified device

// Wifi settings 
const char* ssid = "*****";
const char* password = "*****";

// client device id
// and id's in domoticz
//
// client: 1 - Garage
// tempID: 67
//
// client: 2 - Bedroom
// tempID: 68
// luxID: 69
//
// moistID: 70
//
const char* clientID = "2";
int tempID = 68;  // send temp data to this ID in domoticz
int luxID = 69;   // send LUX data to this ID in domoticz
int moistID = 70; // send soil moisture data to this ID in domoticz

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

// MQTT server and topic settings
const char* mqtt_server = "172.16.0.19";
//const char* mqtt_ID = "TempHum_";
const char* domoticzTopic = "domoticz/in";
const char* outTopic = "esp8266/out";     // info and errors to here
const char* inTopic = "esp8266/in";       // subscribe to this topic
 
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

  // Connect to WiFi network
  //WiFi.config(ip, gateway, subnet, dns); // Only required for fixed IP
  WiFi.begin(ssid, password);
  
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
    char msg[50] = "Error: Couldn't find SHT31..";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println(msg);
    while (1) delay(1);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Check incoming message using really ugly code
  String nstring = "";
  Serial.println("Message arrived [" + String(topic) + "] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    nstring = String (nstring + (char)payload[i]);
  }
  nstring = String (nstring + '\0');
  Serial.println("");
  Serial.println("nstring = " + String(nstring));
  int value;
  if (String(topic + '\0' == inTopic)) {
    value = nstring.toInt(); // (long) 0 is returned if unsuccessful)
    if (value > 0 ) {
      Serial.println(String(value) + " requested");
      // print message to mqtt
      String data = "status requested for " + String(clientID) + ". My ip is " + WiFi.localIP().toString();
      int length = data.length();
      char msgBuffer[length];
      data.toCharArray(msgBuffer,length+1);
      client.publish(outTopic, msgBuffer);
    }
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
    if (client.connect(mqtt_ID)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      char msg[50] = "this is a new connection setup, from client ID ";
      strcat(msg, clientID);
      client.publish(outTopic, msg);
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      Serial.println("failed, rc=" + String(client.state()) + ", trying again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  //pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);X
  client.setCallback(callback);
  setup_sht();
}

void sht31_temp_hum()
{
  
  // Get data
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  int error = 0;
  
  // Set start of mqtt message
  // Data will be: TEMP;HUM;HUM_STAT
  String mqtt_msg = "{ \"idx\" : " + String(tempID) + ", \"nvalue\" : 0, \"svalue\" : \"";
  
  if (! isnan(temperature)) 
  {
    // Temperature read
    Serial.println("Temp *C = " + String(temperature));
    char msg[25]; 
    dtostrf(temperature , 5, 1, msg); // width including sign, "." . etc., precision/decimals
    // remove blanks
    int pos = 0;
    for (int i = 0; i < sizeof(msg); i ++) {
      if (msg[i] != ' ') {
        msg[pos] = msg[i];
        pos++;
      }
    }
    // add data to mqtt message
    mqtt_msg += String(msg);
  } 
  else 
  {
    error++;
    char msg[50] = "Failed to read temperature, for client ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println(msg);
  }
  mqtt_msg += ";";
 
  if (! isnan(humidity)) 
  {
    // Humidity read
    Serial.println("Hum. %  = " + String(humidity));
    char msg[25]; 
    dtostrf(humidity , 5, 1, msg); // width including sign, "." etc., precision/decimals
    // remove blanks
    int pos = 0;
    for (int i = 0; i < sizeof(msg); i ++) {
      if (msg[i] != ' ') {
        msg[pos] = msg[i];
        pos++;
      }
    }
    // add data to mqtt message
    mqtt_msg += String(msg) + ";";
    
    // Humidity status calculation
    // 0=Normal
    // 1=ComfortableX
    // 2=Dry
    // 3=Wet
    int hum_stat = 0;
    if (humidity < 30){
      hum_stat = 2;
    }
    else if (humidity > 70){
      hum_stat = 3;
    }
    else if (humidity > 45 && humidity < 50){
      hum_stat = 1;
    }X

    Serial.println("Hum. status = " + String(hum_stat));
    mqtt_msg += String(hum_stat);
  } 
  else 
  {
    error++;
    char msg[50] = "Failed to read humidity, for client ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println(msg);
  }

  mqtt_msg += "\" }";

  // publish to topic, unless both have failed
  if (error != 2)
  {
    int length = mqtt_msg.length();
    char msgBuffer[length];
    mqtt_msg.toCharArray(msgBuffer,length+1);
    client.publish(domoticzTopic, msgBuffer);
  }
  
  Serial.println();
}

void soil_moisture_sensor()
{
  int moist;
  moist = analogRead(sensor_pin);

  if (! isnan(moist))
  {
    Serial.println("Moisture Sensor Value " + String(moist));
    String mqtt_msg = "{ \"idx\" : " + String(moistID) + ", \"nvalue\" : 0, \"svalue\" : \"" + moist + "\" }";

    // publish to topic
    int length = mqtt_msg.length();
    char msgBuffer[length];
    mqtt_msg.toCharArray(msgBuffer,length+1);
    client.publish(domoticzTopic, msgBuffer);
  } 
  else 
  {
    char msg[60] = "Failed to read moisture from sensor, for client ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println(msg);
  }
  
  Serial.println();
}

void light_sensor()
{
  int lux;
  lux = analogRead(sensor_pin);

  if (! isnan(lux))
  {
    Serial.println("Light Sensor Value " + String(lux));
    String mqtt_msg = "{ \"idx\" : " + String(luxID) + ", \"nvalue\" : 0, \"svalue\" : \"" + lux + "\" }";

    // publish to topic
    int length = mqtt_msg.length();
    char msgBuffer[length];
    mqtt_msg.toCharArray(msgBuffer,length+1);
    client.publish(domoticzTopic, msgBuffer);
  } 
  else 
  {
    char msg[60] = "Failed to read lux from light sensor, for client ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println(msg);
  }
  
  Serial.println();
}

void loop()
{
  // Check if MQTT client is connected, otherwise reconnect
  if (!client.connected()) {
    reconnect();
  }

  // maintain the MQTT connection and check for incoming messages
  client.loop();

  //
  // Get data from sensors and send to mqtt queue
  //
  // For temp sensor (temperature + humidity)
  //sht31_temp_hum();
  //sht21_temp_hum();
  
  // For light sensor, like lx1972
  light_sensor();
  
  // For soil moisture sensor (LM393 compatible chip)
  //soil_moisture_sensor();

  delay(sleepDelay * 1000);
}
