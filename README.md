# wemos HA
Publish temp etc to MQTT to use from home assistant

Subscribe to the topics with:
```
- platform: mqtt
  unique_id: "67temp"
  name: "67 - Temperature"
  state_topic: "sensor/67/temperature"
  qos: 0
  unit_of_measurement: "ºC"
  
- platform: mqtt
  unique_id: "67hum"
  name: "67 - Humidity"
  state_topic: "sensor/67/humidity"
  qos: 0
  unit_of_measurement: "%"
```

# pre-req
https://averagemaker.com/2018/03/wemos-d1-mini-setup.html
https://github.com/esp8266/Arduino#installing-with-boards-manager
https://community.home-assistant.io/t/report-the-temperature-with-esp8266-to-mqtt/42489

# wemos-domoticz
Send data from sensors on Wemos D1 Mini to Domoticz, using MQTT

Got the following sensors working so far:
 - SHT31
 - SHT21
 - lx1972

Untested:
 - LM393
