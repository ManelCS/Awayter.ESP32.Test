#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

// Activa o desactiva logs
#define ENABLE_LOGS 1   // 1 = activats, 0 = desactivats

#if ENABLE_LOGS
  #define LOG_PRINT(x)    Serial.print(x)
  #define LOG_PRINTLN(x)  Serial.println(x)
#else
  #define LOG_PRINT(x)
  #define LOG_PRINTLN(x)
#endif


// Configuració del WiFi i MQTT
const char* ssid = "GOUFONE-22539B";
const char* password = "20bellpuig20";
const char *mqtt_server = "test.mosquitto.org";  // Broker públic
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

// Variables globals
String softVersion = "1.0.0";
String scheduledUpdHour = "";   // hora programada "hh:MM"
String scheduledSoftVersion = "";  // versió nova
bool updateScheduled = false;

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

   LOG_PRINT("📩 Missatge rebut al topic ");
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
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) {
      scheduledUpdHour = doc["hour"].as<String>();
      scheduledSoftVersion = doc["softVersion"].as<String>();
      updateScheduled = true;

      // Persistim a memòria
      preferences.putString("updHour", scheduledUpdHour);
      preferences.putString("updateSoftVersion", scheduledSoftVersion);

       LOG_PRINTLN("UPDATE rebut i guardat!");
       LOG_PRINT("Hora programada: ");
       LOG_PRINTLN(scheduledUpdHour);
       LOG_PRINT("Nova versió: ");
       LOG_PRINTLN(scheduledSoftVersion);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
     LOG_PRINT("Intentant connexió MQTT...");
    if (client.connect("ESP32Client1885962")) {
       LOG_PRINTLN("connectat!");
      client.subscribe("device/1885962/reboot");
      client.subscribe("device/1885962/update");
    } else {
       LOG_PRINT("Error, rc=");
       LOG_PRINT(client.state());
       LOG_PRINTLN(" intentem en 5s");
      delay(5000);
    }
  }
}

// --------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(1000);
   LOG_PRINTLN("Inici ESP32 MQTT Demo");

  preferences.begin("storage", false);
  softVersion = preferences.getString("softVersion", "1.0.0");
  scheduledUpdHour = preferences.getString("updHour", "");
  scheduledSoftVersion = preferences.getString("updateSoftVersion", "");
  if (scheduledUpdHour != "" && scheduledSoftVersion != "") {
    updateScheduled = true;
  }

   LOG_PRINT("SoftVersion carregada: ");
   LOG_PRINTLN(softVersion);

  WiFi.begin(ssid, password);
   LOG_PRINT("Connectant a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
     LOG_PRINT(".");
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
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  // Enviar PING cada 5 minuts
  if (now - lastPing > 300000) {
    lastPing = now;
    DynamicJsonDocument doc(128);
    doc["softVersion"] = softVersion;
    doc["statusCode"] = 1;
    String output;
    serializeJson(doc, output);
    client.publish("device/1885962/ping", output.c_str());
     LOG_PRINTLN("📤 PING enviat: " + output);
  }

  // Enviar dades cada 30s
  if (now - lastData > 30000) {
    lastData = now;
    DynamicJsonDocument doc(256);
    doc["currentDateTime"] = getCurrentDateTimeUTC();
    doc["pressure"] = random(5, 100);
    doc["temperature"] = random(105, 605) / 10.0; // 10.5–60.5
    doc["alarm"] = random(0, 2) == 1;
    String output;
    serializeJson(doc, output);
    client.publish("device/1885962/data", output.c_str());
     LOG_PRINTLN("📤 DATA enviada: " + output);
  }

  // Comprovar UPDATE programat
  if (updateScheduled) {
    String currentHourMinute = getCurrentHourMinuteUTC();
    
    if (currentHourMinute == scheduledUpdHour) {
       LOG_PRINTLN("Executant UPDATE programat...");
      softVersion = scheduledSoftVersion;
      preferences.putString("softVersion", softVersion);
      preferences.remove("updHour");
      preferences.remove("updateSoftVersion");
      delay(500);
      ESP.restart();
    }
  }
}
