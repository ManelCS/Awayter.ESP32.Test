#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>
#include <ArduinoJson.h>
#include <utility> 

struct DataMessage {
    String timestamp;
    float pressure;
    float temperature;
    bool alarm;
};

class Storage {
public:
    Storage();
    void begin();

    // Soft version
    void saveSoftVersion(const String &version);
    String loadSoftVersion();

    // Update info
    void saveUpdateInfo(const String &hour, const String &version);
    bool loadUpdateInfo(String &hour, String &version);
    void clearUpdateInfo();

    // WiFi credentials
    void saveWiFiCredentials(const String &ssid, const String &pass);
    std::pair<String, String> loadWiFiCredentials();
    void clearWiFiCredentials();

    // Missatges en cua
    void saveQueuedMessages(const std::vector<DataMessage> &queue);
    std::vector<DataMessage> loadQueuedMessages();

    void clearQueuedMessages();

private:
    Preferences preferences;
};


#endif
