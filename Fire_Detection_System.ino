#define BLYNK_TEMPLATE_ID "Paste your owm"
#define BLYNK_TEMPLATE_NAME "Paste your own"
#define BLYNK_AUTH_TOKEN "Paste your own"

#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>  // Use ESP32-compatible Servo library

// WiFi and ThingSpeak
char ssid[] = ".............";       
char pass[] = "............."; 
String apiKey = "Paste your own"; 
const char* server = "http://api.thingspeak.com/update";

// Pin Definitions
#define DHTPIN 2
#define DHTTYPE DHT11
#define MQ135_PIN 34
#define MQ9_PIN   35
#define FLAME_PIN 25
#define BUZZER    26
#define LED_PIN   27
#define RELAY_PIN 14
#define SERVO_SCAN_PIN 32     // Servo 1 (sensor mount)
#define SERVO_NOZZLE_PIN 33   // Servo 2 (nozzle mount)
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1  

// OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT sensor object
DHT dht(DHTPIN, DHTTYPE);

// Servo objects
Servo servoScan;
Servo servoNozzle;

// Variables
float temp, hum;
int flameValue;
float co2_ppm, nh3_ppm, co_ppm, ch4_ppm;

// Conversion Functions (approximate)
float mq135_to_co2(int adc) { return (adc / 4095.0) * 2000; }
float mq135_to_nh3(int adc) { return (adc / 4095.0) * 300; }
float mq9_to_co(int adc)   { return (adc / 4095.0) * 1000; }
float mq9_to_ch4(int adc)  { return (adc / 4095.0) * 5000; }

// Function to display data
void displayData() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Temp:");
  display.print(temp);
  display.println(" C");
  display.print("Hum:");
  display.print(hum);
  display.println(" %");
  display.print("CO2:");
  display.print(co2_ppm);
  display.println(" ppm");
  display.print("NH3:");
  display.print(nh3_ppm);
  display.println(" ppm");
  display.print("CO:");
  display.print(co_ppm);
  display.println(" ppm");
  display.print("CH4:");
  display.print(ch4_ppm);
  display.println(" ppm");
  display.print("Flame:");
  display.println(flameValue == 0 ? "DETECTED" : "No");
  display.display();
}

// Setup function
void setup() {
  Serial.begin(115200);
  
  dht.begin();
  
  pinMode(FLAME_PIN, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    while(true);
  }
  display.clearDisplay();
  display.display();

  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("✅ Blynk Connected!");

  // Allocate timer for ESP32 servo PWM  
  ESP32PWM::allocateTimer(0);

  // Attach servos
  servoScan.attach(SERVO_SCAN_PIN);
  servoNozzle.attach(SERVO_NOZZLE_PIN);

// Initialize servo positions
servoScan.write(0);
servoNozzle.write(0);
}

void loop() {
  Blynk.run();

  // Scan flame with sensor-mounted servo
  bool flameDetected = false;
  int detectedPosition = -1;
  for (int angle = 0; angle <= 180; angle += 10) {
    servoScan.write(angle);
    delay(300);
    int val = digitalRead(FLAME_PIN);
    if(val == 0) { // Flame detected (active LOW)
      flameDetected = true;
      detectedPosition = angle;
      Serial.print("Flame detected at angle: ");
      Serial.println(angle);
      break;
    }
  }
  if(!flameDetected) detectedPosition = 0;  // Default center

  // Read sensors
  hum = dht.readHumidity();
  temp = dht.readTemperature();
  int mq135_adc = analogRead(MQ135_PIN);
  int mq9_adc = analogRead(MQ9_PIN);
  flameValue = digitalRead(FLAME_PIN);

  co2_ppm = mq135_to_co2(mq135_adc);
  nh3_ppm = mq135_to_nh3(mq135_adc);
  co_ppm = mq9_to_co(mq9_adc);
  ch4_ppm = mq9_to_ch4(mq9_adc);

  // Fire alert logic based on thresholds and flame detection
  bool fireAlert = false;
  if(flameDetected) fireAlert = true;
  if(co2_ppm > 2000) fireAlert = true;
  if(nh3_ppm > 300) fireAlert = true;
  if(co_ppm > 2000) fireAlert = true;
  if(ch4_ppm > 2000) fireAlert = true;
  if(temp > 45) fireAlert = true;

  if(fireAlert) {
    digitalWrite(BUZZER, HIGH);
    digitalWrite(LED_PIN, HIGH);
    servoNozzle.write(detectedPosition);     // Aim sprinkler nozzle
    digitalWrite(RELAY_PIN, HIGH);            // Turn water pump ON
    Blynk.logEvent("fire_alert", "🔥 Fire detected and sprinkler activated!");
  } else {
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED_PIN, LOW);
    servoNozzle.write(90);                    // Default nozzle position
    digitalWrite(RELAY_PIN, LOW);             // Turn water pump OFF
  }

  // Update OLED display with sensor values and flame status
  displayData();

  // Send sensor data to Blynk virtual pins
  Blynk.virtualWrite(V0, temp);
  Blynk.virtualWrite(V1, hum);
  Blynk.virtualWrite(V2, co2_ppm);
  Blynk.virtualWrite(V3, nh3_ppm);
  Blynk.virtualWrite(V4, co_ppm);
  Blynk.virtualWrite(V5, ch4_ppm);
  Blynk.virtualWrite(V6, flameValue == 0 ? 1 : 0);
  Blynk.virtualWrite(V7, digitalRead(BUZZER));

  // Send data to ThingSpeak server
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(server) + "?api_key=" + apiKey +
                 "&field1=" + String(temp) +
                 "&field2=" + String(hum) +
                 "&field3=" + String(co2_ppm) +
                 "&field4=" + String(nh3_ppm) +
                 "&field5=" + String(co_ppm) +
                 "&field6=" + String(ch4_ppm) +
                 "&field7=" + String(flameValue) +
                 "&field8=" + String(digitalRead(BUZZER));
    http.begin(url.c_str());
    int httpCode = http.GET();
    if(httpCode > 0) {
      Serial.printf("ThingSpeak Response Code: %d\n", httpCode);
    } else {
      Serial.printf("ThingSpeak Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }

  delay(5000);  // Delay for 5 seconds before next reading/update
}

