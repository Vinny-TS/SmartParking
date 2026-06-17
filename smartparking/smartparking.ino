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

// Topico onde o ESP8266 publica os dados da vaga
const char* mqtt_topic = MQTT_TOPIC;

// Topico onde o ESP8266 recebe comandos vindos do MQTTX
const char* mqtt_command_topic = "inatel/smartparking/vaga01/comando";

// Topico para o ESP8266 responder qual comando executou
const char* mqtt_response_topic = "inatel/smartparking/vaga01/resposta";

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

// true  = sistema automatico ligado, sensor controla os LEDs
// false = sistema desligado, LEDs podem ser controlados manualmente por MQTT
bool sistemaAtivo = true;

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

// Funcao para atualizar LEDs automaticamente de acordo com o sensor
void atualizarLedsAutomatico(String statusVaga) {
  if (statusVaga == "ocupada") {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledVermelho, HIGH);
  } 
  else if (statusVaga == "livre") {
    digitalWrite(ledVerde, HIGH);
    digitalWrite(ledVermelho, LOW);
  } 
  else {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledVermelho, LOW);
  }
}

// Funcao para controlar LEDs e sistema por comando MQTT
void controlarLedsPorComando(String comando) {
  comando.trim();
  comando.toUpperCase();

  // Liga o sistema automatico novamente
  if (comando == "SISTEMA_LIGAR") {
    sistemaAtivo = true;

    Serial.println("Sistema automatico ligado.");
    client.publish(mqtt_response_topic, "SISTEMA_LIGADO");

    return;
  }

  // Desliga o sistema automatico e libera o modo manual
  if (comando == "SISTEMA_DESLIGAR") {
    sistemaAtivo = false;

    digitalWrite(ledVerde, LOW);
    digitalWrite(ledVermelho, LOW);

    Serial.println("Sistema automatico desligado. Modo manual liberado.");
    client.publish(mqtt_response_topic, "SISTEMA_DESLIGADO_MODO_MANUAL_ATIVO");

    return;
  }

  // Se o sistema estiver ligado, comandos manuais de LED sao ignorados
  if (sistemaAtivo) {
    Serial.println("Comando manual ignorado. Sistema automatico esta ligado.");

    client.publish(
      mqtt_response_topic,
      "ALERTA: desligue o sistema antes de controlar os LEDs manualmente"
    );

    return;
  }

  // A partir daqui, so executa se o sistema estiver desligado

  if (comando == "VERDE_ON" || comando == "VERDE") {
    digitalWrite(ledVerde, HIGH);

    Serial.println("Modo manual: LED verde ligado");
    client.publish(mqtt_response_topic, "MODO_MANUAL_LED_VERDE_LIGADO");
  }

  else if (comando == "VERDE_OFF") {
    digitalWrite(ledVerde, LOW);

    Serial.println("Modo manual: LED verde desligado");
    client.publish(mqtt_response_topic, "MODO_MANUAL_LED_VERDE_DESLIGADO");
  }

  else if (comando == "VERMELHO_ON" || comando == "VERMELHO") {
    digitalWrite(ledVermelho, HIGH);

    Serial.println("Modo manual: LED vermelho ligado");
    client.publish(mqtt_response_topic, "MODO_MANUAL_LED_VERMELHO_LIGADO");
  }

  else if (comando == "VERMELHO_OFF") {
    digitalWrite(ledVermelho, LOW);

    Serial.println("Modo manual: LED vermelho desligado");
    client.publish(mqtt_response_topic, "MODO_MANUAL_LED_VERMELHO_DESLIGADO");
  }

  else if (comando == "AMBOS_ON") {
    digitalWrite(ledVerde, HIGH);
    digitalWrite(ledVermelho, HIGH);

    Serial.println("Modo manual: ambos os LEDs ligados");
    client.publish(mqtt_response_topic, "MODO_MANUAL_AMBOS_LEDS_LIGADOS");
  }

  else if (comando == "DESLIGAR" || comando == "LEDS_OFF") {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledVermelho, LOW);

    Serial.println("Modo manual: LEDs desligados");
    client.publish(mqtt_response_topic, "MODO_MANUAL_LEDS_DESLIGADOS");
  }

  else {
    Serial.print("Comando desconhecido: ");
    Serial.println(comando);

    client.publish(mqtt_response_topic, "COMANDO_DESCONHECIDO");
  }
}

// Esta funcao e chamada automaticamente quando chega uma mensagem MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String mensagem = "";

  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  Serial.print("Mensagem recebida no topico: ");
  Serial.println(topic);

  Serial.print("Payload recebido: ");
  Serial.println(mensagem);

  if (String(topic) == mqtt_command_topic) {
    controlarLedsPorComando(mensagem);
  }
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

      // Aqui o ESP8266 se inscreve no topico de comando
      client.subscribe(mqtt_command_topic);

      Serial.print("Inscrito no topico de comando: ");
      Serial.println(mqtt_command_topic);

      client.publish(mqtt_response_topic, "ESP8266_ONLINE");
    } 
    else {
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

void publicarStatus() {
  if (medirDistancia()) {
    String statusVaga;

    if (distanceCm <= DISTANCIA_OCUPADA_CM) {
      statusVaga = "ocupada";
    } 
    else {
      statusVaga = "livre";
    }

    // Se o sistema automatico estiver ligado,
    // o sensor controla os LEDs normalmente
    if (sistemaAtivo) {
      atualizarLedsAutomatico(statusVaga);
    }

    String payload = "{";
    payload += "\"vaga\":\"vaga01\",";
    payload += "\"sistema\":\"";
    payload += sistemaAtivo ? "ligado" : "desligado";
    payload += "\",";
    payload += "\"modo\":\"";
    payload += sistemaAtivo ? "automatico" : "manual";
    payload += "\",";
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
    Serial.print(statusVaga);
    Serial.print(" | Sistema: ");
    Serial.println(sistemaAtivo ? "ligado" : "desligado");

    client.publish(mqtt_topic, payload.c_str());
  } 
  else {
    if (sistemaAtivo) {
      atualizarLedsAutomatico("erro");
    }

    Serial.println("Falha ao medir distancia.");

    String payload = "{";
    payload += "\"vaga\":\"vaga01\",";
    payload += "\"sistema\":\"";
    payload += sistemaAtivo ? "ligado" : "desligado";
    payload += "\",";
    payload += "\"status\":\"erro\",";
    payload += "\"erro\":\"falha_na_medicao\"";
    payload += "}";

    client.publish(mqtt_topic, payload.c_str());
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

  // Essa linha e essencial para o ESP8266 receber mensagens MQTT
  client.setCallback(callback);
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
