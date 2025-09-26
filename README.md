# Prova tècnica Awayter - ESP32

## 📂 Instruccions GitHub

1. Fes un **fork** d’aquest repositori al teu compte personal de GitHub.
2. Treballa sempre al teu fork, fent commits petits i amb missatges clars.
3. Quan acabis la prova, fes una PR (Pull Request) al nostre repositori, per així poder veure els canvis realitzats.


## ⚙️ Configuració de l’entorn

Abans de començar, cal preparar l’entorn de desenvolupament per al **WisCore RAK11200**.

### VSCode + PlatformIO
1. Instal·la [Visual Studio Code](https://code.visualstudio.com/).
2. Instal·la l’extensió **PlatformIO IDE**.
3. Segueix la documentació de [WisCore RAK11200](https://docs.rakwireless.com/product-categories/wisblock/rak11200/quickstart/)


## 🎯 Objectiu
L’objectiu de la prova és desenvolupar un petit software que, a través de la connexió **WiFi**, reculli dades d’un equip (en aquest cas dades simulades)  
i les enviï a través de **MQTT** perquè una plataforma web pugui guardar i mostrar les dades recollides de cada equip.  

A més, aquesta plataforma també ha de poder enviar instruccions als equips mitjançant MQTT, com per exemple:
- Executar una actualització.
- Forçar un reinici.

L’enviament dels missatges que simulin aquesta plataforma es pot fer directament des del **broker MQTT gratuït** que s’utilitzi.


---

## 📋 Requisits

### 1. Connexió
- Connectar l’ESP32 a un broker MQTT gratuït via WiFi (exemple: `test.mosquitto.org`, `testclient-cloud.mqtt.cool`).

### 2. Missatges
- Els missatges enviats i rebuts han de ser de tipus JSON.

### 3. Missatge de PING
- Publicar cada **5 minuts** al topic:
  ```
  device/1885962/ping
  ```
- Camps:
  - `softVersion` (ex: `"1.2.3"`)
  - `statusCode` (enter)

**Exemple JSON:**
```json
{
  "softVersion": "1.2.3",
  "statusCode": 1
}
```

### 4. Missatge de dades simulades
- Publicar cada **30 segons** al topic:
  ```
  device/1885962/data
  ```
- Camps:
  - `currentDateTime` (format: `"dd/mm/yyyy hh:MM"`, hora UTC)
  - `pressure` (valor aleatori 5–100)
  - `temperature` (valor aleatori 10.5–60.5)
  - `alarm` (booleà aleatori)

**Exemple JSON:**
```json
{
  "currentDateTime": "25/09/2025 10:42",
  "pressure": 27,
  "temperature": 32.5,
  "alarm": false
}
```

### 5. Reboot remot
- Subscriure’s al topic:
  ```
  device/1885962/reboot
  ```
- Quan arribi un missatge, reiniciar l’ESP32.

### 6. Update programat
- Subscriure’s al topic:
  ```
  device/1885962/update
  ```
- Camps rebuts:
  - `hour` (format `"hh:MM"`, ex: `"10:56"`)
  - `softVersion` (ex: `"1.2.3"`)

**Comportament esperat:**
1. Guardar l’hora programada.
2. Quan arribi l’hora, simular una actualització:
   - Actualitzar el número de versió guardat.
   - Reiniciar l’ESP32.
3. En el següent missatge de **PING**, enviar la nova versió.

**Exemple JSON rebut:**
```json
{
  "hour": "15:30",
  "softVersion": "1.2.4"
}
```

### 7. Persistencia
Implementar una classe `Storage` per guardar i recuperar valors en memòria.  

### Ús esperat:
- Guardar l'hora i la softVersion rebudes.
- Llegir la softVersion al reiniciar la placa.
- Guardar les dades dels missatges que no s'han pogut enviar si no hi ha connexió i recuperar-los.

### 8. Logs
L'aplicació ha d'imprimir per Serial els esdeveniments més importants per poder depurar el codi més fàcilment. Però també s'ha de
preparar un mecanisme que permeti desactivar els logs per quan es compila una versió per producció.

### Extra
- Fer que el mòdul creï un Acces Point, per tal de que ens puguem connectar amb un mòbil per poder introduir el SSID i Password, a través d'un formulari.
- Realitzar una conexió MQTT autenticada amb certificats. (Alguns del Brokers gratis ho permeten, ells mateixos ja generen els certificats).
- Si Es perd conexió amb el Broker MQTT o es desconnecta de la xarxa, guardar les dades que s'haurien d'enviar cada 30 segons. Per quan es recoperi la conexió 
  enviarles. (S'han d'enviar amb la data y hora corresponent a la seva generació.)
- Fer que software treballi amb threads diferents. Per tal que en un thread gestioni els missatges rebuts per MQTT i l'altre la resta del programa.

---

## ✅ Criteris d’avaluació
- Correcta connexió a WiFi i MQTT.
- Sistema de reconnexió si es perd la conexió WIFI.
- Serialització/deserialització de missatges en JSON.
- Organització del codi (classes, modularitat).
- Codi no bloquejant.
- Ús de Git (Commits petits i clars)
- Codi comentat i ben estructurat.

## Altres
Si no s'ha pogut complir algun dels objectius, explicar quin ha sigut el motiu i que es faria si es disposés de més temps per poder-ho resoldre. 
Quines millores es podrien aplicar si es disposés de més temps.

 

