// Pin definitions
#define TRIG_PIN     5     // Ultrasonic Trigger pin
#define ECHO_PIN     6     // Ultrasonic Echo pin
#define SS_PIN      10     // RC522 SPI SS (SDA) pin
#define RST_PIN      4     // RC522 Reset pin
#define SERVO_PIN    9     // Servo signal pin
#define LED_PIN      7     // Optional LED pin (e.g. green for "scan card" prompt)
#define THERMISTOR_PIN A0  // Thermistor analog pin

// --- THERMISTOR CONSTANTS ---
const float nominal_resistance = 10000; // 10k ohm
const float nominal_temperature = 25;   // 25 degrees C
const float beta_coefficient = 3950;    // beta value
const float series_resistor = 10000;    // The 10k resistor in the circuit

// Adjustable parameters
const int triggerDist   = 120;    // Distance threshold (cm) to detect vehicle
const int openAngle     = 0;      // Servo angle for gate OPEN (adjust to your setup)
const int closedAngle   = 90;     // Servo angle for gate CLOSED
const int gateOpenTime  = 8000;   // Time (ms) to keep gate open after valid card

// Authorized RFID card UIDs (replace with your real card UIDs)
byte authorizedUIDs[][4] = {
  {0xA1, 0xB2, 0xC3, 0xD4},   // Example UID 1 - CHANGE THIS!
  {0xE5, 0xF6, 0xA7, 0xB8}    // Example UID 2 - Add more as needed
};
const int numAuthorized = 2;      // Number of valid UIDs in the array above

Servo barrier;
MFRC522 rfid(SS_PIN, RST_PIN);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);  // OLED I2C (SSD1306 128x64)

bool carDetected = false;
bool gateOpen = false;
unsigned long openStartTime = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  barrier.attach(SERVO_PIN);
  barrier.write(closedAngle);  // Start with gate closed
  
  SPI.begin();
  rfid.PCD_Init();             // Initialize MFRC522
  
  u8g2.begin();
  displayMessage("Parking Entrance", "Approach Vehicle");
  Serial.println("System Ready - Waiting for vehicle");
}

void loop() {
  int distance = getDistance();
  
  // Step 1: Detect vehicle approaching
  if (distance > 0 && distance < triggerDist && !carDetected && !gateOpen) {
    carDetected = true;
    digitalWrite(LED_PIN, HIGH);                // Turn on LED to indicate "scan card"
    displayMessage("Vehicle Detected", "Please Scan Card");
    Serial.println("Car detected! Please scan RFID card");
  }
  
  // Step 2: Wait for valid RFID card scan
  if (carDetected && !gateOpen) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      Serial.print("Card UID: ");
      String uidStr = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(rfid.uid.uidByte[i], HEX);
        uidStr += String(rfid.uid.uidByte[i], HEX);
      }
      Serial.println();
      
      // Check if card is authorized
      bool valid = false;
      for (int i = 0; i < numAuthorized; i++) {
        if (memcmp(rfid.uid.uidByte, authorizedUIDs[i], 4) == 0) {
          valid = true;
          break;
        }
      }
      
      if (valid) {
        displayMessage("Access Granted", "Gate Opening");
        barrier.write(openAngle);
        gateOpen = true;
        openStartTime = millis();
        digitalWrite(LED_PIN, LOW);
        Serial.println("Valid card! Gate OPEN");
      } else {
        displayMessage("Invalid Card", "Try Again");
        Serial.println("Invalid card!");
        delay(2000);  // Show error for 2 seconds
        displayMessage("Vehicle Detected", "Please Scan Card");
      }
      
      rfid.PICC_HaltA();  // Halt PICC (stop reading current card)
    }
  }
  
  // Step 3: Auto-close gate after timeout
  if (gateOpen && (millis() - openStartTime > gateOpenTime)) {
    barrier.write(closedAngle);
    gateOpen = false;
    carDetected = false;
    displayMessage("Parking Entrance", "Approach Vehicle");
    Serial.println("Gate CLOSED");
  }
  
  delay(100);  // Small delay to avoid high CPU usage
}

// --- ADVANCED TEMPERATURE-COMPENSATED DISTANCE CALCULATION ---
int getDistance() {
  // 1. Read temperature sensor
  int rawValue = analogRead(THERMISTOR_PIN);
  if (rawValue == 0) rawValue = 1; // Safety check to prevent divide-by-zero
  
  // 2. Calculate resistance and Steinhart-Hart
  float resistance = series_resistor * (1023.0 / rawValue - 1.0);
  float steinhart = resistance / nominal_resistance;
  steinhart = log(steinhart);
  steinhart /= beta_coefficient;
  steinhart += 1.0 / (nominal_temperature + 273.15);
  steinhart = 1.0 / steinhart;
  float tempC = steinhart - 273.15;

  // 3. Calculate Speed of Sound (converted directly to cm/us)
  // Base speed in m/s = 331.3 + (0.606 * tempC)
  // Multiply by 0.0001 to convert m/s directly to cm/us
  float speed_of_sound_cm_us = (331.3 + (0.606 * tempC)) * 0.0001;

  // 4. Trigger Ultrasonic Sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // 5. Read Echo with 30ms timeout
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1; // Out of range or path clear
  
  // 6. Calculate distance in cm and return it
  return (duration * speed_of_sound_cm_us) / 2.0;
}

// Display two-line message on OLED
void displayMessage(String line1, String line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);  
  u8g2.setCursor(0, 20);
  u8g2.print(line1);
  u8g2.setCursor(0, 45);
  u8g2.print(line2);
  u8g2.sendBuffer();
}
