#include "../include/WifiAP.h"
#include "../include/log.h"

// HTML
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

// Handlers web
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    ssid = server.arg("ssid");
    password = server.arg("password");

    storage.saveWiFiCredentials(ssid, password);

    server.send(200, "text/html", "<h3>Credencials guardades! Reiniciant...</h3>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "Falten camps!");
  }
}

// Arrancar AP mode
void startAPMode() {
  LOG_PRINTLN("No hi ha WiFi configurat: Obrint AP mode");
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

// Resetejar les credencials del wifi
void resetWiFiCredentials() {
    storage.clearWiFiCredentials();
    ESP.restart();
}


