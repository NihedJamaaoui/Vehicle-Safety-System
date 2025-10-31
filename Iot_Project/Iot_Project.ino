/**************************************************
  Projet IoT Véhicule Connecté - ESP8266 + DHT11 + Firebase
  Auteur : Nourhène Jamaoui
  Version : Finale - Compatible avec votre structure Firebase
**************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <DHT.h>

// === CONFIG WIFI ===
#define WIFI_SSID "Orange-1F3A"
#define WIFI_PASSWORD "RihemeJamaoui2006"

// === CONFIG FIREBASE ===
#define DATABASE_URL "https://controle-leds-78445-default-rtdb.europe-west1.firebasedatabase.app"
#define API_KEY "AIzaSyBInBhvp3WQUFHv20lG9SJoAmuJLvm38j4"

// === CONFIG DHT11 ===
#define DHTPIN 4  // D2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// === CONFIG LEDs ===
#define LED_ROUGE 14   // D5
#define LED_ORANGE 12  // D6
#define LED_BLEUE 13   // D7

// === HTTPS client ===
std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

// === Variables GPS et environnement ===
float latitude = 36.8065;
float longitude = 10.1815;
float altitude = 50.0;
bool zoneDangereuse = false;

// === Timers ===
unsigned long lastSend = 0;
unsigned long lastLedCheck = 0;
unsigned long lastHistorySave = 0;
const unsigned long SEND_INTERVAL = 5000;
const unsigned long LED_CHECK_INTERVAL = 3000;
const unsigned long HISTORY_INTERVAL = 10000;

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Configuration des LEDs
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_ORANGE, OUTPUT);
  pinMode(LED_BLEUE, OUTPUT);

  // LEDs éteintes au démarrage
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_ORANGE, LOW);
  digitalWrite(LED_BLEUE, LOW);

  // Connexion WiFi
  Serial.print("Connexion WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n WiFi connecté !");
  Serial.println("IP: " + WiFi.localIP().toString());
  
  client->setInsecure();
}

void loop() {
  unsigned long now = millis();

  // Envoi des données capteur
  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    sendSensorData();
  }

  // Vérification état LEDs
  if (now - lastLedCheck >= LED_CHECK_INTERVAL) {
    lastLedCheck = now;
    checkLedStates();
    controlAutoLEDs();
  }

  // Sauvegarde historique
  if (now - lastHistorySave >= HISTORY_INTERVAL) {
    lastHistorySave = now;
    saveToHistory();
  }
}

// === Envoi données capteurs vers /sensor ===
void sendSensorData() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("  Erreur lecture DHT11 !");
    return;
  }

  // Simulation mouvement GPS réaliste
  latitude += (random(-5, 6) * 0.0001);
  longitude += (random(-5, 6) * 0.0001);
  
  // Détection zone dangereuse (zone prédéfinie)
  zoneDangereuse = (latitude > 36.8080 && latitude < 36.8120 && 
                   longitude > 10.1790 && longitude < 10.1830);

  // Construction JSON selon votre structure
  String jsonData = "{";
  jsonData += "\"Altitude\":" + String(altitude, 1) + ",";
  jsonData += "\"Humidity\":" + String(hum, 2) + ",";
  jsonData += "\"Temperature\":" + String(temp, 2) + ",";
  jsonData += "\"humidite\":" + String(hum, 2) + ",";
  jsonData += "\"temp\":" + String(temp, 2) + ",";
  jsonData += "\"localisation\":{";
  jsonData += "\"latitude\":" + String(latitude, 6) + ",";
  jsonData += "\"longitude\":" + String(longitude, 6) + "},";
  jsonData += "\"timestamp\":\"" + getTimestamp() + "\",";
  jsonData += "\"zoneDangereuse\":" + String(zoneDangereuse ? "true" : "false") + "}";

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient https;
    String url = String(DATABASE_URL) + "/sensor.json?auth=" + API_KEY;
    https.begin(*client, url);
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.PATCH(jsonData);
    
    if (httpCode == HTTP_CODE_OK) {
      Serial.println(" Données sensor envoyées !");
    } else {
      Serial.println("L Erreur HTTP sensor: " + String(httpCode));
    }
    https.end();
  }
}

// === Sauvegarde dans l'historique ===
void saveToHistory() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (isnan(temp) || isnan(hum)) {
    Serial.println("  Erreur DHT11 pour historique");
    return;
  }

  String timestamp = getTimestamp();
  
  // Structure JSON pour l'historique
  String jsonData = "{";
  jsonData += "\"latitude\":" + String(latitude, 6) + ",";
  jsonData += "\"longitude\":" + String(longitude, 6) + ",";
  jsonData += "\"altitude\":" + String(altitude, 1) + ",";
  jsonData += "\"temperature\":" + String(temp, 2) + ",";
  jsonData += "\"humidity\":" + String(hum, 2) + ",";
  jsonData += "\"timestamp\":\"" + timestamp + "\",";
  jsonData += "\"zoneDangereuse\":" + String(zoneDangereuse ? "true" : "false") + "}";

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient https;
    String url = String(DATABASE_URL) + "/historique.json?auth=" + API_KEY;
    https.begin(*client, url);
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.POST(jsonData);
    
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("=Ý Historique sauvegardé !");
    } else {
      Serial.println("L Erreur historique: " + String(httpCode));
    }
    https.end();
  }
}

// === Contrôle automatique des LEDs selon les conditions ===
void controlAutoLEDs() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (isnan(temp) || isnan(hum)) {
    Serial.println("  Erreur lecture capteurs pour contrôle LEDs");
    return;
  }

  bool ledRouge = false;
  bool ledOrange = false;
  bool ledBleue = false;

  // Application des règles métier
  if (temp > 60.0 || hum > 80.0 || zoneDangereuse) {
    ledRouge = true;
    Serial.println("=¨ Alerte ROUGE - Conditions critiques");
  } else if (temp > 40.0 || hum > 70.0) {
    ledOrange = true;
    Serial.println("  Alerte ORANGE - Conditions modérées");
  } else {
    ledBleue = true;
    Serial.println(" État BLEU - Conditions normales");
  }

  // Mise à jour Firebase
  String jsonData = "{";
  jsonData += "\"rouge\":" + String(ledRouge ? "true" : "false") + ",";
  jsonData += "\"orange\":" + String(ledOrange ? "true" : "false") + ",";
  jsonData += "\"bleue\":" + String(ledBleue ? "true" : "false") + "}";

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient https;
    String url = String(DATABASE_URL) + "/leds.json?auth=" + API_KEY;
    https.begin(*client, url);
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.PATCH(jsonData);
    
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("=¡ États LEDs mis à jour");
    }
    https.end();
  }
}

// === Lecture manuelle des LEDs depuis Firebase ===
void checkLedStates() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient https;
  String url = String(DATABASE_URL) + "/leds.json?auth=" + API_KEY;
  https.begin(*client, url);
  int httpCode = https.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = https.getString();
    
    bool ledRouge = payload.indexOf("\"rouge\":true") > 0;
    bool ledOrange = payload.indexOf("\"orange\":true") > 0;
    bool ledBleue = payload.indexOf("\"bleue\":true") > 0;

    // Application sur les LEDs physiques
    digitalWrite(LED_ROUGE, ledRouge ? HIGH : LOW);
    digitalWrite(LED_ORANGE, ledOrange ? HIGH : LOW);
    digitalWrite(LED_BLEUE, ledBleue ? HIGH : LOW);

    Serial.println("=
 LEDs - R:" + String(ledRouge) + " O:" + String(ledOrange) + " B:" + String(ledBleue));
  } else {
    Serial.println("L Erreur lecture LEDs: " + String(httpCode));
  }
  https.end();
}

// === Génération timestamp ===
String getTimestamp() {
  unsigned long time = millis() / 1000;
  int hours = (time / 3600) % 24;
  int minutes = (time / 60) % 60;
  int seconds = time % 60;
  
  char timestamp[20];
  sprintf(timestamp, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(timestamp);
}
