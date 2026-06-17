#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* wifiPassword = WIFI_PASSWORD;

const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;

const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWORD;

const char* mqtt_topic = MQTT_TOPIC;

const int trigPin = 12; 
const int echoPin = 14;

const int ledVerde = 5;
const int ledVermelho = 4;

const float DISTANCIA_OCUPADA_CM = 8.0;

#define SOUND_VELOCITY 0.034
#define CM_TO_INCH 0.393701

long duration;
float distanceCm;
float distanceInch;

BearSSL::WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000;

void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT... ");

    String clientId = "ESP8266-SmartParking-";
    clientId += String(ESP.getChipId(), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("conectado!");

      client.publish(
        mqtt_topic,
        "{\"status\":\"ESP8266 conectado ao HiveMQ\"}"
      );
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

bool medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return false;
  }

  distanceCm = duration * SOUND_VELOCITY / 2;
  distanceInch = distanceCm * CM_TO_INCH;

  return true;
}

void atualizarLeds(String statusVaga) {
  if (statusVaga == "ocupada") {
    digitalWrite(ledVermelho, HIGH);
    digitalWrite(ledVerde, LOW);
  } 
  else if (statusVaga == "livre") {
    digitalWrite(ledVermelho, LOW);
    digitalWrite(ledVerde, HIGH);
  } 
  else {
    digitalWrite(ledVermelho, LOW);
    digitalWrite(ledVerde, LOW);
  }
}

void publicarStatus() {
  if (medirDistancia()) {
    String statusVaga;

    if (distanceCm <= DISTANCIA_OCUPADA_CM) {
      statusVaga = "ocupada";
    } else {
      statusVaga = "livre";
    }

    atualizarLeds(statusVaga);

    String payload = "{";
    payload += "\"vaga\":\"vaga01\",";
    payload += "\"status\":\"";
    payload += statusVaga;
    payload += "\",";
    payload += "\"distance_cm\":";
    payload += String(distanceCm, 2);
    payload += ",";
    payload += "\"distance_inch\":";
    payload += String(distanceInch, 2);
    payload += "}";

    Serial.print("Distancia: ");
    Serial.print(distanceCm);
    Serial.print(" cm | Status: ");
    Serial.println(statusVaga);

    client.publish(mqtt_topic, payload.c_str());
  } 
  else {
    atualizarLeds("erro");

    Serial.println("Falha ao medir distÃ¢ncia.");

    client.publish(
      mqtt_topic,
      "{\"vaga\":\"vaga01\",\"status\":\"erro\",\"erro\":\"falha_na_medicao\"}"
    );
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(ledVerde, OUTPUT);
  pinMode(ledVermelho, OUTPUT);

  digitalWrite(ledVerde, LOW);
  digitalWrite(ledVermelho, LOW);

  setup_wifi();

  espClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }

  client.loop();

  unsigned long now = millis();

  if (now - lastPublish >= publishInterval) {
    lastPublish = now;
    publicarStatus();
  }
}
