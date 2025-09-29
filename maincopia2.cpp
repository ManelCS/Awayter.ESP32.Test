#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <vector>
#include "WiFiAP.h"

// -------- CONFIGURACIÓ --------
#define ENABLE_LOGS 1
#define USE_AP_MODE  true   // true = AP mode amb portal web, false = mode normal amb SSID/password fix

#if ENABLE_LOGS
  #define LOG_PRINT(x)    Serial.print(x)
  #define LOG_PRINTLN(x)  Serial.println(x)
#else
  #define LOG_PRINT(x)
  #define LOG_PRINTLN(x)
#endif

Preferences preferences;
WebServer server(80);
String ssid;
String password;

// Mode normal
const char* fixed_ssid = "S23 Ultra de Manel";
const char* fixed_password = "mcs123456";

// MQTT
const char *mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

// NTP
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Update programat
String softVersion = "1.0.0";
String scheduledUpdHour = "";
String scheduledSoftVersion = "";
bool updateScheduled = false;

// Cua de missatges
struct DataMessage {
  String timestamp;
  float pressure;
  float temperature;
  bool alarm;
};
std::vector<DataMessage> messageQueue;

// -------- FUNCIONS AUXILIARS --------
String getCurrentDateTimeUTC() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00/00/0000 00:00";
  char buffer[20]; strftime(buffer,sizeof(buffer),"%d/%m/%Y %H:%M",&timeinfo);
  return String(buffer);
}
String getCurrentHourMinuteUTC() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "00:00";
  char buffer[6]; strftime(buffer,sizeof(buffer),"%H:%M",&timeinfo);
  return String(buffer);
}

// -------- FUNCIONS MQTT --------
void callback(char *topic, byte *message, unsigned int length){
  String msg;
  for(unsigned int i=0;i<length;i++) msg+=(char)message[i];
  LOG_PRINT("Missatge rebut al topic "); LOG_PRINT(topic); LOG_PRINT(": "); LOG_PRINTLN(msg);

  if(String(topic)=="device/1885962/reboot"){ ESP.restart(); }

  if(String(topic)=="device/1885962/update"){
    DynamicJsonDocument doc(512);
    if(!deserializeJson(doc,msg)){
      scheduledUpdHour = doc["hour"].as<String>();
      scheduledSoftVersion = doc["softVersion"].as<String>();
      updateScheduled = true;
      preferences.putString("updHour", scheduledUpdHour);
      preferences.putString("updateSoftVersion", scheduledSoftVersion);
      LOG_PRINTLN("UPDATE rebut i guardat!");
    }
  }
}

void reconnectMQTT(){
  while(WiFi.status()==WL_CONNECTED && !client.connected()){
    LOG_PRINT("Intentant connexió MQTT...");
    if(client.connect("ESP32Client1885962")){
      LOG_PRINTLN("connectat!");
      client.subscribe("device/1885962/reboot");
      client.subscribe("device/1885962/update");
    }else{
      LOG_PRINT("Error, rc="); LOG_PRINT(client.state()); LOG_PRINTLN(" intentem en 5s");
      delay(5000);
    }
  }
}

void reconnectWiFi(){
  if(WiFi.status()!=WL_CONNECTED){
    LOG_PRINTLN("WiFi desconnectat! Reintentant...");
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    while(WiFi.status()!=WL_CONNECTED){
      delay(500); LOG_PRINT(".");
    }
    LOG_PRINTLN("\nWiFi recuperat!");
  }
}

void enviarMissatge(const DataMessage &msg){
  DynamicJsonDocument doc(256);
  doc["currentDateTime"]=msg.timestamp;
  doc["pressure"]=msg.pressure;
  doc["temperature"]=msg.temperature;
  doc["alarm"]=msg.alarm;
  String output; serializeJson(doc,output);
  client.publish("device/1885962/data",output.c_str());
  LOG_PRINTLN("DATA enviada: "+output);
}

// -------- SETUP --------
void setup(){
  Serial.begin(115200); delay(1000);
  LOG_PRINTLN("Inici ESP32 MQTT Demo");
  preferences.begin("storage",false);

  softVersion = preferences.getString("softVersion","1.0.0");
  scheduledUpdHour = preferences.getString("updHour","");
  scheduledSoftVersion = preferences.getString("updateSoftVersion","");
  if(scheduledUpdHour!="" && scheduledSoftVersion!="") updateScheduled=true;

  // ----- Selecció mode WiFi -----
  if(USE_AP_MODE){
    ssid = preferences.getString("wifi_ssid","");
    password = preferences.getString("wifi_pass","");
    if(ssid=="" || password=="") startAPMode(); // AP amb portal web
  } else {
    ssid = fixed_ssid;
    password = fixed_password;
  }

  // Connectar WiFi
  WiFi.begin(ssid.c_str(),password.c_str());
  LOG_PRINT("Connectant a WiFi");
  while(WiFi.status()!=WL_CONNECTED){ delay(500); LOG_PRINT("."); }
  LOG_PRINTLN("\nWiFi connectat! IP: "); LOG_PRINTLN(WiFi.localIP());

  configTime(gmtOffset_sec,daylightOffset_sec,ntpServer);

  // Config MQTT
  client.setServer(mqtt_server,mqtt_port);
  client.setCallback(callback);
}

// -------- LOOP --------
unsigned long lastPing=0;
unsigned long lastData=0;

void loop(){
  // Recuperar connexió
  if(WiFi.status()!=WL_CONNECTED) reconnectWiFi();
  if(WiFi.status()==WL_CONNECTED && !client.connected()) reconnectMQTT();
  client.loop();

  unsigned long now=millis();

  // PING 5min
  if(now-lastPing>300000 && WiFi.status()==WL_CONNECTED && client.connected()){
    lastPing=now;
    DynamicJsonDocument doc(128); doc["softVersion"]=softVersion; doc["statusCode"]=1;
    String output; serializeJson(doc,output);
    client.publish("device/1885962/ping",output.c_str());
    LOG_PRINTLN("PING enviat: "+output);
  }

  // DATA 30s
  if(now-lastData>30000){
    lastData=now;
    DataMessage msg={getCurrentDateTimeUTC(), random(5,100), random(105,605)/10.0, random(0,2)==1};
    if(WiFi.status()==WL_CONNECTED && client.connected()) enviarMissatge(msg);
    else { messageQueue.push_back(msg); LOG_PRINTLN("DATA desada a cua (sense connexió)"); }
  }

  // Enviar cua
  if(WiFi.status()==WL_CONNECTED && client.connected() && !messageQueue.empty()){
    for(auto &msg:messageQueue) enviarMissatge(msg);
    messageQueue.clear();
    LOG_PRINTLN("Tots els missatges pendents enviats!");
  }

  // UPDATE programat
  if(updateScheduled){
    String cur=getCurrentHourMinuteUTC();
    if(cur==scheduledUpdHour){
      LOG_PRINTLN("Executant UPDATE programat...");
      softVersion=scheduledSoftVersion;
      preferences.putString("softVersion",softVersion);
      preferences.remove("updHour"); preferences.remove("updateSoftVersion");
      delay(500); ESP.restart();
    }
  }
}
