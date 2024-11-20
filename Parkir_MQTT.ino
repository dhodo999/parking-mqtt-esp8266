#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Servo.h>

#define WIFI_SSID "" // Your WiFi SSID
#define WIFI_PASSWORD "" // Your WiFi password

// MQTT Broker details
#define MQTT_HOST "broker.mqtt-dashboard.com" // MQTT broker URL
#define MQTT_PORT 1883 // MQTT broker port
#define MQTT_PUB_TOPIC "parkir/kuota" // MQTT topic to publish parking quota

// Ultrasonic & sensor pins
const int trigPinA = D1; 
const int echoPinA = D2;
const int trigPinB = D5;
const int echoPinB = D6;
const int buzzerPin = D3;

int parkirKuota = 300; // Initial parking quota

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Servo myServo;

const long interval = 5000; // Interval to read ultrasonic sensor (can be adjusted)
unsigned long previousMillis = 0; // Variable to store last time sensor was read

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  mqttClient.connect();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi. Reconnecting...");
  mqttReconnectTimer.detach();
  wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT. Reconnecting...");
  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, [](){ mqttClient.connect(); });
  }
}

long readUltrasonicDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

void updateKuota(int perubahan) {
  if (parkirKuota + perubahan >= 0) {
    parkirKuota += perubahan;
    Serial.printf("Kuota parkir sekarang: %i\n", parkirKuota);

    // Publish parking quota to MQTT
    mqttClient.publish(MQTT_PUB_TOPIC, 0, true, String(parkirKuota).c_str());
  } else {
    Serial.println("Kuota sudah 0, tidak dapat mengurangi lebih lanjut.");
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(trigPinA, OUTPUT);
  pinMode(echoPinA, INPUT);
  pinMode(trigPinB, OUTPUT);
  pinMode(echoPinB, INPUT);
  pinMode(buzzerPin, OUTPUT);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();

  myServo.attach(D4);
  Serial.println("Servo test started");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    long distanceA = readUltrasonicDistance(trigPinA, echoPinA);
    long distanceB = readUltrasonicDistance(trigPinB, echoPinB);

    // Gerbang A logic
    if (distanceA < 10 && parkirKuota > 0) {
      updateKuota(-1);
      myServo.write(180);   // Open gate A
      delay(3000);          // Keep gate open for 3 seconds
      myServo.write(0);     // Close gate A
    }

    // Gerbang B logic
    if (distanceB < 10) {
      updateKuota(1);
      digitalWrite(buzzerPin, HIGH);  // Activate buzzer
      delay(1000);                    // Buzz briefly
      digitalWrite(buzzerPin, LOW);   // Deactivate buzzer
    }
  }

  delay(500);  // Stabilization delay
}
