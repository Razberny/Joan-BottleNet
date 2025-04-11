#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ======================
// Hardware Configuration
// ======================
// Bin Ultrasonic Sensor
const int BIN_TRIG_PIN = 3;
const int BIN_ECHO_PIN = 4;
const float BIN_FULL_THRESHOLD = 10.0; // cm or less means bin is full

// SIM900A
#include <SoftwareSerial.h>
SoftwareSerial sim900(7, 6); // RX, TX

// LCD (Try both addresses if needed)
LiquidCrystal_I2C lcd(0x27, 20, 4);  // 0x27 or 0x3F

// Servo
Servo gateServo;
const int pulsePin = 8;
const int SERVO_PIN = 11;
const int SERVO_OPEN_ANGLE = 180;
const int SERVO_CLOSED_ANGLE = 0;

// Sensors
const int IR_SENSOR_PIN = 2;      // LOW when bottle detected
const int CAP_SENSOR_PIN = A0;    // Higher value = water present
const int TRIG_PIN = 9;           // Ultrasonic
const int ECHO_PIN = 10;          // Ultrasonic

// =================
// System Parameters
// =================
const float SMALL_MAX = 15.0;      // ≤15cm = Small
const float MEDIUM_MAX = 25.0;     // 15-25cm = Medium
const int CAP_WATER_THRESHOLD = 500;  // Adjust based on your sensor
const float SENSOR_HEIGHT = 40.0;  // Distance from sensor to conveyor (cm)

// ===============
// Initialization
// ===============
void setup() {
  Serial.begin(9600);
  Serial.println("Bottle Inspection System Booting...");

  Wire.begin();  // I2C Bus
  initializeLCD();

  gateServo.attach(SERVO_PIN);
  gateServo.write(SERVO_CLOSED_ANGLE);

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  pinMode(BIN_TRIG_PIN, OUTPUT);
  pinMode(BIN_ECHO_PIN, INPUT);
  digitalWrite(BIN_TRIG_PIN, LOW);

  sim900.begin(9600);
  delay(1000);
  sim900.println("AT");
  delay(1000);
  sim900.println("AT+CMGF=1");
  delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();
}

// ============
// Main Loop
// ============
void loop() {
  if (bottleDetected()) {
    inspectBottle();
    while (bottleDetected()) { delay(10); }  // Wait for bottle to pass
    delay(2000);  // Display results
    lcd.clear();
  }
  delay(10);

  float binLevel = measureBinLevel();
  static bool binWasFull = false;

  if (binLevel < BIN_FULL_THRESHOLD && !binWasFull) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("!!! BIN FULL !!!");
    sendSMS("The Bin is FULL");
    binWasFull = true;
  } else if (binLevel >= BIN_FULL_THRESHOLD) {
    binWasFull = false;
  }
}

// ===================
// Core Functions
// ===================
bool bottleDetected() {
  static unsigned long lastDebounce = 0;
  static bool lastState = HIGH;

  bool currentState = digitalRead(IR_SENSOR_PIN);

  if (currentState != lastState) {
    lastDebounce = millis();
  }

  if ((millis() - lastDebounce) > 50) {
    if (currentState == LOW) return true;
  }

  lastState = currentState;
  return false;
}

void sendSMS(String message) {
  sim900.println("AT+CMGF=1");
  delay(500);
  sim900.println("AT+CMGS=\"+639312046126\"");
  delay(500);
  sim900.print(message);
  delay(500);
  sim900.write(26);
  delay(5000);
}

float measureBinLevel() {
  digitalWrite(BIN_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(BIN_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(BIN_TRIG_PIN, LOW);
  long duration = pulseIn(BIN_ECHO_PIN, HIGH);
  float distance = duration * 0.0343 / 2;
  return distance;
}

void inspectBottle() {
  float height = measureBottleHeight();
  String size = classifyBottleSize(height);
  bool hasWater = checkWaterPresence();
  bool accepted = !hasWater;

  updateDisplay(height, size, hasWater, accepted);

  if (accepted) {
    openGate();
    delay(5000);
    closeGate();

    // Pulse based on size
    if (size == "Small") {
      sendPulse(1);
    } else if (size == "Medium") {
      sendPulse(2);
    } else if (size == "Large") {
      sendPulse(3);
    }
  }

  logResults(height, size, hasWater, accepted);
}


// ======================
// Hardware Control
// ======================
void initializeLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Test");
  delay(500);
}

void updateDisplay(float height, String size, bool hasWater, bool accepted) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Bottle Detected   ");
  lcd.setCursor(0, 1); lcd.print("H:"); lcd.print(height, 1); lcd.print("cm  "); lcd.print(size);
  lcd.setCursor(0, 2); lcd.print("Water:"); lcd.print(hasWater ? "Present  " : "Absent  ");
  lcd.setCursor(0, 3); lcd.print("Status:"); lcd.print(accepted ? "ACCEPTED" : "REJECTED");
}

void openGate() {
  for (int pos = SERVO_CLOSED_ANGLE; pos <= SERVO_OPEN_ANGLE; pos += 2) {
    gateServo.write(pos);
    delay(20);
  }
}

void closeGate() {
  for (int pos = SERVO_OPEN_ANGLE; pos >= SERVO_CLOSED_ANGLE; pos -= 2) {
    gateServo.write(pos);
    delay(20);
  }
}

float measureBottleHeight() {
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.0343 / 2;
  return max(SENSOR_HEIGHT - distance, 0.0);
}

String classifyBottleSize(float height) {
  if (height <= SMALL_MAX) return "Small";
  else if (height <= MEDIUM_MAX) return "Medium";
  else return "Large";
}

bool checkWaterPresence() {
  int avg = 0;
  for (int i = 0; i < 5; i++) {
    avg += analogRead(CAP_SENSOR_PIN);
    delay(10);
  }
  avg /= 5;
  return (avg > CAP_WATER_THRESHOLD);
}

void sendPulse(int pulseCount) {
  for (int i = 0; i < pulseCount; i++) {
    digitalWrite(pulsePin, HIGH);  // Send pulse (HIGH)
    delay(50);  // Pulse duration in milliseconds
    digitalWrite(pulsePin, LOW);  // End pulse (LOW)
    delay(50);  // Brief delay between pulses
  }
}   
void logResults(float height, String size, bool hasWater, bool accepted) {
  Serial.print("\nHeight: "); Serial.print(height); Serial.print("cm");
  Serial.print(" | Size: "); Serial.print(size);
  Serial.print(" | Water: "); Serial.print(hasWater ? "Yes" : "No");
  Serial.print(" | Status: "); Serial.println(accepted ? "ACCEPTED" : "REJECTED");
  float binLevel = measureBinLevel();
  Serial.print(" | Bin Level: "); Serial.print(binLevel); Serial.println("cm");
}
