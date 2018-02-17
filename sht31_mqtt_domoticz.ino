#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* version = "sht31_mqtt_domoticz 2.1";
const char* date_name = "2018-02-17 mike";

// SHT31 temperature and humidity sensor, connected to D1, D2
// on Wemos D1 mini. Connection according to
// http://www.esp8266learning.com/wemos-and-sht31.php

// Example use from server side for obtaining data from mqtt broker:
// mosquitto_sub -h localhost -t '#' -v

// Wifi settings 
const char* ssid = "*****";
const char* password = "*****";

// device id in domoticz
//
// 67 - Garage
// 68 - Sovrum
//
const char* clientID = "67";

// The following parameters are only required for fixed IP,
// make sure WiFi.config() is uncommented as well for this to work
IPAddress ip(172, 16, 0, 67);
IPAddress gateway(172, 16, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(172, 16, 0, 18);

// How long in seconds to sleep until next check
int sleepDelay = 10;

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
  client.setServer(mqtt_server, 1883);
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
  char mqtt_msg[60] = "{ \"idx\" : ";
  strcat(mqtt_msg, clientID);
  strcat(mqtt_msg, ", \"nvalue\" : 0, \"svalue\" : \"");
  
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
    strcat(mqtt_msg, msg);
  } 
  else 
  {
    error++;
    char msg[50] = "Failed to read temperature, for client ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println(msg);
  }
  strcat(mqtt_msg, ";");
 
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
    strcat(mqtt_msg, msg);
    strcat(mqtt_msg, ";");
    
    // Humidity status calculation
    // 0=Normal
    // 1=Comfortable
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
    }

    Serial.println("Hum. status = " + String(hum_stat));
    char buf[8];
    sprintf(buf,"%d",hum_stat);
    strcat(mqtt_msg, buf);
  } 
  else 
  {
    error++;
    char msg[50] = "Failed to read humidity, for client ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println(msg);
  }

  strcat(mqtt_msg, "\" }");

  // publish to topic, unless both have failed
  if (error != 2)
  {
    client.publish(domoticzTopic, mqtt_msg);
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

  // Get data from temp sensor (temperature + humidity) and send to mqtt queue
  sht31_temp_hum();

  delay(sleepDelay * 1000);
}
