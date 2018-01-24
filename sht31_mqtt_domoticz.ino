#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* version = "sht31_mqtt_domoticz 2.0";
const char* date_name = "2018-01-18 neeej";

// SHT31 temperature and humidity sensor, connected to D1, D2
// on Wemos D1 mini. Connection according to
// http://www.esp8266learning.com/wemos-and-sht31.php

// Example on server side for obtaining data from mqtt broker:
// mosquitto_sub -h localhost -t '#' -v

// Wifi settings 
const char* ssid = "*****";
const char* password = "******";

// device id in domoticz
//
// 67 - Garage
// 68 - Sovrum
//
const char* clientID = "68";


// The follwing parameters are only required for fixed IP, otherwise uncomment, also uncomment WiFi.config(ip, gateway, subnet)
//IPAddress ip(172, 16, 0, 37); 
//IPAddress gateway(172, 16, 0, 1);
//IPAddress subnet(255, 255, 255, 0); 

// How long in ms to sleep until next temp check
int sleep = 15000;

// MQTT server and topic settings
const char* mqtt_server = "172.16.0.19";
//const char* mqtt_ID = "TempHum_";
const char* domoticzTopic = "domoticz/in";
const char* outTopic = "esp8266/out";     // info and errors mostly
const char* inTopic = "esp8266/in";
 
Adafruit_SHT31 sht31 = Adafruit_SHT31(); 
WiFiClient espClient;
PubSubClient client(espClient);
 
void setup_wifi()
{
  delay(10);
  Serial.println("setup started ");

  // Connect to WiFi network
  //WiFi.config(ip, gateway, subnet); // Only required for fixed IP
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_sht()
{
  // Connect to SHT31
  if (! sht31.begin(0x44)) 
  {
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Check incoming message using really ugly code
  String nstring = "";
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    nstring = String (nstring + (char)payload[i]);
  }
  nstring = String (nstring + '\0');
  Serial.println("");
  Serial.print("nstring = ");
  Serial.println(nstring);
  int value;
  if (String(topic + '\0' == inTopic)) {
    value = nstring.toInt(); // (long) 0 is returned if unsuccessful)
    if (value > 0 ) {
      // Serial.print("TBD value change to ");
      Serial.print(value);
      Serial.println(" requested");
      // print message
      char msg[50] = "status requested for ";
      strcat(msg, clientID);
      //strcat(msg, ". My ip is ");
      //strcat(msg, String(WiFi.localIP()));
      client.publish(outTopic, msg);
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
      char msg[50] = "new connection setup from sensor ID ";
      strcat(msg, clientID);
      client.publish(outTopic, msg);
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
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
  setup_sht();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

void loop() 
{
  
  // Check if a client has connected
  if (!client.connected()) {
    reconnect();
  }

  // Perform PubSubClient tasks
  client.loop();

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
    Serial.print("Temp *C = "); Serial.println(temperature);
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
    char msg[50] = "Failed to read temperature on ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println("Failed to read temperature");
  }
  strcat(mqtt_msg, ";");
 
  if (! isnan(humidity)) 
  {
    // Humidity read
    Serial.print("Hum. % = "); Serial.println(humidity);
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

    Serial.print("Hum status = "); Serial.println(hum_stat);
    char buf[8];
    sprintf(buf,"%d",hum_stat);
    strcat(mqtt_msg, buf);
  } 
  else 
  {
    error++;
    char msg[50] = "Failed to read humidity on ID ";
    strcat(msg, clientID);
    client.publish(outTopic, msg);
    Serial.println("Failed to read humidity");
  }

  strcat(mqtt_msg, "\" }");

  // publish to topic, unless both have failed
  if (error != 2)
  {
    client.publish(domoticzTopic, mqtt_msg);
  }
  
  Serial.println();
  delay(sleep);
}
