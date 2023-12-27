#include <DHT.h>
#include <MQ135.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <SFE_BMP180.h>

const char* ssid = "***";           // Ağ adınızı girin
const char* password = "***";  //Ağ şifrenizi girin
const char* mqtt_server = "****";
const char* mqtt_server_user = "***";
const char* mqtt_server_password = "****";

char temperature[100];
char humidity[100];
char heatIndex[100];
char aqi[100];
char _pressure[100];
char seapressure[100];
char btemperature[100];


#define PIN_MQ135 A0   // MQ135 Analog Input Pin
#define DHTPIN 14      // DHT Digital Input Pin
#define DHTTYPE DHT11  // DHT11 or DHT22, depends on your sensor
#define LAMP_PIN 16
#define ALTITUDE 20.0

bool isLambOn;
unsigned long lastMsg = 0;

MQ135 mq135_sensor(PIN_MQ135);
DHT dht(DHTPIN, DHTTYPE);
SFE_BMP180 pressure;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_server_user, mqtt_server_password)) {
      Serial.println("connected");
      client.subscribe("lamb");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(LAMP_PIN, OUTPUT);
  digitalWrite(LAMP_PIN, HIGH);
  dht.begin();
  pressure.begin();
}

void loop() {
  double T, P, p0, a;
  char status;
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    float _temperature = dht.readTemperature();
    float _humidity = dht.readHumidity();
    float _correctedPPM = mq135_sensor.getCorrectedPPM(_temperature, _humidity);
    float _heatIndex = dht.computeHeatIndex(_temperature, _humidity, false);
    status = pressure.startTemperature();
    delay(status);
    status = pressure.getTemperature(T);
    status = pressure.startPressure(3);
    delay(status);
    status = pressure.getPressure(P, T);
    p0 = pressure.sealevel(P, ALTITUDE);


    dtostrf(_temperature, 2, 6, temperature);
    dtostrf(_humidity, 2, 6, humidity);
    dtostrf(_heatIndex, 2, 6, heatIndex);
    dtostrf(_correctedPPM, 2, 6, aqi);
    dtostrf(T, 2, 6, btemperature);
    dtostrf(p0, 2, 6, seapressure);
    dtostrf(P, 2, 6, _pressure);

    client.publish("temperature", temperature);
    client.publish("humidity", humidity);
    client.publish("heatIndex", heatIndex);
    client.publish("lamb_status", GetLambStatus());
    client.publish("aqi", GetAQI(_correctedPPM));
    client.publish("btemperature", btemperature);
    client.publish("seapressure", seapressure);
    client.publish("pressure", _pressure);
    
  }
  delay(2500);
}

char* GetAQI(float ppm) {
  if (ppm <= -1) { return "unknown-value"; }
  if (ppm >= 0 && ppm <= 100) { return "excellent-value"; }
  if (ppm >= 101 && ppm <= 200) { return "good-value"; }
  if (ppm >= 201 && ppm <= 300) { return "fair-value"; }
  if (ppm >= 301 && ppm <= 400) { return "inferior-value"; }
  if (ppm >= 400) { return "poor-value"; }
  return "unknown-value";
}
char* GetLambStatus() {

  if (isLambOn == true) {
    return "true";
  }
  return "false";
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String _payload = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    _payload += String((char)payload[i]);
  }
  Serial.println();

  if (String(topic) == "lamb") {
    Serial.print("isLamb");
    if (_payload == "true") {
      Serial.print("isHigh");
      digitalWrite(LAMP_PIN, LOW);
      isLambOn = true;
    } else {
      digitalWrite(LAMP_PIN, HIGH);
      Serial.print("isLow");
      isLambOn = false;
    }
  }
}