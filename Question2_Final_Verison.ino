#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int ldrPin = A0;
const int potPin = A1;
const int ledPin = 3; 

//Serial Timer (Faster for graphing)
unsigned long previousSerialMillis = 0;
const long serialInterval = 20; 

//LCD Timer (Slower for LCD)
unsigned long previousLcdMillis = 0;
const long lcdInterval = 1000;

LiquidCrystal_I2C lcd(0x27, 16, 2);

//PID Setup
float Kp = 2.65; // Proportional Tuning 
float Ki = 0.70; // Integral Tuning
float Kd = 0.0; // Derivative Tuning

float integral = 0; // Accumulates the error over time
float previousError = 0; // Remembers the last error to calculate the speed of change
int currentPWM = 0; // Storing Final LED Brightness

float ldrFiltered = 500; 
float smoothingFactor = 0.15;

unsigned long previousPidTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);

  Wire.begin();
  Wire.setClock(100000);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  delay(100);

  lcd.setCursor(0, 0);       
  lcd.print("System Starting");
  delay(1000);               
  lcd.clear();

previousPidTime = millis();

}

void loop() {
int ldrValue = analogRead(ldrPin);
long potValue = analogRead(potPin);

ldrValue = constrain(ldrValue, 250, 715); //determined by maxmimum and minimum LDR values in the environment
ldrFiltered = (ldrValue * smoothingFactor) + (ldrFiltered * (1.0 - smoothingFactor)); //Digital Low Pass Filter

int currentLight = map((int)ldrFiltered, 250, 715, 0, 100);
int targetLight = map(potValue, 0, 1023, 0, 100);

if (targetLight > 0) { 
    float error = targetLight - currentLight; 

    unsigned long currentTime = millis();
    float dt = (currentTime - previousPidTime) / 1000.0; //calculating the change in time

    if (dt > 0) { //To ensure we don't divide by 0 at startup

   //Integral Math (Accumulate error)
    integral = integral + (error * dt);
    
    //Prevent the integral memory from spiraling out of control
    integral = constrain(integral, -255 / Ki, 255 / Ki); 

    //Derivative Math (Calculate how fast the error is changing)
    float derivative = (error - previousError) / dt;

    //The Final PID Equation
    float pidOutput = (Kp * error) + (Ki * integral) + (Kd * derivative);
    
    //Save the current error and time to use as the "previous" next time
    previousError = error;
    previousPidTime = currentTime;

    currentPWM = constrain((int)pidOutput, 0, 255);
    }

  } 
  else {
    currentPWM = 0;
    integral = 0;
    previousError = 0;
    previousPidTime = millis();
  }

analogWrite(ledPin, currentPWM);

unsigned long currentMillis = millis();

if (currentMillis - previousSerialMillis >= serialInterval) {

    previousSerialMillis = currentMillis;

    Serial.print("Target:"); //Target Light % 
    Serial.print(targetLight);
    Serial.print(',');
    Serial.print("Current:"); //Actual Light % 
    Serial.print(currentLight);
    Serial.print(',');
    Serial.print("currentPWM:");
    Serial.println(currentPWM);

}

if (currentMillis - previousLcdMillis >= lcdInterval) { //LCD display code
    previousLcdMillis = currentMillis;

    lcd.setCursor(0, 0);  
    lcd.print("Target:  ");
    lcd.print(targetLight);
    lcd.print("%   "); 
    
    lcd.setCursor(0, 1); 
    lcd.print("Current: ");
    lcd.print(currentLight);
    lcd.print("%   ");
  }
}
