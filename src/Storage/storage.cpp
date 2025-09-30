#include "Storage.h"

Storage::Storage() {}
void Storage::begin() {
    preferences.begin("storage", false);
}

// ---- Soft version ----
void Storage::saveSoftVersion(const String &version) {
    preferences.putString("softVersion", version);
}

String Storage::loadSoftVersion() {
    return preferences.getString("softVersion", "1.0.0");
}

// ---- Update info ----
void Storage::saveUpdateInfo(const String &hour, const String &version) {
    preferences.putString("updHour", hour);
    preferences.putString("updSoftVersion", version);
}

bool Storage::loadUpdateInfo(String &hour, String &version) {
    hour = preferences.getString("updHour", "");
    version = preferences.getString("updSoftVersion", "");
    return (hour != "" && version != "");
}

void Storage::clearUpdateInfo() {
    preferences.remove("updHour");
    preferences.remove("updSoftVersion");
}

// ---- WiFi credentials ----
void Storage::saveWiFiCredentials(const String &ssid, const String &pass) {
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", pass);
}

std::pair<String, String> Storage::loadWiFiCredentials() {
    String ssid = preferences.getString("wifi_ssid", "");
    String pass = preferences.getString("wifi_pass", "");
    return {ssid, pass};
}

void Storage::clearWiFiCredentials() {
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");
}

// ---- Missatges en cua ----
void Storage::saveQueuedMessages(const std::vector<DataMessage> &queue) {
    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();

    for (const auto &msg : queue) {
        JsonObject obj = arr.createNestedObject();
        obj["timestamp"] = msg.timestamp;
        obj["pressure"] = msg.pressure;
        obj["temperature"] = msg.temperature;
        obj["alarm"] = msg.alarm;
    }

    String json;
    serializeJson(doc, json);
    preferences.putString("messageQueue", json);
}

void Storage::clearQueuedMessages() {
    preferences.remove("messageQueue");
}


std::vector<DataMessage> Storage::loadQueuedMessages() {
    std::vector<DataMessage> queue;
    String json = preferences.getString("messageQueue", "");

    if (json == "") return queue;

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, json) == DeserializationError::Ok) {
        for (JsonObject obj : doc.as<JsonArray>()) {
            DataMessage msg;
            msg.timestamp = obj["timestamp"].as<String>();
            msg.pressure = obj["pressure"].as<float>();
            msg.temperature = obj["temperature"].as<float>();
            msg.alarm = obj["alarm"].as<bool>();
            queue.push_back(msg);
        }
    }

    return queue;
}
