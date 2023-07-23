#include <PubSubClient.h>
#include <WiFi.h>
#include "DHTesp.h"
#include <Servo.h>
//#include <NTPClient.h>  // To get the real time using a server
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "time.h"

LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 16, 2);

const int DHT_PIN = 15;
const int LDR_PIN = 34; //LDR connected to ADC 34
const int Servo_PIN = 4;  // servo connected to pwm pin 4
const int Buzzer_PIN = 12;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 19800; // 19800 is 3600 (seconds per hour) x 5.5
                                      // since sri Lankan time is +5.30
Servo servo;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;

char tempAr[6];
char humAr[6];
char ldrAr[6];
char Time[6];

bool MainSwitchState = true;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  LCD.init();
  LCD.backlight();
  setupWiFi();
  setupMQTT();
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22); // seting up the sensor
  servo.attach(Servo_PIN);  // Attaches the servo on PIN 4
  pinMode(Buzzer_PIN, OUTPUT);
  
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.println("Online");
  LCD.setCursor(0, 1);
  LCD.println("Updating time...");
  
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Print_Time();
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

  // Getting real-time and date & print in LCD
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Print_Time();
  mqttClient.publish("lw-time", Time);

  delay(100);
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
    LCD_wifi();    
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
      mqttClient.subscribe("lw-Servo"); // Need to give the topics of the subscribing fields here
      mqttClient.subscribe("lw-Buzzer");  // buzzer on off
      mqttClient.subscribe("lw-switch"); // for main switch
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
  //Serial.println("Temp:- " + String(tempAr));
  //Serial.println("Humidity:- " + String(humAr));
}

// updating the LDR value
void updateLDR() {
  float ldrval = (float)analogRead(LDR_PIN) / 4095.0;
  String(ldrval, 3).toCharArray(ldrAr, 6);
  //Serial.println("LDR Value: " + String(ldrAr));
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

  BuzzerControl(topic, payloadCharArr); // control buzzer
  ServoControl(topic, payloadCharArr ); // function to control servo motor
  MainSwitch(topic,payloadCharArr );
}


// Controling function for servo
void ServoControl(char* topic, char payloadCharArr[] ) {
  if (strcmp(topic, "lw-Servo") == 0) {
    Serial.println("Angle recieved");
    int servo_ang = atoi(payloadCharArr); // value in between 0 - 180
    servo.write(servo_ang);
  }
}

// controling buzzer
void BuzzerControl(char* topic, char payloadCharArr[]) {
  if (strcmp(topic, "lw-Buzzer") == 0) {
    if (MainSwitchState){       // if main switch is on
      if (payloadCharArr[0] =='1'){
        Serial.println("Alarm is ON");
        LCD.clear();
        LCD.setCursor(1, 6);
        LCD.println("Alarm is ON");
        buzzer_music(); // adjust fequency to change the tone
        }
      else{noTone(Buzzer_PIN);}
      }
  }
}

void MainSwitch(char* topic, char payloadCharArr[]){
  if (strcmp(topic, "lw-switch")==0){
    Serial.println("Main Switch Pressed");
    if (payloadCharArr[0] =='0'){
      digitalWrite(Buzzer_PIN, LOW);
      LCD.clear();
      LCD.setCursor(1, 5);
      LCD.println("Alarm is Off");
      MainSwitchState = false; // Alarm is off
    }
    else {
      MainSwitchState = true; // Alarm is ON
    }
  }
}

// LCD wifi connect

void LCD_wifi() {
  static int8_t counter = 0;
  const char* glyphs = "\xa1\xa5\xdb";
  LCD.setCursor(15, 1);
  LCD.print(glyphs[counter++]);
  if (counter == strlen(glyphs)) {
    counter = 0;
  }
}

// LCD time and Date
void Print_Time() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    LCD.setCursor(0, 1);
    LCD.println("Connection Err");
    return;
  }

  strftime(Time,sizeof(Time),"%H:%M:%S",&timeinfo);
  Serial.println(Time);
  LCD.clear();
  LCD.setCursor(8, 0);
  LCD.println(&timeinfo, "%H:%M:%S");

  LCD.setCursor(0, 1);
  LCD.println(&timeinfo, "%d/%m/%Y   %Z");
}
void buzzer_music(){
  // Play the musical tone for 500 milliseconds
  playTone(262, 1000); // Play frequency 262 Hz (Note: Middle C) for 500 milliseconds

  playTone(294, 1000); // Play frequency 294 Hz (Note: D) for 500 milliseconds

  playTone(330, 1000); // Play frequency 330 Hz (Note: E) for 500 milliseconds
}

// Function to play a musical tone on the buzzer
void playTone(int frequency, int duration) {
  tone(Buzzer_PIN, frequency, duration);
  delay(duration);
  noTone(Buzzer_PIN);
}
