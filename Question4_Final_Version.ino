#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// === Pin Definitions ===
#define TRIG_PIN          5     // Ultrasonic sensor Trigger pin
#define ECHO_PIN          6     // Ultrasonic sensor Echo pin
#define SS_PIN           10     // MFRC522 SPI SS (SDA) pin
#define RST_PIN           4     // MFRC522 Reset pin
#define SERVO_PIN         9     // Servo motor signal pin
#define LED_PIN           7     // Status LED (green - scan card prompt)
#define THERMISTOR_PIN   A0     // NTC thermistor analog input pin

// === OLED Configuration (SSD1306 128x64 I2C) ===
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT     64
#define OLED_RESET       -1     // No reset pin used
#define SCREEN_ADDRESS  0x3C    // I2C address (change to 0x3D if needed)

// === Thermistor Parameters (NTC 10kΩ, β=3950) ===
const float NOMINAL_RESISTANCE  = 10000.0;  // 10kΩ at 25°C
const float NOMINAL_TEMPERATURE = 25.0;     // Reference temperature °C
const float BETA_COEFFICIENT    = 3950.0;   // Beta value of thermistor
const float SERIES_RESISTOR     = 10000.0;  // Fixed 10kΩ resistor in voltage divider

// === Adjustable System Parameters ===
const int TRIGGER_DISTANCE   = 20;     // Distance threshold to detect vehicle (cm)
const int GATE_OPEN_ANGLE    = 90;    // Servo angle for gate OPEN (adjust based on direction)
const int GATE_CLOSED_ANGLE  = 0;      // Servo angle for gate CLOSED
const int GATE_OPEN_DURATION = 8000;   // Time (ms) to keep gate open after valid card

// === Authorized RFID Card UIDs (4-byte classic MIFARE) ===
byte authorizedUIDs[][4] = {
  {0x90, 0x39, 0xC7, 0x22},   // Example UID 1 - REPLACE WITH REAL ONES
  {0xF3, 0xDD, 0x5B, 0xA7}    // Example UID 2
};
const int NUM_AUTHORIZED = 2;

// === Global Objects ===
Servo barrier;
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// === State Variables ===
bool carDetected     = false;
bool gateOpen        = false;
unsigned long openStartTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Attach and initialize servo
  barrier.attach(SERVO_PIN);
  barrier.write(GATE_CLOSED_ANGLE);   // Start with gate closed

  // Initialize SPI and RFID module
  SPI.begin();
  rfid.PCD_Init();

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 initialization failed"));
    for(;;);  // Halt if display fails
  }

  // Welcome message on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(F("Parking Entrance"));
  display.setCursor(0, 30);
  display.println(F("Approach Vehicle"));
  display.display();

  Serial.println(F("System Ready - Waiting for vehicle"));
}

void loop() {
  // Read current distance and temperature
  int distance = getDistance();
  float temperature = getTemperature();

  // Print distance and temperature to Serial every loop (for monitoring)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 800) {  // Print every 800ms to avoid flooding
    lastPrint = millis();

    Serial.print(F("Distance: "));
    if (distance > 0) {
      Serial.print(distance);
      Serial.print(F(" cm"));
    } else {
      Serial.print(F("Out of range"));
    }

    Serial.print(F("   |   Temp: "));
    if (temperature > -50 && temperature < 100) {  // Reasonable range
      Serial.print(temperature, 1);
      Serial.println(F(" C"));
    } else {
      Serial.println(F("N/A"));
    }
  }

  // Step 1: Detect approaching vehicle
  if (distance > 0 && distance < TRIGGER_DISTANCE && !carDetected && !gateOpen) {
    carDetected = true;
    digitalWrite(LED_PIN, HIGH);
    displayMessage("Vehicle Detected", "Please Scan Card");
    Serial.println(F("=== Vehicle detected! Please scan RFID card ==="));
  }

  // Step 2: Check for valid RFID card when vehicle is detected
  if (carDetected && !gateOpen) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      Serial.print(F("Card UID: "));
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
        Serial.print(rfid.uid.uidByte[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // Check if card is authorized
      bool valid = false;
      for (int i = 0; i < NUM_AUTHORIZED; i++) {
        if (memcmp(rfid.uid.uidByte, authorizedUIDs[i], 4) == 0) {
          valid = true;
          break;
        }
      }

      if (valid) {
        displayMessage("Access Granted", "Gate Opening");
        barrier.write(GATE_OPEN_ANGLE);
        gateOpen = true;
        openStartTime = millis();
        digitalWrite(LED_PIN, LOW);
        Serial.println(F("Valid card → Gate OPEN"));
      } else {
        displayMessage("Invalid Card", "Try Again");
        Serial.println(F("Invalid card!"));
        delay(2000);
        displayMessage("Vehicle Detected", "Please Scan Card");
      }

      rfid.PICC_HaltA();  // Stop reading current card
    }
  }

  // Step 3: Auto-close gate after timeout
  if (gateOpen && (millis() - openStartTime > GATE_OPEN_DURATION)) {
    barrier.write(GATE_CLOSED_ANGLE);
    gateOpen = false;
    carDetected = false;
    displayMessage("Parking Entrance", "Approach Vehicle");
    Serial.println(F("Gate auto CLOSED"));
  }

  delay(100);  // Main loop delay
}

// === Read temperature from NTC thermistor ===
float getTemperature() {
  int rawValue = analogRead(THERMISTOR_PIN);
  if (rawValue <= 0 || rawValue >= 1023) return -999.0;  // Invalid reading

  float resistance = SERIES_RESISTOR * (1023.0 / rawValue - 1.0);
  float steinhart = resistance / NOMINAL_RESISTANCE;
  steinhart = log(steinhart);
  steinhart /= BETA_COEFFICIENT;
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
  steinhart = 1.0 / steinhart;
  float tempC = steinhart - 273.15;

  return tempC;
}

// === Temperature-compensated ultrasonic distance measurement ===
int getDistance() {
  float tempC = getTemperature();
  if (tempC < -50 || tempC > 100) return -1;  // Invalid temperature

  // Speed of sound in air (m/s) adjusted by temperature
  float speedOfSound = 331.3 + (0.606 * tempC);

  // Trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo time (timeout 30ms ≈ 5m max range)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;

  // Calculate distance in cm (round trip / 2)
  float distanceCm = (duration * speedOfSound) / 20000.0;

  // Filter unrealistic values
  if (distanceCm < 2 || distanceCm > 400) return -1;

  return (int)round(distanceCm);
}

// === Display two-line message on OLED ===
void displayMessage(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(line1);
  display.setCursor(0, 30);
  display.println(line2);
  display.display();
}
