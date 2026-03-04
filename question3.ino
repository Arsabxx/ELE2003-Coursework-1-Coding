#include <Servo.h>

Servo stamper;                // Create servo object

// Pin definitions
const int buttonPin   = 2;    // Button pin (active LOW with internal pull-up)
const int trigPin     = 5;    // Ultrasonic sensor Trigger pin
const int echoPin     = 6;    // Ultrasonic sensor Echo pin
const int servoPin    = 9;    // Servo signal pin

// Adjustable parameters
const int DOWN_ANGLE  = 0;    // Angle when stamping down (adjust to your stamp position)
const int UP_ANGLE    = 80;   // Angle when lifted up (rest position)
const int cycleTime   = 1500; // Desired full cycle time in ms (≈1.5 seconds)
const int safeDistance = 15;  // If distance < this value (cm), consider hand detected

// Variables
bool isRunning = false;       // Auto stamping mode flag
int lastButtonState = HIGH;   // For debouncing
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);       // For debugging
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  stamper.attach(servoPin);
  stamper.write(UP_ANGLE);    // Start with stamp lifted
  
  Serial.println("Automatic Stamp Machine Ready");
  Serial.println("Press button to start/stop auto mode");
}

void loop() {
  // Handle button press (with debounce)
  int reading = digitalRead(buttonPin);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Button state is stable
    static bool lastStable = HIGH;
    if (reading != lastStable) {
      lastStable = reading;
      
      if (lastStable == LOW) {   // Button pressed
        isRunning = !isRunning;
        Serial.print("Auto mode: ");
        Serial.println(isRunning ? "ON" : "OFF");
        
        if (!isRunning) {
          stamper.write(UP_ANGLE);  // Lift stamp when stopped
        }
      }
    }
  }
  lastButtonState = reading;

  // Main logic: only run if auto mode is active
  if (isRunning) {
    // Check for obstacle/hand
    int distance = getDistance();
    
    if (distance > 0 && distance < safeDistance) {
      Serial.print("Hand detected! Distance: ");
      Serial.print(distance);
      Serial.println(" cm → Pausing");
      stamper.write(UP_ANGLE);   // Safety: lift stamp
      delay(300);                // Short delay to avoid spamming
    } 
    else {
      // Perform one stamping cycle
      doOneStamp();
    }
  }
  
  delay(10);   // Small delay to reduce CPU load
}

// Measure distance in cm (HC-SR04)
int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000);  // Timeout after 30ms
  
  if (duration == 0) return -1;  // Error or out of range
  int cm = duration * 0.034 / 2;
  return cm;
}

// Perform one complete stamp cycle
void doOneStamp() {
  // Stamp down
  stamper.write(DOWN_ANGLE);
  delay(400);               // Dwell time at bottom (adjustable)
  
  // Lift up
  stamper.write(UP_ANGLE);
  delay(700);               // Time after lifting
  
  // Pad remaining time to reach desired cycle time
  delay(cycleTime - 1100);  // Adjust if you change the delays above
}