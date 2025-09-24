#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "time.h"
#include "esp_eap_client.h"
#include <M5Unified.h>
#include <ArduinoJson.h> // NEW: Include the ArduinoJson library

// --- WIFI & MQTT CONFIGURATION ---
char* EAP_IDENTITY = "your uga ID";
char* EAP_PASSWORD = "password";
char* ssid = "eduroam";

const char* mqtt_broker = "test.mosquitto.org";
const char* topic = "shake";
const int mqtt_port = 1883;

// --- IMU & TIMING ---
float accX = 0.0F, accY = 0.0F, accZ = 0.0F;
float gyroX = 0.0F, gyroY = 0.0F, gyroZ = 0.0F;
String macAddress;
TaskHandle_t collectorTaskHandle;
TaskHandle_t senderTaskHandle;
TickType_t xLastWakeTime;
double startTime, startMil;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int daylightOffset_sec = 0;
const int numDataPoints = 100;
const int samplePeriord = 10; // 10 ms == 100hz
const int sampleFrequency = 1000/samplePeriord;

// --- DATA STRUCTURE ---
struct RowData {
  float accXArray[numDataPoints];
  float accYArray[numDataPoints];
  float accZArray[numDataPoints];
  float gyroXArray[numDataPoints];
  float gyroYArray[numDataPoints];
  float gyroZArray[numDataPoints];
  String packetStartEpoch;
};

// --- DOUBLE BUFFERING IMPLEMENTATION ---
RowData dataBuffers[2];
volatile int activeBufferIndex = 0;
volatile int readyBufferIndex = -1;
volatile int dataPointCount = 0;

// --- MQTT CLIENT ---
WiFiClient espClient;
PubSubClient client(espClient);

// NEW: Helper function to add a float array to a JsonArray with reduced precision
void addFloatArrayToJson(JsonArray& jsonArr, float* dataArr, int points) {
  for (int i = 0; i < points; i++) {
    // MODIFIED: Round to 3 decimal places to reduce payload size
    jsonArr.add(round(dataArr[i] * 1000) / 1000.0);
  }
}

// MODIFIED: This function now creates a JSON payload instead of a custom string.
String createJsonPayload(RowData& data, String macAddress, int interval) {
  // Create a JSON document. The size is critical and needs to accommodate all data.
  // This size should be sufficient for 100 data points with 3 decimal places.
  StaticJsonDocument<4000> doc;

  // Add metadata
  doc["mac"] = macAddress;
  doc["epoch"] = data.packetStartEpoch;
  doc["interval_ms"] = interval;

  // Create JSON arrays for each sensor axis
  JsonArray accX = doc.createNestedArray("accX");
  JsonArray accY = doc.createNestedArray("accY");
  JsonArray accZ = doc.createNestedArray("accZ");
  JsonArray gyroX = doc.createNestedArray("gyroX");
  JsonArray gyroY = doc.createNestedArray("gyroY");
  JsonArray gyroZ = doc.createNestedArray("gyroZ");

  // Populate the arrays with sensor data
  addFloatArrayToJson(accX, data.accXArray, numDataPoints);
  addFloatArrayToJson(accY, data.accYArray, numDataPoints);
  addFloatArrayToJson(accZ, data.accZArray, numDataPoints);
  addFloatArrayToJson(gyroX, data.gyroXArray, numDataPoints);
  addFloatArrayToJson(gyroY, data.gyroYArray, numDataPoints);
  addFloatArrayToJson(gyroZ, data.gyroZArray, numDataPoints);

  // Serialize the JSON object into a String
  String output;
  serializeJson(doc, output);
  return output;
}


// Data collection task running on Core 0
void collectorTask(void* pvParameters) {
  xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    M5.Imu.getGyroData(&gyroX, &gyroY, &gyroZ);
    M5.Imu.getAccelData(&accX, &accY, &accZ);

    if(dataPointCount < numDataPoints){
      if(dataPointCount == 0){
        double syncMil = (millis() - startMil) / 1000.0;
        double outTime = startTime + syncMil;
        dataBuffers[activeBufferIndex].packetStartEpoch = String(outTime, 3);
      }
      dataBuffers[activeBufferIndex].gyroXArray[dataPointCount] = gyroX;
      dataBuffers[activeBufferIndex].gyroZArray[dataPointCount] = gyroY;
      dataBuffers[activeBufferIndex].gyroYArray[dataPointCount] = gyroZ;
      dataBuffers[activeBufferIndex].accXArray[dataPointCount] = accX;
      dataBuffers[activeBufferIndex].accYArray[dataPointCount] = accY;
      dataBuffers[activeBufferIndex].accZArray[dataPointCount] = accZ;
      dataPointCount++;
    }
    else {
      readyBufferIndex = activeBufferIndex;
      activeBufferIndex = 1 - activeBufferIndex;
      dataPointCount = 0;
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(samplePeriord));
  }
}

void sendMQTT(String inputString) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected. Restarting ESP");
    ESP.restart();
  }

  if (!client.connected()) {
    String client_id = "esp32-client-" + macAddress;
    if (client.connect(client_id.c_str())) {
      Serial.println("MQTT reconnected.");
    } else {
      delay(1000);
      return;
    }
  }
  client.publish(topic, inputString.c_str());
}

// Dedicated task for sending data on Core 1
void senderTask(void* pvParameters) {
  for (;;) {
    if (readyBufferIndex != -1) {
      // MODIFIED: Create the new JSON payload
      String jsonMessage = createJsonPayload(dataBuffers[readyBufferIndex], macAddress, samplePeriord);

      // MODIFIED: Debugging serial prints are now commented out to reduce the gap
      Serial.println("-----------------------------------");
      Serial.print("Sender Task: Sending data with start time: ");
      Serial.println(dataBuffers[readyBufferIndex].packetStartEpoch);
      // Serial.print("Sender Task: Packet length: ");
      // Serial.println(jsonMessage.length());

      sendMQTT(jsonMessage);
      readyBufferIndex = -1; // Reset the flag

    }
    // vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Imu.init();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_IDENTITY, EAP_PASSWORD);
  Serial.print("Connecting to network: ");
  Serial.println(ssid);

  while ((WiFi.status() != WL_CONNECTED)) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("\nConnected to WiFi");
  macAddress = WiFi.macAddress();
  M5.Display.clear();
  M5.Display.setTextSize(4);
  M5.Display.setCursor(10, 30);
  M5.Display.setCursor(5, 5);
  M5.Display.setTextSize(2);
  M5.Display.printf("Sampling Frequency: %dHz\n", sampleFrequency);
  M5.Display.printf("WiFi SSID: %s\n", ssid);
  M5.Display.printf("MQTT Broker: %s\n", mqtt_broker);
  M5.Display.printf("MAC: %s\n", macAddress.c_str());
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 30);

  client.setServer(mqtt_broker, mqtt_port);
  // This buffer size is important. 4096 is required for the large JSON payload.
  client.setBufferSize(4096);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time, retrying...");
    delay(1000);
  }
  startTime = time(nullptr);
  startMil = millis();

  xTaskCreatePinnedToCore(collectorTask, "Collector", 10000, NULL, 1, &collectorTaskHandle, 0);
  xTaskCreatePinnedToCore(senderTask, "Sender", 10000, NULL, 1, &senderTaskHandle, 1);
}

void loop() {
  M5.update();
  client.loop(); // Keep MQTT connection stable
}