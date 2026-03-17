const int thermistorPin = A0;
const int trigPin = 9;
const int echoPin = 10;

const float nominal_resistance = 10000; // 10k ohm
const float nominal_temperature = 25;   // 25 degrees C
const float beta_coefficient = 3950;    // beta value
const float series_resistor = 10000;    // The 10k resistor in the circuit

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  Serial.begin(9600);
}

void loop() {
  int rawValue = analogRead(thermistorPin);

  if (rawValue == 0) {
    rawValue = 1; 
  }
  
  // Convert ADC value to resistance
  float resistance = series_resistor * (1023.0 / rawValue - 1.0);
  
  // Apply Steinhart-Hart Equation
  float steinhart;
  steinhart = resistance / nominal_resistance;     // (R/Ro)
  steinhart = log(steinhart);                        // ln(R/Ro)
  steinhart /= beta_coefficient;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (nominal_temperature + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                       // Invert to get Kelvin
  
  // Convert Kelvin to Celsius
  float tempC = steinhart - 273.15;

  // Speed (m/s) = 331.3 + (0.606 * tempC)
  // Ultrasonic Sensor 
  float speed_of_sound = (331.3 + (0.606 * tempC)) * 0.001;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the bounce back time, maximum of 30ms to ensure it doesn't create an error
  long duration = pulseIn(echoPin, HIGH, 30000);
  
  // Calculate distance: (Time * Speed) / 2 
  float distanceMm = (duration * speed_of_sound) / 2.0;

  Serial.print("Temperature: ");
  Serial.print(tempC);
  Serial.println(" °C");

 if (duration == 0) {
    Serial.println("Distance: Out of range (Path Clear)");
  } else {
    Serial.print("Distance: ");
    Serial.print(distanceMm, 1); 
    Serial.println(" mm");
  }

  delay(1000);
}
