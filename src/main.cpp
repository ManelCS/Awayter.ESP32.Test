
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <vector>
#include "../include/WifiAP.h"
#include "../include/storage.h"
#include "../include/log.h"

// Mode Acces Point
#define USE_AP_MODE  true // true = AP mode , false = mode normal


// --------- VARIABLES GLOBALS ----------
Storage storage;

WiFiClient espClient;
PubSubClient client(espClient);

WebServer server(80);
String ssid;
String password;

// Configuració del WiFi i MQTT
const char* fixed_ssid =  "SSID";
const char* fixed_password = "PASSWORD";
const char *mqtt_server = "test.mosquitto.org";  // Broker públic
const int mqtt_port = 1883;

String softVersion = "1.0.0";
String scheduledUpdHour = "";   // hora programada per actualitzar
String scheduledSoftVersion = "";  // versió nova
bool updateScheduled = false;

std::vector<DataMessage> messageQueue;

// Config NTP (hora UTC)
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// --------- FUNCIONS ----------
String getCurrentDateTimeUTC() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "00/00/0000 00:00";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

String getCurrentHourMinuteUTC() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "00:00";
  }
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  return String(buffer);
}

void callback(char *topic, byte *message, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)message[i];

  LOG_PRINT("Missatge rebut al topic ");
  LOG_PRINT(topic);
  LOG_PRINT(": ");
  LOG_PRINTLN(msg);

  if (String(topic) == "device/1885962/reboot") {
    LOG_PRINTLN("Reboot remot rebut!");
    delay(500);
    ESP.restart();
  }

  if (String(topic) == "device/1885962/update") {
    DynamicJsonDocument doc(512);
    if (!deserializeJson(doc, msg)) {
      scheduledUpdHour = doc["hour"].as<String>();
      scheduledSoftVersion = doc["softVersion"].as<String>();
      updateScheduled = true;

      storage.saveUpdateInfo(scheduledUpdHour, scheduledSoftVersion);

      LOG_PRINTLN("UPDATE rebut i guardat!");
       LOG_PRINT("Hora programada: ");
       LOG_PRINTLN(scheduledUpdHour);
       LOG_PRINT("Nova versió: ");
       LOG_PRINTLN(scheduledSoftVersion);
    }
  }
}

void reconnectMQTT() {
  while (WiFi.status() == WL_CONNECTED && !client.connected()) {
    LOG_PRINT("Intentant connexió MQTT...");
    if (client.connect("ESP32Client1885962")) {
      LOG_PRINTLN("connectat!");
      client.subscribe("device/1885962/reboot");
      client.subscribe("device/1885962/update");
    } else {
       LOG_PRINTLN("Error, rc=");
       LOG_PRINT(client.state());
      delay(5000);
    }
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_PRINTLN("WiFi desconnectat! Reintentant...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      LOG_PRINT(".");
    }
    LOG_PRINTLN("\nWiFi recuperat!");
  }
}

void enviarMissatge(const DataMessage &msg) {
  DynamicJsonDocument doc(256);
  doc["currentDateTime"] = msg.timestamp;
  doc["pressure"] = msg.pressure;
  doc["temperature"] = msg.temperature;
  doc["alarm"] = msg.alarm;

  String output;
  serializeJson(doc, output);
  client.publish("device/1885962/data", output.c_str());
  LOG_PRINTLN("DATA enviada: " + output);
}

// --------- SETUP ----------
void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
  storage.begin();  
  delay(1000);
  LOG_PRINTLN("Inici ESP32 MQTT Demo");
  
  softVersion = storage.loadSoftVersion();
  if (storage.loadUpdateInfo(scheduledUpdHour, scheduledSoftVersion)) {
    updateScheduled = true;
  }
  messageQueue = storage.loadQueuedMessages();

   LOG_PRINT("SoftVersion carregada: ");
   LOG_PRINTLN(softVersion);

// ----- Selecció mode WiFi -----
  if (USE_AP_MODE) {
    auto creds = storage.loadWiFiCredentials();
    ssid = creds.first;
    password = creds.second;
    if(ssid=="" || password=="") startAPMode();
  } 
  else {
    ssid = fixed_ssid;
    password = fixed_password;
  }

  WiFi.begin(ssid.c_str(), password.c_str());

  LOG_PRINT("Connectant a WiFi");
  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
     LOG_PRINT(".");
    if (millis() - startTime > 20000) {
      resetWiFiCredentials(); 
    }
  }
  LOG_PRINTLN("\nWiFi connectat!");
   LOG_PRINT("IP: ");
   LOG_PRINTLN(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// --------- LOOP ----------
unsigned long lastPing = 0;
unsigned long lastData = 0;

void loop() {
  // Recuperar connexió WiFi si es perd
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }

  // Recuperar connexió MQTT si cal
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    reconnectMQTT();
  }
  
  client.loop();

  unsigned long now = millis();

  // Enviar PING cada 5 minuts
  if (now - lastPing > 300000 && WiFi.status() == WL_CONNECTED && client.connected()) {
    lastPing = now;
    DynamicJsonDocument doc(128);
    doc["softVersion"] = softVersion;
    doc["statusCode"] = 1;
    String output;
    serializeJson(doc, output);
    client.publish("device/1885962/ping", output.c_str());
    LOG_PRINTLN("PING enviat: " + output);
  }

  // Enviar dades cada 30s
  if (now - lastData > 30000) {
    lastData = now;
    DataMessage msg = {
      getCurrentDateTimeUTC(),
      random(5, 100),
      random(105, 605) / 10.0,
      (random(0, 2) == 1)
    };

    if (WiFi.status() == WL_CONNECTED && client.connected()) {
      enviarMissatge(msg);
    } else {
      messageQueue.push_back(msg);
      storage.saveQueuedMessages(messageQueue);
      LOG_PRINTLN("DATA desada a cua (sense connexió)");
    }
  }

  // Si tenim cua i hi ha connexió: enviar tot
  if (WiFi.status() == WL_CONNECTED && client.connected() && !messageQueue.empty()) {
    for (auto &msg : messageQueue) {
      enviarMissatge(msg);
    }
    LOG_PRINTLN("Tots els missatges pendents enviats!");
    messageQueue.clear();
    storage.clearQueuedMessages();
  }

  // Comprovar UPDATE programat
  if (updateScheduled) {
    String currentHourMinute = getCurrentHourMinuteUTC();
    if (currentHourMinute == scheduledUpdHour) {
      LOG_PRINTLN("Executant UPDATE programat...");
      softVersion = scheduledSoftVersion;
      storage.saveSoftVersion(softVersion);
      storage.clearUpdateInfo();
      delay(500);
      ESP.restart();
    }
  }
}
