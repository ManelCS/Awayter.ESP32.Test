#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <vector>

#define ENABLE_LOGS 1
#if ENABLE_LOGS
  #define LOG_PRINT(x)    Serial.print(x)
  #define LOG_PRINTLN(x)  Serial.println(x)
#else
  #define LOG_PRINT(x)
  #define LOG_PRINTLN(x)
#endif

// ----- Globals -----
Preferences preferences;
WebServer server(80);

String ssid;
String password;

// MQTT
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

// NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Programació update
String softVersion = "1.0.0";
String scheduledUpdHour = "";
String scheduledSoftVersion = "";
bool updateScheduled = false;

// Cua missatges pendents
struct DataMessage {
  String timestamp;
  float pressure;
  float temperature;
  bool alarm;
};
std::vector<DataMessage> messageQueue;

// HTML per configurar WiFi
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Config WiFi ESP32</title>
</head>
<body>
  <h2>Configuració WiFi ESP32</h2>
  <form action="/save" method="POST">
    SSID: <input type="text" name="ssid"><br><br>
    Password: <input type="password" name="password"><br><br>
    <input type="submit" value="Guardar">
  </form>
</body>
</html>
)rawliteral";

// ----- Funcions Web -----
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    ssid = server.arg("ssid");
    password = server.arg("password");

    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", password);

    server.send(200, "text/html", "<h3>Credencials guardades! Reiniciant...</h3>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "Falten camps!");
  }
}

// ----- AP Mode -----
void startAPMode() {
  LOG_PRINTLN("No hi ha WiFi configurat → Obrint AP mode");
  WiFi.softAP("ESP32_SETUP", "12345678");
  LOG_PRINT("Connecta't a la WiFi: ESP32_SETUP amb pass: 12345678\nIP: ");
  LOG_PRINTLN(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();

  while (true) {
    server.handleClient();
    delay(10);
  }
}

// ----- Funcions MQTT i missatges -----
String getCurrentDateTimeUTC() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00/00/0000 00:00";
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

String getCurrentHourMinuteUTC() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00";
  char buffer[6];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  return String(buffer);
}

void printQueue() {
  LOG_PRINTLN("📋 Contingut cua missatges pendents:");
  if (messageQueue.empty()) { LOG_PRINTLN("  (buit)"); return; }
  for (size_t i = 0; i < messageQueue.size(); i++) {
    LOG_PRINT("  #"); LOG_PRINT(i);
    LOG_PRINT(" -> "); LOG_PRINT(messageQueue[i].timestamp);
    LOG_PRINT(", P="); LOG_PRINT(messageQueue[i].pressure);
    LOG_PRINT(", T="); LOG_PRINT(messageQueue[i].temperature);
    LOG_PRINT(", A="); LOG_PRINTLN(messageQueue[i].alarm ? "true" : "false");
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
  LOG_PRINTLN("📤 DATA enviada: " + output);
}

// MQTT callback
void callback(char *topic, byte *message, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)message[i];

  LOG_PRINT("📩 Missatge rebut al topic ");
  LOG_PRINT(topic);
  LOG_PRINT(": ");
  LOG_PRINTLN(msg);

  if (String(topic) == "device/1885962/reboot") { ESP.restart(); }

  if (String(topic) == "device/1885962/update") {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, msg);
    if (!error) {
      scheduledUpdHour = doc["hour"].as<String>();
      scheduledSoftVersion = doc["softVersion"].as<String>();
      updateScheduled = true;
      preferences.putString("updHour", scheduledUpdHour);
      preferences.putString("updateSoftVersion", scheduledSoftVersion);
      LOG_PRINTLN("UPDATE rebut i guardat!");
    }
  }
}

// ----- Reconnect WiFi/MQTT -----
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_PRINTLN("WiFi desconnectat! Reintentant...");
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis()-start<15000) { delay(500); LOG_PRINT("."); }
    if (WiFi.status() == WL_CONNECTED) LOG_PRINTLN("\n✅ WiFi recuperat!");
    else LOG_PRINTLN("\n⚠️ No s'ha pogut connectar WiFi");
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
      LOG_PRINT("Error rc="); LOG_PRINT(client.state());
      LOG_PRINTLN(" intentem en 5s"); delay(5000);
    }
  }
}

// ----- SETUP -----
void setup() {
  Serial.begin(115200);
  preferences.begin("storage", false);

  ssid = preferences.getString("wifi_ssid", "");
  password = preferences.getString("wifi_pass", "");

  softVersion = preferences.getString("softVersion", "1.0.0");
  scheduledUpdHour = preferences.getString("updHour", "");
  scheduledSoftVersion = preferences.getString("updateSoftVersion", "");
  if (scheduledUpdHour != "" && scheduledSoftVersion != "") updateScheduled = true;

  if (ssid == "" || password == "") startAPMode();

  LOG_PRINT("Connectant a WiFi: "); LOG_PRINTLN(ssid);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-start<15000) { delay(500); LOG_PRINT("."); }
  if (WiFi.status() != WL_CONNECTED) startAPMode();
  LOG_PRINTLN("WiFi connectat!"); LOG_PRINT("IP: "); LOG_PRINTLN(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ----- LOOP -----
unsigned long lastPing = 0;
unsigned long lastData = 0;

void loop() {
  reconnectWiFi();
  reconnectMQTT();
  client.loop();

  unsigned long now = millis();

  // PING cada 5 min
  if (now - lastPing > 300000 && WiFi.status()==WL_CONNECTED && client.connected()) {
    lastPing = now;
    DynamicJsonDocument doc(128);
    doc["softVersion"]=softVersion; doc["statusCode"]=1;
    String output; serializeJson(doc, output);
    client.publish("device/1885962/ping", output.c_str());
    LOG_PRINTLN("📤 PING enviat: "+output);
  }

  // Dades cada 30s
  if (now - lastData > 30000) {
    lastData = now;
    DataMessage msg = { getCurrentDateTimeUTC(), random(5,100), random(105,605)/10.0, random(0,2)==1 };
    if (WiFi.status()==WL_CONNECTED && client.connected()) enviarMissatge(msg);
    else { messageQueue.push_back(msg); LOG_PRINTLN("💾 DATA desada a cua"); printQueue(); }
  }

  // Enviar cua si hi ha connexió
  if (WiFi.status()==WL_CONNECTED && client.connected() && !messageQueue.empty()) {
    for (auto &msg : messageQueue) enviarMissatge(msg);
    messageQueue.clear(); LOG_PRINTLN("✅ Cua buidada"); printQueue();
  }

  // UPDATE programat
  if (updateScheduled) {
    String cur = getCurrentHourMinuteUTC();
    if (cur == scheduledUpdHour) {
      LOG_PRINTLN("Executant UPDATE...");
      softVersion = scheduledSoftVersion;
      preferences.putString("softVersion", softVersion);
      preferences.remove("updHour"); preferences.remove("updateSoftVersion");
      delay(500); ESP.restart();
    }
  }
}
