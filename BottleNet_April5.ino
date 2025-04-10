#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ======================
// Hardware Configuration
// ======================

// LCD (Try both addresses if needed)
LiquidCrystal_I2C lcd(0x27, 20, 4);  // 0x27 or 0x3F

// Servo
Servo gateServo;
const int SERVO_PIN = 11;
const int SERVO_OPEN_ANGLE = 180;
const int SERVO_CLOSED_ANGLE = 0;

// Sensors
const int IR_SENSOR_PIN = 2;      // LOW when bottle detected
const int CAP_SENSOR_PIN = A0;    // Higher value = water present
const int TRIG_PIN = 9;           // Ultrasonic
const int ECHO_PIN = 10;          // Ultrasonic

// Indicators
const int BUZZER_PIN = 8;
const int GREEN_LED = 5;           // ACCEPT
const int RED_LED = 6;             // REJECT

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

  // Initialize I2C bus first
  Wire.begin();

  // LCD Setup with Error Handling
  initializeLCD();

  // Servo Setup
  gateServo.attach(SERVO_PIN);
  gateServo.write(SERVO_CLOSED_ANGLE);

  // Sensor Setup
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Indicator Setup
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // System Ready
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  startupBeep();
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
  
  if ((millis() - lastDebounce) > 50) {  // 50ms debounce
    if (currentState == LOW) return true;
  }
  
  lastState = currentState;
  return false;
}

void inspectBottle() {
  // Measure properties
  float height = measureBottleHeight();
  String size = classifyBottleSize(height);
  bool hasWater = checkWaterPresence();
  bool accepted = !hasWater;  // Only accept empty bottles

  // Update Display
  updateDisplay(height, size, hasWater, accepted);

  // Control outputs
  digitalWrite(GREEN_LED, accepted ? HIGH : LOW);
  digitalWrite(RED_LED, accepted ? LOW : HIGH);

  // Handle acceptance/rejection
  if (accepted) {
    openGate();
    acceptBeep();
    delay(5000);  // Gate open time
    closeGate();
  } else {
    rejectBeep();
  }

  // Debug output
  logResults(height, size, hasWater, accepted);
}

// ======================
// Hardware Control
// ======================

void initializeLCD() {
  lcd.init();
  lcd.backlight();
  
  // Test communication
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

// =================
// Feedback Signals
// =================
void startupBeep() {
  beep(100); delay(50); beep(100);
}

void acceptBeep() {
  beep(200); delay(100); beep(200);
}

void rejectBeep() {
  beep(1000);
}

void beep(int duration) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
}

void logResults(float height, String size, bool hasWater, bool accepted) {
  Serial.print("\nHeight: "); Serial.print(height); Serial.print("cm");
  Serial.print(" | Size: "); Serial.print(size);
  Serial.print(" | Water: "); Serial.print(hasWater ? "Yes" : "No");
  Serial.print(" | Status: "); Serial.println(accepted ? "ACCEPTED" : "REJECTED");
}