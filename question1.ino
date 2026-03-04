// T-Junction with separated main left/right + stem exit + pedestrian on stem
// - Main left and right NOT simultaneous for turning into stem
// - Ped request waits for full cycle end
// - Auto ped every N cycles if no button pressed

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pins (example - adjust)
const int LEFT_G  = 13; const int LEFT_Y  = 12; const int LEFT_R  = 11;
const int RIGHT_G = 10; const int RIGHT_Y = 9;  const int RIGHT_R = 8;
const int STEM_G  = 7;  const int STEM_Y  = 6;  const int STEM_R  = 5;
const int PED_G   = 4;  const int PED_R   = 3;
const int BUTTON  = 2;

// Timings
const unsigned long LEFT_GO_TIME   = 20000UL;
const unsigned long RIGHT_GO_TIME  = 20000UL;
const unsigned long STEM_GO_TIME   = 15000UL;
const unsigned long YELLOW_TIME    =  4000UL;
const unsigned long PED_TIME       = 12000UL;
const unsigned long PED_FLASH_TIME =  4000UL;

// Auto ped every how many full cycles (if no button)
const int AUTO_PED_EVERY_CYCLES = 2;

// States
enum TrafficState {
  MAIN_LEFT_GO,
  MAIN_RIGHT_GO,
  STEM_GO,
  YELLOW_CLEAR,
  PED_WALK,
  PED_FLASH
};

TrafficState currentState = MAIN_LEFT_GO;
unsigned long stateStart = 0;
bool pedRequested = false;
bool btnDebounce = false;
int cycleCount = 0;           

void setup() {
  initPins();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  display.clearDisplay();
  display.display();
  
  startMainLeftGo();
}

void loop() {
  checkButton();
  updateSystem();
}

void initPins() {
  pinMode(LEFT_G, OUTPUT); pinMode(LEFT_Y, OUTPUT); pinMode(LEFT_R, OUTPUT);
  pinMode(RIGHT_G, OUTPUT); pinMode(RIGHT_Y, OUTPUT); pinMode(RIGHT_R, OUTPUT);
  pinMode(STEM_G, OUTPUT); pinMode(STEM_Y, OUTPUT); pinMode(STEM_R, OUTPUT);
  pinMode(PED_G, OUTPUT); pinMode(PED_R, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  allOff();
}

void allOff() {
  digitalWrite(LEFT_G, LOW); digitalWrite(LEFT_Y, LOW); digitalWrite(LEFT_R, LOW);
  digitalWrite(RIGHT_G, LOW); digitalWrite(RIGHT_Y, LOW); digitalWrite(RIGHT_R, LOW);
  digitalWrite(STEM_G, LOW); digitalWrite(STEM_Y, LOW); digitalWrite(STEM_R, LOW);
  digitalWrite(PED_G, LOW); digitalWrite(PED_R, LOW);
}

// State functions
void startMainLeftGo() {
  allOff();
  digitalWrite(LEFT_G, HIGH);
  digitalWrite(RIGHT_R, HIGH);
  digitalWrite(STEM_R, HIGH);
  digitalWrite(PED_R, HIGH);
  
  currentState = MAIN_LEFT_GO;
  stateStart = millis();
  
  show("MAIN LEFT GO", "Straight + Right Turn In");
}

void startMainRightGo() {
  allOff();
  digitalWrite(RIGHT_G, HIGH);
  digitalWrite(LEFT_R, HIGH);
  digitalWrite(STEM_R, HIGH);
  digitalWrite(PED_R, HIGH);
  
  currentState = MAIN_RIGHT_GO;
  stateStart = millis();
  
  show("MAIN RIGHT GO", "Straight + Left Turn In");
}

void startStemGo() {
  allOff();
  digitalWrite(LEFT_R, HIGH);
  digitalWrite(RIGHT_R, HIGH);
  digitalWrite(STEM_G, HIGH);
  digitalWrite(PED_R, HIGH);
  
  currentState = STEM_GO;
  stateStart = millis();
  
  show("STEM GO", "Turn Left/Right Out");
}

void startYellowClear() {
  digitalWrite(LEFT_G, LOW);  digitalWrite(LEFT_Y, HIGH);
  digitalWrite(RIGHT_G, LOW); digitalWrite(RIGHT_Y, HIGH);
  digitalWrite(STEM_G, LOW);  digitalWrite(STEM_Y, HIGH);
  
  currentState = YELLOW_CLEAR;
  stateStart = millis();
  
  show("YELLOW CLEAR", "All Prepare to Stop");
  
  cycleCount++;
}

void startPedWalk() {
  allOff();
  digitalWrite(LEFT_R, HIGH);
  digitalWrite(RIGHT_R, HIGH);
  digitalWrite(STEM_R, HIGH);
  digitalWrite(PED_G, HIGH);
  
  currentState = PED_WALK;
  stateStart = millis();
  
  show("WALK NOW", "All Vehicles Red");
}

void startPedFlash() {
  currentState = PED_FLASH;
  stateStart = millis();
  
  show("HURRY UP!", "Ped Flashing");
}

// Button check (only effective in vehicle phases)
void checkButton() {
  if (currentState == PED_WALK || currentState == PED_FLASH) return;
  
  if (digitalRead(BUTTON) == LOW && !btnDebounce) {
    btnDebounce = true;
    pedRequested = true;
    show("Ped Requested", "Wait for cycle end");
  }
  if (digitalRead(BUTTON) == HIGH) btnDebounce = false;
}

// Main update
void updateSystem() {
  unsigned long now = millis();
  unsigned long elapsed = now - stateStart;
  
  switch (currentState) {
    case MAIN_LEFT_GO:
      if (elapsed >= LEFT_GO_TIME) {
        startMainRightGo();
      }
      break;
      
    case MAIN_RIGHT_GO:
      if (elapsed >= RIGHT_GO_TIME) {
        startStemGo();
      }
      break;
      
    case STEM_GO:
      if (elapsed >= STEM_GO_TIME) {
        startYellowClear();
      }
      break;
      
    case YELLOW_CLEAR:
      if (elapsed >= YELLOW_TIME) {
        bool needPed = pedRequested || (cycleCount % AUTO_PED_EVERY_CYCLES == 0 && cycleCount > 0);
        if (needPed) {
          pedRequested = false;
          startPedWalk();
        } else {
          startMainLeftGo(); 
        }
      }
      break;
      
    case PED_WALK:
      updateCountdown(elapsed);
      if (elapsed >= PED_TIME - PED_FLASH_TIME) {
        startPedFlash();
      }
      break;
      
    case PED_FLASH:
      digitalWrite(PED_G, (now / 500) % 2);
      updateCountdown(elapsed);
      if (elapsed >= PED_TIME) {
        startMainLeftGo();
      }
      break;
  }
}

// OLED
void show(String l1, String l2) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,10);
  display.println(l1);
  display.setTextSize(1);
  display.setCursor(0,40);
  display.println(l2);
  display.display();
}

void updateCountdown(unsigned long elapsed) {
  unsigned long remain = PED_TIME - elapsed;
  int sec = remain / 1000;
  
  display.clearDisplay();
  display.setTextSize(4);
  display.setCursor(30,10);
  display.print(sec < 10 ? "0" : "");
  display.print(sec);
  display.print("s");
  display.setTextSize(1);
  display.setCursor(0,50);
  display.println(currentState == PED_FLASH ? "HURRY! FLASHING" : "WALK NOW");
  display.display();
}