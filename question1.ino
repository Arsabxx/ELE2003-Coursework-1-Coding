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
enum Phase { LEFT_GREEN, LEFT_TO_STEM_YELLOW, STEM_GREEN, STEM_TO_LEFT_YELLOW, PED_GREEN };
Phase currentPhase = LEFT_GREEN;
Phase nextPhaseAfterPed = STEM_GREEN;  // Remember which phase to resume after pedestrian
unsigned long phaseStartTime = 0;

bool pedRequest = false;
bool goToPedAfterYellow = false;  // Flag to handle delayed pedestrian phase after yellow
int greenPhasesToWait = 0;  // New: Cooldown counter after pedestrian crossing (wait 2 green phases)

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
  
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Please Cross");
  lcd.setCursor(0, 1); lcd.print("Watch for cars!");
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
  lcd.print("T-Junction v5");
  delay(2000);
  lcd.clear();

  // Initial state: Left green (straight + turn), Stem red, Ped red
  setJunction(false, false, true, true, true, false, false);
  setPedRed();
  phaseStartTime = millis();
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
        lcd.clear();
        lcd.print("Pedestrian Request!");
        delay(1000);  // Short delay to show message, optional for debug
        // Note: LCD will update in phases later
      }
    }
  }
  lastButtonState = reading;

  unsigned long currentMillis = millis();
  unsigned long elapsed;

  switch (currentPhase) {

    case LEFT_GREEN:
      if (currentMillis - phaseStartTime >= GREEN_TIME) {
        if (pedRequest && greenPhasesToWait <= 0) {
          goToPedAfterYellow = true;
          nextPhaseAfterPed = STEM_GREEN;  // Would have gone to Stem next
        }
        // Always enter yellow
        currentPhase = LEFT_TO_STEM_YELLOW;
        phaseStartTime = currentMillis;
        setJunction(false, true, false, false, false, true, false);  // Both yellow
        lcd.clear(); lcd.print("Both Yellow");
      }
      break;

    case LEFT_TO_STEM_YELLOW:
      if (currentMillis - phaseStartTime >= YELLOW_TIME) {
        if (goToPedAfterYellow) {
          goToPedAfterYellow = false;
          pedRequest = false;  // Clear after handling
          goToPedPhase();
        } else {
          setJunction(true, false, false, false, false, false, true);  // Left red, Stem green
          currentPhase = STEM_GREEN;
          phaseStartTime = currentMillis;
          lcd.clear(); lcd.print("Stem Green");
          // Decrement cooldown if active
          if (greenPhasesToWait > 0) greenPhasesToWait--;
        }
      }
      break;

    case STEM_GREEN:
      if (currentMillis - phaseStartTime >= GREEN_TIME) {
        if (pedRequest && greenPhasesToWait <= 0) {
          goToPedAfterYellow = true;
          nextPhaseAfterPed = LEFT_GREEN;  // Would have gone to Left next
        }
        // Always enter yellow
        currentPhase = STEM_TO_LEFT_YELLOW;
        phaseStartTime = currentMillis;
        setJunction(false, true, false, false, false, true, false);  // Both yellow
        lcd.clear(); lcd.print("Both Yellow");
      }
      break;

    case STEM_TO_LEFT_YELLOW:
      if (currentMillis - phaseStartTime >= YELLOW_TIME) {
        if (goToPedAfterYellow) {
          goToPedAfterYellow = false;
          pedRequest = false;  // Clear after handling
          goToPedPhase();
        } else {
          setJunction(false, false, true, true, true, false, false);  // Left green, Stem red
          currentPhase = LEFT_GREEN;
          phaseStartTime = currentMillis;
          lcd.clear(); lcd.print("Left Green");
          // Decrement cooldown if active
          if (greenPhasesToWait > 0) greenPhasesToWait--;
        }
      }
      break;

    case PED_GREEN:
      elapsed = currentMillis - phaseStartTime;

      if (elapsed >= PED_TIME) {
        // Pedestrian phase ends → resume previous cycle
        digitalWrite(pedGreen, LOW);
        digitalWrite(pedRed, HIGH);
        noTone(buzzerPin);
        lcd.clear();
        lcd.print("Crossing Complete");
        delay(1500);
        lcd.clear();

        if (nextPhaseAfterPed == STEM_GREEN) {
          setJunction(true, false, false, false, false, false, true);
          currentPhase = STEM_GREEN;
          lcd.print("Stem Green");
        } else {
          setJunction(false, false, true, true, true, false, false);
          currentPhase = LEFT_GREEN;
          lcd.print("Left Green");
        }
        phaseStartTime = millis();
        greenPhasesToWait = 2;  // Start cooldown: wait 2 green phases before next ped

      } else if (elapsed >= PED_TIME - PED_FLASH_TIME) {
        // Last 3 seconds: flashing green + beeping
        bool flashOn = (millis() % 1000 < 500);
        digitalWrite(pedGreen, flashOn ? HIGH : LOW);

        // Beep every 600ms for 200ms
        if ((millis() % 600) < 200) {
          tone(buzzerPin, 1200);
        } else {
          noTone(buzzerPin);
        }

      } else {
        // Normal pedestrian green: continuous buzzer tone
        digitalWrite(pedGreen, HIGH);
        tone(buzzerPin, 1000);  // Continuous long tone
      }
      break;
  }
}