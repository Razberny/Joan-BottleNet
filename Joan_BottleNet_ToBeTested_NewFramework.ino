#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ======================
// Hardware Configuration
// ======================
// Bin Ultrasonic Sensor
const int BIN_TRIG_PIN = 3;
const int BIN_ECHO_PIN = 4;
const float BIN_FULL_THRESHOLD = 10.0; // cm or less means bin is full

// SIM900A
#define SIM_RX_PIN 7
#define SIM_TX_PIN 6
SoftwareSerial sim900(SIM_RX_PIN, SIM_TX_PIN);

// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Change to 0x3F if not working

// Servo
Servo gateServo;
const int pulsePin = 8;
const int SERVO_PIN = 11;
const int SERVO_OPEN_ANGLE = 180;
const int SERVO_CLOSED_ANGLE = 0;

// Sensors
const int IR_SENSOR_PIN = 2;
const int CAP_SENSOR_PIN = A0;
const int TRIG_PIN = 9;
const int ECHO_PIN = 10;

// Button
const int BUTTON_PIN = 12;

// =================
// System Parameters
// =================
const float SMALL_MAX = 15.0;
const float MEDIUM_MAX = 25.0;
const int CAP_WATER_THRESHOLD = 500;
const float SENSOR_HEIGHT = 40.0;  // distance from sensor to surface
bool systemActive = false;
bool binNotified = false;

// ======================
// Setup
// ======================
void setup() {
  Serial.begin(9600);
  sim900.begin(9600);
  lcd.begin();
  lcd.backlight();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(pulsePin, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BIN_TRIG_PIN, OUTPUT);
  pinMode(BIN_ECHO_PIN, INPUT);

  gateServo.attach(SERVO_PIN);
  gateServo.write(SERVO_CLOSED_ANGLE);

  showWelcomeScreen();
  initializeSIM900();
}

// ======================
// Main Loop
// ======================
void loop() {
  if (digitalRead(BUTTON_PIN) == LOW && !systemActive) {
    systemActive = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pls insert bottle");
    Serial.println("[System] Waiting for bottle...");
    delay(1000);
  }

  if (systemActive && digitalRead(IR_SENSOR_PIN) == LOW) {
    Serial.println("[Sensor] Bottle detected!");

    float distance = readDistance(TRIG_PIN, ECHO_PIN);
    int capValue = analogRead(CAP_SENSOR_PIN);

    String bottleSize = classifyBottle(distance);
    bool isDry = capValue < CAP_WATER_THRESHOLD;

    Serial.print("[Ultrasonic] Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
    Serial.print("[Cap Sensor] Value: ");
    Serial.println(capValue);
    Serial.print("[Size] ");
    Serial.println(bottleSize);
    Serial.print("[Water] ");
    Serial.println(isDry ? "Dry" : "Wet");

    if (isDry) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Accepted Bottle:");
      lcd.setCursor(0, 1);
      lcd.print(bottleSize);
      sendPulse(bottleSize);
      delay(3000);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Please remove");
      lcd.setCursor(0, 1);
      lcd.print("the water inside");
      Serial.println("[System] Bottle rejected due to water.");
      delay(3000);
    }

    showWelcomeScreen();
    systemActive = false;
  }

  // Bin Monitoring
  float binDistance = readDistance(BIN_TRIG_PIN, BIN_ECHO_PIN);
  if (binDistance <= BIN_FULL_THRESHOLD && !binNotified) {
    Serial.println("[Bin] Bin is full! Sending SMS...");
    sendSMS("+639473732949", "The Bin is Full.");
    binNotified = true;
  }

  delay(500); // Reduce CPU usage
}

// ======================
// Helper Functions
// ======================
void showWelcomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome to BottleNet");
  lcd.setCursor(0, 1);
  lcd.print("Do you want to");
  lcd.setCursor(0, 2);
  lcd.print("Insert Bottle?");
  lcd.setCursor(0, 3);
  lcd.print("Press the Button");
  Serial.println("[LCD] Welcome screen displayed.");
}

float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  float duration = pulseIn(echoPin, HIGH);
  return duration * 0.0343 / 2;
}

String classifyBottle(float distance) {
  float level = SENSOR_HEIGHT - distance;
  if (level <= SMALL_MAX) return "Small";
  else if (level <= MEDIUM_MAX) return "Medium";
  else return "Large";
}

void sendPulse(String size) {
  int pulses = (size == "Small") ? 1 : (size == "Medium") ? 2 : 3;

  for (int i = 0; i < pulses; i++) {
    digitalWrite(pulsePin, HIGH);
    delay(500);
    digitalWrite(pulsePin, LOW);
    delay(500);
  }
  Serial.print("[Pulse] Sent ");
  Serial.print(pulses);
  Serial.println(" pulse(s).");
}

void initializeSIM900() {
  Serial.println("Initializing SIM900A...");
  sendATCommand("AT", "OK");
  sendATCommand("AT+CMGF=1", "OK");
  sendATCommand("AT+CSQ", "");
  sendATCommand("AT+CCID", "");
  sendATCommand("AT+COPS?", "");
}

void sendSMS(String number, String message) {
  sim900.println("AT+CMGS=\"" + number + "\"");
  delay(2000);
  sim900.print(message);
  sim900.write(26); // Ctrl+Z
  delay(5000);
  Serial.println("[SMS] Sent: " + message);
  while (sim900.available()) {
    Serial.write(sim900.read());
  }
}

void sendATCommand(String command, String expectedResponse) {
  sim900.println(command);
  delay(1000);
  while (sim900.available()) {
    String response = sim900.readString();
    Serial.println("AT Response: " + response);
    if (expectedResponse != "" && response.indexOf(expectedResponse) == -1) {
      Serial.println("⚠️ Unexpected response.");
    }
  }
}
