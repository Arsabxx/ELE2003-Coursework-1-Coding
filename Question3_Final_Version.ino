#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

Servo myservo;  // Servo motor object

int servoPin = 9;              // Servo signal pin
int buttonContinuous = 2;      // Button for continuous mode (toggle start/stop)
int buttonSingle = 3;          // Button for single action
int trigPin = 4;               // Ultrasonic sensor Trig pin
int echoPin = 5;               // Ultrasonic sensor Echo pin
int potPin = A0;               // Potentiomet Pin

LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD screen

int upAngle = 0;               // Lift-up angle (default 0 degrees)
int downAngle = 90;            // Press-down angle (default 90 degrees, adjustable via Serial)
bool running = false;          // Continuous mode status
unsigned long lastDebounceTime1 = 0;  // Debounce timer for button 1
unsigned long lastDebounceTime2 = 0;  // Debounce timer for button 2
int debounceDelay = 50;        // Debounce delay in ms
int lastButtonState1 = HIGH;   // Previous state of button 1
int lastButtonState2 = HIGH;   // Previous state of button 2
unsigned long cycleCount = 0;  // Count of completed stamp cycles

void setup() {
  myservo.attach(servoPin);            // Attach servo to pin
  pinMode(buttonContinuous, INPUT_PULLUP);  // Button 1 with internal pull-up
  pinMode(buttonSingle, INPUT_PULLUP);      // Button 2 with internal pull-up
  pinMode(trigPin, OUTPUT);        // Ultrasonic trigger pin as output
  pinMode(echoPin, INPUT);         // Ultrasonic echo pin as input
  Serial.begin(9600);              // Start Serial Monitor at 9600 baud
  myservo.write(upAngle);          // Start at lift-up position
  Serial.println("System ready. Press button1 to start/stop continuous mode.");
  Serial.println("Press button2 for single action.");
  Serial.println("Enter a number (e.g., 120) to adjust down angle.");

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("STAMP READY");
  delay(1500);
  lcd.clear();

  myservo.write(upAngle);          
  updateLCD(); // Initial display
}

void loop() {

  int potValue = analogRead(potPin);

  int mappedAngle = map(potValue, 0, 1023, 0, 90);

  if (abs(mappedAngle - downAngle) > 2) { 
    downAngle = mappedAngle;
    updateLCD();
    Serial.print("New Down Angle set by Pot: ");
    Serial.println(downAngle);
  }

  // Check Serial input to adjust down angle
  if (Serial.available() > 0) {
    int newAngle = Serial.parseInt();
    if (newAngle > 0 && newAngle <= 180) {
      downAngle = newAngle;
      Serial.print("Down angle adjusted to: ");
      Serial.println(downAngle);
    }
  }

  // Read button 1 (continuous mode toggle)
  int reading1 = digitalRead(buttonContinuous);
  if (reading1 != lastButtonState1) {
    lastDebounceTime1 = millis();
  }
  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if (reading1 == LOW) {          // Button pressed
      running = !running;           // Toggle mode
      updateLCD();
      if (running) {
        Serial.println("Continuous mode started.");
      } else {
        Serial.println("Continuous mode stopped.");
        myservo.write(upAngle);     // Return to lift-up when stopped
        updateLCD();
      }
    }
  }
  lastButtonState1 = reading1;

  // Read button 2 (single action)
  int reading2 = digitalRead(buttonSingle);
  if (reading2 != lastButtonState2) {
    lastDebounceTime2 = millis();
  }
  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if (reading2 == LOW) {          // Button pressed
      doSingleCycle();              // Perform one single cycle
      updateLCD();
    }
  }
  lastButtonState2 = reading2;

  // Continuous mode logic
  if (running) {
    if (checkSensor()) {            // If obstacle detected, stop
      running = false;
      Serial.println("Obstacle detected! Continuous mode stopped.");
      myservo.write(upAngle);
    } else {
      doCycle();                    // Perform one full cycle
      updateLCD();
      delay(500);                  // 0.5 seconds per cycle (including movement time)
    }
  }

  // Print status data every second (non-blocking)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    printData();
    lastPrint = millis();
  }
}

// Perform one full stamp cycle: press down → wait → lift up
void doCycle() {
  myservo.write(downAngle);   // Press down
  delay(500);                 // Hold for 0.5 seconds
  myservo.write(upAngle);     // Lift up
  delay(500);                 // Hold for 0.5 seconds
  cycleCount++;               // Increment cycle counter
}

// Perform single action (with obstacle check)
void doSingleCycle() {
  if (checkSensor()) {
    Serial.println("Obstacle detected! Single action cancelled.");
    return;
  }
  Serial.println("Performing single action.");
  doCycle();
}

// Check ultrasonic sensor for obstacle
// Returns true if obstacle is too close (< 20 cm)
bool checkSensor() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;  // Distance in cm
  return (distance < 20 && distance > 0);  // Treat <20cm as obstacle
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Angle: ");
  lcd.print(downAngle);
  lcd.print(" deg   "); 

  lcd.setCursor(0, 1);
  if (running) {
    lcd.print("RUN | Count: ");
  } else {
    lcd.print("OFF | Count: ");
  }
  lcd.print(cycleCount);
}

// Print current status to Serial Monitor
void printData() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;

  Serial.print("Current angle: ");
  Serial.print(myservo.read());
  Serial.print(" degrees | Distance: ");
  Serial.print(distance);
  Serial.print(" cm | Cycle count: ");
  Serial.println(cycleCount);
}
