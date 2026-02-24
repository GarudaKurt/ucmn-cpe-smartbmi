#include "configFirebase.h"

#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// Firebase helpers
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define WIFI_SSID "cpesmartbmi"
#define WIFI_PASSWORD "smartbmi"

#define API_KEY "AIzaSyCHeqVIPxksNZsPYK_0a3hQ7lI9Xk7POFI"
#define DATABASE_URL "https://ucmn-cpe-smartbmi-default-rtdb.firebaseio.com"

#define USER_EMAIL "admin@gmail.com"
#define USER_PASSWORD "admin123"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool isWiFiError = false;
static int retries = 0;
unsigned long sendDataPrevMillis = 0;

CONFIGFIREBASE::CONFIGFIREBASE() {}
CONFIGFIREBASE::~CONFIGFIREBASE() {}

void CONFIGFIREBASE::initFirebase() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries <= 60) {
    retries++;
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() != WL_CONNECTED) {
    isWiFiError = true;
    Serial.println("\nWiFi connection failed!");
    return;
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  config.token_status_callback = tokenStatusCallback;

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  Firebase.begin(&config, &auth);
}

void CONFIGFIREBASE::sendFirebaseData(float hr, float spo2, float temp) {
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    if (hr < 0 ||  spo2 < 0 || temp < 0) return;

    Serial.printf("Set Heart Rate... %s\n",
      Firebase.setFloat(fbdo, "/monitoring/hr", hr) ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Set SpO2... %s\n",
      Firebase.setFloat(fbdo, "/monitoring/spo2", spo2) ? "ok" : fbdo.errorReason().c_str());

    Serial.printf("Set Temp... %s\n",
      Firebase.setFloat(fbdo, "/monitoring/temp", temp) ? "ok" : fbdo.errorReason().c_str());
  }
}

bool CONFIGFIREBASE::WiFiError() {
  return isWiFiError;
}