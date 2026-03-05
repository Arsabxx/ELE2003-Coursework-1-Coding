#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change to 0x3F if display doesn't work

// Pin definitions
const int leftRed       = 2;
const int leftYellow    = 3;
const int leftStraight  = 4;   // Straight green
const int leftTurn      = 5;   // Left-turn green
const int stemRed       = 6;
const int stemYellow    = 7;
const int stemGreen     = 8;
const int pedRed        = 9;
const int pedGreen      = 10;
const int buzzerPin     = 11;
const int buttonPin     = 12;

// Timing constants (in milliseconds)
const unsigned long GREEN_TIME     = 7000;  // Extended by 2 seconds
const unsigned long YELLOW_TIME    = 3000;
const unsigned long PED_TIME       = 10000;
const unsigned long PED_FLASH_TIME = 3000;  // Last 3 seconds: flashing + beeping

// State machine phases
enum Phase { LEFT_GREEN, LEFT_TO_STEM_YELLOW, STEM_GREEN, STEM_TO_LEFT_YELLOW, PED_GREEN, PED_TO_STEM_YELLOW };  // New phase for safety yellow after ped to stem
Phase currentPhase = LEFT_GREEN;
Phase nextPhaseAfterPed = STEM_GREEN;  // Remember which phase to resume after pedestrian
unsigned long phaseStartTime = 0;

bool pedRequest = false;
bool goToPedAfterYellow = false;  // Flag to handle delayed pedestrian phase after yellow
int greenPhasesToWait = 0;  // Cooldown counter after pedestrian crossing (wait 2 green phases)
unsigned long lastLcdUpdate = 0;  // For updating remaining seconds

// Set all vehicle lights at once (junction control)
void setJunction(bool lRed, bool lYel, bool lStr, bool lTur, bool sRed, bool sYel, bool sGre) {
  digitalWrite(leftRed,      lRed);
  digitalWrite(leftYellow,   lYel);
  digitalWrite(leftStraight, lStr);
  digitalWrite(leftTurn,     lTur);
  digitalWrite(stemRed,      sRed);
  digitalWrite(stemYellow,   sYel);
  digitalWrite(stemGreen,    sGre);
}

void setPedRed() {
  digitalWrite(pedRed, HIGH);
  digitalWrite(pedGreen, LOW);
}

// Enter pedestrian phase: left keeps straight green but turn off, stem full red
void goToPedPhase() {
  setJunction(false, false, true, false, true, false, false);  // Left straight ON, turn OFF; Stem red
  
  digitalWrite(pedGreen, HIGH);
  digitalWrite(pedRed, LOW);
  
  currentPhase = PED_GREEN;
  phaseStartTime = millis();
  lastLcdUpdate = 0;  // Reset for countdown
  
  updateLcd("Pedestrian Green", "Cross Now! 10s");
}

// Update LCD with status on line 1, message on line 2
void updateLcd(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1.substring(0, 16));  // Truncate if too long
  lcd.setCursor(0, 1); lcd.print(line2.substring(0, 16));
}

// Setup
void setup() {
  // Initialize all pins
  pinMode(leftRed, OUTPUT); pinMode(leftYellow, OUTPUT);
  pinMode(leftStraight, OUTPUT); pinMode(leftTurn, OUTPUT);
  pinMode(stemRed, OUTPUT); pinMode(stemYellow, OUTPUT); pinMode(stemGreen, OUTPUT);
  pinMode(pedRed, OUTPUT); pinMode(pedGreen, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  updateLcd("T-Junction v6", "Initializing...");
  delay(2000);

  // Initial state: Left green (straight + turn), Stem red, Ped red
  setJunction(false, false, true, true, true, false, false);
  setPedRed();
  phaseStartTime = millis();
  updateLcd("Left Green", "Normal Operation");
}

// Button debounce variables
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
int buttonState = HIGH;
int lastButtonState = HIGH;

// Main loop
void loop() {
  // Read button with debounce
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW && !pedRequest) {
        pedRequest = true;
        updateLcd(getCurrentStatus(), "Button Pressed");
        delay(1000);  // Show for 1s
        updateLcd(getCurrentStatus(), "Wait!");
      }
    }
  }
  lastButtonState = reading;

  unsigned long currentMillis = millis();
  unsigned long elapsed;

  // Helper to get current status for LCD line 1
  String currentStatus = getCurrentStatus();

  switch (currentPhase) {

    case LEFT_GREEN:
      if (currentMillis - phaseStartTime >= GREEN_TIME) {
        if (pedRequest && greenPhasesToWait <= 0) {
          goToPedAfterYellow = true;
          nextPhaseAfterPed = STEM_GREEN;
        }
        currentPhase = LEFT_TO_STEM_YELLOW;
        phaseStartTime = currentMillis;
        setJunction(false, true, false, false, false, true, false);  // Both yellow
        updateLcd("Both Yellow", pedRequest ? "Wait!" : "Normal");
      }
      break;

    case LEFT_TO_STEM_YELLOW:
      if (currentMillis - phaseStartTime >= YELLOW_TIME) {
        if (goToPedAfterYellow) {
          goToPedAfterYellow = false;
          pedRequest = false;
          goToPedPhase();
        } else {
          setJunction(true, false, false, false, false, false, true);  // Left red, Stem green
          currentPhase = STEM_GREEN;
          phaseStartTime = currentMillis;
          updateLcd("Stem Green", pedRequest ? "Wait!" : "Normal");
          if (greenPhasesToWait > 0) greenPhasesToWait--;
        }
      }
      break;

    case STEM_GREEN:
      if (currentMillis - phaseStartTime >= GREEN_TIME) {
        if (pedRequest && greenPhasesToWait <= 0) {
          goToPedAfterYellow = true;
          nextPhaseAfterPed = LEFT_GREEN;
        }
        currentPhase = STEM_TO_LEFT_YELLOW;
        phaseStartTime = currentMillis;
        setJunction(false, true, false, false, false, true, false);  // Both yellow
        updateLcd("Both Yellow", pedRequest ? "Wait!" : "Normal");
      }
      break;

    case STEM_TO_LEFT_YELLOW:
      if (currentMillis - phaseStartTime >= YELLOW_TIME) {
        if (goToPedAfterYellow) {
          goToPedAfterYellow = false;
          pedRequest = false;
          goToPedPhase();
        } else {
          setJunction(false, false, true, true, true, false, false);  // Left green, Stem red
          currentPhase = LEFT_GREEN;
          phaseStartTime = currentMillis;
          updateLcd("Left Green", pedRequest ? "Wait!" : "Normal");
          if (greenPhasesToWait > 0) greenPhasesToWait--;
        }
      }
      break;

    case PED_GREEN:
      elapsed = currentMillis - phaseStartTime;

      if (elapsed >= PED_TIME) {
        digitalWrite(pedGreen, LOW);
        digitalWrite(pedRed, HIGH);
        noTone(buzzerPin);
        updateLcd("Crossing Complete", "Normal");
        delay(1500);

        greenPhasesToWait = 2;  // Start cooldown

        if (nextPhaseAfterPed == LEFT_GREEN) {
          // Directly to Left Green
          setJunction(false, false, true, true, true, false, false);
          currentPhase = LEFT_GREEN;
          updateLcd("Left Green", "Normal");
        } else {
          // To new yellow phase before Stem Green
          setJunction(false, true, false, false, false, true, false);  // Both yellow
          currentPhase = PED_TO_STEM_YELLOW;
          updateLcd("Both Yellow", "Normal");
        }
        phaseStartTime = millis();

      } else {
        // Update countdown every second
        int remainingSec = (PED_TIME - elapsed) / 1000 + 1;  // Round up
        if (currentMillis - lastLcdUpdate >= 1000) {
          updateLcd("Pedestrian Green", "Cross Now! " + String(remainingSec) + "s");
          lastLcdUpdate = currentMillis;
        }

        if (elapsed >= PED_TIME - PED_FLASH_TIME) {
          // Flashing + beeping
          bool flashOn = (millis() % 1000 < 500);
          digitalWrite(pedGreen, flashOn ? HIGH : LOW);

          if ((millis() % 600) < 200) {
            tone(buzzerPin, 1200);
          } else {
            noTone(buzzerPin);
          }
        } else {
          // Normal green + continuous tone
          digitalWrite(pedGreen, HIGH);
          tone(buzzerPin, 1000);
        }
      }
      break;

    case PED_TO_STEM_YELLOW:
      if (currentMillis - phaseStartTime >= YELLOW_TIME) {
        setJunction(true, false, false, false, false, false, true);  // Left red, Stem green
        currentPhase = STEM_GREEN;
        phaseStartTime = currentMillis;
        updateLcd("Stem Green", "Normal");
        if (greenPhasesToWait > 0) greenPhasesToWait--;
      }
      break;
  }
}

// Helper to get current status string
String getCurrentStatus() {
  switch (currentPhase) {
    case LEFT_GREEN: return "Left Green";
    case STEM_GREEN: return "Stem Green";
    case LEFT_TO_STEM_YELLOW:
    case STEM_TO_LEFT_YELLOW:
    case PED_TO_STEM_YELLOW: return "Both Yellow";
    case PED_GREEN: return "Pedestrian Green";
    default: return "Unknown";
  }
}