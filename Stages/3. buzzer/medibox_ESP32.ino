#include <PubSubClient.h>
#include <WiFi.h>
#include "DHTesp.h"
#include <Servo.h>
#include <NTPClient.h>  // To get the real time using a server
#include <WiFiUdp.h>  // For real time - User Datagram Protocol

const int DHT_PIN = 15;
const int LDR_PIN = 34; //LDR connected to ADC 34
const int Servo_PIN = 4;  // servo connected to pwm pin 4
const int Buzzer_PIN = 12;

Servo servo;
//for real time
WiFiUDP udp; // for udp
NTPClient timeClient(udp, "pool.ntp.org", 19800); // 19800 is 3600 (seconds per hour) x 5.5
// since sri Lankan time is +5.30

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;

char tempAr[6];
char humAr[6];
char ldrAr[6];
char Time[6];

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  setupWiFi();
  setupMQTT();
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22); // seting up the sensor
  servo.attach(Servo_PIN);  // Attaches the servo on PIN 4
  pinMode(Buzzer_PIN, OUTPUT);
}

void loop() {

  if (!mqttClient.connected()) {
    ConnectToBroker();
  }
  mqttClient.loop(); // All the messages checkings and publishes are done inside this

  // MQTT publish temp and hummidity
  updateTempAndHum();
  mqttClient.publish("lw-temp", tempAr);
  mqttClient.publish("lw-hum", humAr);

  // MQTT to publish LDR value
  updateLDR();
  mqttClient.publish("lw-ldr", ldrAr);

  // Getting real-time and date
  GetTimeAndDate();
  mqttClient.publish("lw-time", Time);


  delay(1000);
}

// Seting up the WiFi
void setupWiFi() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.print(1 + i);
    Serial.println(". SSID:- ");
    Serial.print(WiFi.SSID(i));
    Serial.println(" RSSI:- ");
    Serial.print(WiFi.RSSI(i));
  }

  WiFi.begin("Wokwi-GUEST", ""); //Put your wifi credentials or the best way is to use an Access Point and WiFiManager

  while (WiFi.status() !=  WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// setting up the MQTT
void setupMQTT() {
  mqttClient.setServer("test.mosquitto.org", 1883); //  https://test.mosquitto.org/
  mqttClient.setCallback(GetCallBack);  // When ever a msg is recieved, this function inside the brackets,
  // "GetCallBack" will be call.
  // We need to write that function according to our expectations
}

// function to connect to the broker
void ConnectToBroker() {
  while (!mqttClient.connected()) {
    Serial.println("Waiting for the MQTT connection...");
    if (mqttClient.connect("ESP-32-XXX...")) // If we are checking the authentication (1884 instead of 1883)
      // we have to put an id. If we are not using authentication, we just have to put a random string.
    {
      Serial.println("connected");
      mqttClient.subscribe("lw-servo"); // Need to give the topics of the subscribing fields here
      mqttClient.subscribe("lw-buzzer");  // buzzer on off
    } else {
      Serial.println("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

// updating Temperature and humidity
void updateTempAndHum() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);
  String(data.humidity, 2).toCharArray(humAr, 6);
  Serial.println("Temp:- " + String(tempAr));
  Serial.println("Humidity:- " + String(humAr));
}

// updating the LDR value
void updateLDR() {
  float ldrval = (float)analogRead(LDR_PIN) / 4095.0;
  String(ldrval, 3).toCharArray(ldrAr, 6);
  Serial.println("LDR Value: " + String(ldrAr));
}

// Get Time and Date
void GetTimeAndDate() {
  timeClient.update();
  timeClient.getFormattedTime().toCharArray(Time, 10);
  Serial.println(Time);
}

// writing a function to proccess the recieved subscribed data
void GetCallBack(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message recieved [");
  Serial.print(topic);
  Serial.print("]");

  char payloadCharArr[length];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharArr[i] = (char)payload[i];
  }
  Serial.print("\n");

  ServoControl(topic, payloadCharArr ); // function to control servo motor
  BuzzerControl(topic, payloadCharArr); // control buzzer
}

// Controling function for servo
void ServoControl(char* topic, char payloadCharArr[] ) {
  if (strcmp(topic, "lw-servo") == 0) {
    int servo_ang = atoi(payloadCharArr); // value in between 0 - 180
    servo.write(servo_ang);
  }
}

// controling buzzer
void BuzzerControl(char* topic, char payloadCharArr[]) {
  if (strcmp(topic, "lw-buzzer") == 0) {
    if (payloadCharArr[0] =='1'){
      Serial.println("Alarm is ON");
      tone(Buzzer_PIN, 1000); // adjust fequency to change the tone
      delay(5000);
      }
    else{noTone(Buzzer_PIN);}
  }
}
