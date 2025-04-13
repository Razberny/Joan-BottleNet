#include <SoftwareSerial.h>

#define SIM_RX_PIN 7  // Arduino TX → SIM900 RX
#define SIM_TX_PIN 6  // Arduino RX ← SIM900 TX

SoftwareSerial sim900(SIM_RX_PIN, SIM_TX_PIN);

void setup() {
  Serial.begin(9600);        // Serial monitor
  sim900.begin(9600);        // SIM900A baud rate
  delay(2000);

  Serial.println("Initializing SIM900A...");

  sendATCommand("AT", "OK");         // Check SIM900A
  sendATCommand("AT+CMGF=1", "OK");  // Set SMS to text mode
  sendATCommand("AT+CSQ", "");       // Signal strength
  sendATCommand("AT+CCID", "");      // SIM card info
  sendATCommand("AT+COPS?", "");     // Network operator
}

void loop() {
  if (checkSIM900(3000)) {
    sendSMS("+639473732949", "This is a test SMS from Arduino Uno + SIM900A.");
  } else {
    Serial.println("SIM900A not responding.");
  }
  delay(60000);  // Wait 10 seconds before trying again
}

bool checkSIM900(unsigned long timeout) {
  sim900.println("AT");

  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (sim900.available()) {
      String response = sim900.readString();
      Serial.print("SIM900A response: ");
      Serial.println(response);
      if (response.indexOf("OK") != -1) {
        return true;
      }
    }
  }
  return false;
}

void sendSMS(String number, String message) {
  Serial.println("Sending SMS...");

  sim900.println("AT+CMGS=\"" + number + "\"");
  delay(2000);  // Give time for '>' prompt
  sim900.print(message);
  delay(500);
  sim900.write(26);  // Ctrl+Z to send
  delay(5000);       // Wait for delivery response

  Serial.println("SMS sent (check response below):");

  // Read and print the modem's response
  while (sim900.available()) {
    Serial.write(sim900.read());
  }
  delay(60000);
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
