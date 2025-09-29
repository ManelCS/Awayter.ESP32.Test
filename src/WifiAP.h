#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Arduino.h>


extern String ssid;
extern String password;
extern Preferences preferences;
extern WebServer server;


void startAPMode();
void handleRoot();
void handleSave();
void resetWiFiCredentials();

#endif
