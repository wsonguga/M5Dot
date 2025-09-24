## 📁 M5 Stick-C 
### Summary
The Sensor Device for the project was selected to be an M5 stick-C, an off the shelf microcontroller with an ESP32 chip and a 6 axis inertial measurement unit.   

On the device, a script is written to collect, synthesize, and publish the data to the server.

## ⚙️ Installation Instructions
The Installation for this product is as easy as two steps:
1. Pick which arduino script you want to run on the M5 Stick-C. Each script has a different purpose. **The script included outputs all 6 axis data**
2. Using the [arduino IDE](https://www.arduino.cc/en/software), upload the script to the M5 Stick-C. Be sure to change the wifi settings, port, and broker. Troubleshouting tips can be found below.


## Troubleshooting 
1. Change the Topic, Broker, and Port to match the server's settings.
```
const char* mqtt_broker = "172.21.127.197";
const char* topic = "M5Test";
const int mqtt_port = 1883;
```
2. The onboard battery is quite small. If the M5 Stick-C is not plugged in, it will only last an hours maximum.