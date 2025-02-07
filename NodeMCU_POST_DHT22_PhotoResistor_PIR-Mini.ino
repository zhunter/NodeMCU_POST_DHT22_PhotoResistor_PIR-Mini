/*
    HTTP REST API POST/GET over TLS (HTTPS)
    Created By: Zachary Hunter / zachary.hunter@gmail.com
    Created On: May 1, 2024
    Verified Working on NodeMCU & NodeMCU MMINI on ESP8266
      5/1 - Sensors Active:  PIR/Photocell, pending DHT in correct package. 
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Arduino_JSON.h>
#include "certs.h"
#include "DHT.h"

// ESP8266 WIFI Setup
const char* ssid = "AutumnHillsGuest";
const char* password = "Hunter2023";
const char* serverName = "https://www.zachhunter.net/wp-json/wp-arduino-api/v1/arduino";

int LightSensorLastStatus = LOW;
int MotionSensorLastStatus = LOW;
int MotionRetriggerTimeout = (1000 * 60);
unsigned long MotionLastTriggerTime = 0;

// DHT-11's Setup
#define DHTPIN D5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Timer Setup (1000ms = 1second), setup for 30 minutes
unsigned long timerDelay = (1000 * 60 * 30);
unsigned long lastTime = millis() - timerDelay;

X509List cert(cert_ISRG_Root_X1);

float* ReadDHT();

void setup() {
  dht.begin();
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(5000);

  pinMode(D4, OUTPUT);  //Onboard LED
  pinMode(D7, INPUT);   //Passive InfraRed Motion Sensor
  pinMode(D1, INPUT);   //Photocell Resistor w/1M OHM Resistor
  //pinMode(D8, INPUT);   //DHT

  digitalWrite(D4, LOW);  // Turn off LED

  WiFi.begin(ssid, password);

  Serial.print("Connecting ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  // Set time via NTP, as required for x.509 validation
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
  Serial.println("");

  Serial.println("= [ Sensor Startup ]=================================");

  Serial.print("Light Sensor: ");
  Serial.print(digitalRead(D1));
  Serial.println("");

  Serial.print("Motion Sensor: ");
  Serial.print(digitalRead(D7));
  Serial.println("");

  float dhtStartupResults[4];
  ReadDHT(dhtStartupResults);

  Serial.print("Humidity: ");
  Serial.print(dhtStartupResults[1]);
  Serial.println("%");

  Serial.print("Temperature: ");
  Serial.print(dhtStartupResults[3]);
  Serial.println("°F");

  Serial.println("= [ End Sensor Startup ]=============================");
  Serial.println("");
}

void loop() {

  int photocell = digitalRead(D1);
  int pir = digitalRead(D7);

  if (pir) {
    digitalWrite(D4, LOW);
  } else {
    digitalWrite(D4, HIGH);
  }

  // Temp Every x Minutes
  if (millis() > lastTime + timerDelay) {

    float dhtResults[4];
    ReadDHT(dhtResults);

    if (WiFi.status() == WL_CONNECTED) {
      JSONVar myObject;
      String jsonString;

      myObject["sendorValue"] = WiFi.localIP().toString();
      myObject["sensorType"] = "Humidity Sensor";
      myObject["sensorValue"] = dhtResults[1];
      jsonString = JSON.stringify(myObject);
      httpPOSTRequest(serverName, jsonString);

      myObject["sendorValue"] = WiFi.localIP().toString();
      myObject["sensorType"] = "Temperature Sensor";
      myObject["sensorValue"] = dhtResults[3];
      jsonString = JSON.stringify(myObject);
      httpPOSTRequest(serverName, jsonString);

    } else {
      Serial.println("WiFi Disconnected");
    }

    lastTime = millis();
  }

  if (LightSensorLastStatus != photocell) {
    if (WiFi.status() == WL_CONNECTED) {
      JSONVar myObject;
      String jsonString;

      myObject["sendorValue"] = WiFi.localIP().toString();
      myObject["sensorType"] = "Light Sensor";
      myObject["sensorValue"] = photocell ? "ON" : "OFF";
      jsonString = JSON.stringify(myObject);
      httpPOSTRequest(serverName, jsonString);

      LightSensorLastStatus = photocell;

    } else {
      Serial.println("WiFi Disconnected");
    }
  }

  if (MotionSensorLastStatus != pir) {
    if (millis() > MotionLastTriggerTime + MotionRetriggerTimeout) {
      if (WiFi.status() == WL_CONNECTED) {
        JSONVar myObject;
        String jsonString;

        myObject["sendorValue"] = WiFi.localIP().toString();
        myObject["sensorType"] = "Motion Sensor";
        myObject["sensorValue"] = pir ? "ON" : "OFF";
        jsonString = JSON.stringify(myObject);
        httpPOSTRequest(serverName, jsonString);

        MotionSensorLastStatus = pir;

      } else {
        Serial.println("WiFi Disconnected");
      }

      MotionLastTriggerTime = millis();
    }
  }
}

void ReadDHT(float (&dhtResults)[4]) {
  // Read Humidity
  float h = dht.readHumidity();
  dhtResults[0] = h;
  dhtResults[1] = truncf(h * 10) / 10;

  // Read Temperature Fahrenheit
  float t = dht.readTemperature();
  dhtResults[2] = t;
  dhtResults[3] = (t * 1.8) + 32;
}

// POST JSON Data to REST API
String httpPOSTRequest(const char* serverName, String httpRequestData) {
  WiFiClientSecure client;
  HTTPClient http;

  client.setTrustAnchors(&cert);

  // Enable if your having certificate issues
  //client.setInsecure();

  Serial.println("Secure POST Request to: " + String(serverName));
  Serial.println("Payload: " + httpRequestData);

  http.begin(client, serverName);
  http.addHeader("Authorization", "QT1kIG0Dt9u090ODH6bHXvGU");
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(httpRequestData);

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  Serial.println();

  http.end();

  return payload;
}
