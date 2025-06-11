#include <DHT.h>
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <LiquidCrystal.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include "time.h"

// ---- Configuración NTP ----
const char* NTP_SERVER = "pool.ntp.org";
const long UTC_OFFSET = -21600; // GMT-6
const long UTC_OFFSET_DST = 0;

// ---- Pines ----
#define DHTPIN 4
#define PIRPIN 34
#define TRIGPIN 5
#define ECHOPIN 18
#define BUZZERPIN 2
#define RING_DIN 15
const int servoPin = 13;

// LCD
LiquidCrystal lcd(16, 17, 19, 20, 21, 26);

// MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* topicTemp = "sensor/temperatura";
const char* topicHumi = "sensor/humedad";
const char* topicMovimiento = "sensor/movimiento";
const char* topicDistancia = "sensor/distancia";
const char* topicFecha = "sensor/fecha";
const char* topicServo = "sensor/servo";

WiFiClient espClient;
PubSubClient client(espClient);

// Sensores
DHT dht(DHTPIN, DHT22);
Adafruit_NeoPixel pixels(8, RING_DIN, NEO_GRB + NEO_KHZ800);
Servo miServo;

// Estado
bool controlManual = false;
unsigned long tiempoActivacionServo = 0;
String estadoServo = "cerrado";
int pirState = LOW;

void setup() {
  Serial.begin(115200);
  pinMode(PIRPIN, INPUT);
  pinMode(TRIGPIN, OUTPUT);
  pinMode(ECHOPIN, INPUT);
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(RING_DIN, OUTPUT);

  WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  dht.begin();
  lcd.begin(16, 2);
  Wire.begin();
  pixels.begin();
  pixels.show();
  miServo.attach(servoPin);
  miServo.write(0); // Inicialmente cerrado

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  while (!client.connected()) {
    Serial.print("Conectado a MQTT...");
    String clientID = "esp32_client_" + String(random(0xffff), HEX);
    if (client.connect(clientID.c_str())) {
      client.subscribe("home9/ebb2025/servo1");
      client.publish(topicServo, estadoServo.c_str());
    } else {
      delay(2000);
    }
  }
}

void loop() {
  client.loop();

  float temp = dht.readTemperature();
  float humi = dht.readHumidity();
  float distancia = medirDistancia();
  int val = digitalRead(PIRPIN);
  String fechaHora = obtenerFechaHora();

  // Mostrar en LCD
  lcd.setCursor(0, 0); lcd.print("                ");
  lcd.setCursor(0, 0); lcd.print(fechaHora);
  lcd.setCursor(0, 1);
  lcd.printf("T:%.1fC H:%d%% M:%d D:%dcm  ", temp, (int)humi, val == HIGH, (int)distancia);

  // --- Mostrar en formato JSON en Serial ---
  Serial.println("{");
  Serial.print("  \"fecha_hora\": \""); Serial.println(fechaHora);
  Serial.print("  \"temperatura_c\": "); Serial.println(temp, 2); 
  Serial.print("  \"humedad_pct\": "); Serial.println(humi, 2); 
  Serial.print("  \"movimiento\": "); Serial.println(val == HIGH ? "SI" : "No"); 
  Serial.print("  \"distancia_cm\": "); Serial.println(distancia, 2); 
  Serial.print("  \"estado_servo\": \""); Serial.print(estadoServo); Serial.println("\"");
  Serial.println("}");
  Serial.println();

  // NeoPixel
  if (val == HIGH) {
    activarNeoPixel(255, 0, 0);
    if (pirState == LOW) pirState = HIGH;
  } else {
    apagarNeoPixel();
    if (pirState == HIGH) pirState = LOW;
  }

  // Buzzer
  if (distancia < 100) tone(BUZZERPIN, 1000);
  else noTone(BUZZERPIN);

  // Servo automático (si no está en control manual)
  if (!controlManual) {
    if (distancia < 100) {
      miServo.write(90);
      if (estadoServo != "abierto") {
        estadoServo = "abierto";
        client.publish(topicServo, estadoServo.c_str());
      }
      tiempoActivacionServo = millis();
    } else if (millis() - tiempoActivacionServo >= 5000) {
      miServo.write(0);
      if (estadoServo != "cerrado") {
        estadoServo = "cerrado";
        client.publish(topicServo, estadoServo.c_str());
      }
    }
  }

  // Publicar datos por MQTT
  client.publish(topicTemp, String(temp).c_str());
  client.publish(topicHumi, String(humi).c_str());
  client.publish(topicMovimiento, String(val).c_str());
  client.publish(topicDistancia, String(distancia).c_str());
  client.publish(topicFecha, fechaHora.c_str());

  delay(5000);
}

float medirDistancia() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);
  return pulseIn(ECHOPIN, HIGH) / 58.0;
}

String obtenerFechaHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Sin hora";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

void activarNeoPixel(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

void apagarNeoPixel() {
  for (int i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }
  pixels.show();
}

void callback(char* topic, byte* message, unsigned int length) {
  String msg = "";
  for (int i = 0 ; i < length; i++) msg += (char)message[i];

  if (String(topic) == "home9/ebb2025/servo1") {
    controlManual = true;
    if (msg == "abrir") {
      miServo.write(90);
      estadoServo = "abierto";
      client.publish(topicServo, estadoServo.c_str());
    } else if (msg == "cerrar") {
      miServo.write(0);
      estadoServo = "cerrado";
      client.publish(topicServo, estadoServo.c_str());
    }
  }
}