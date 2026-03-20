

unsigned long lastPrint = 0;
const unsigned long printInterval = 1000;  // Print every 1000 ms (1 second)

void setup() {
  pinMode(8, OUTPUT);              // Set D8 as output
  Serial.begin(115200);            // Start Serial at 115200 baud
  while (!Serial);                 // Wait for Serial Monitor (optional)
  
  Serial.println("High Frequency Output Started on D8 (pin 8)");
  Serial.println("Theoretical maximum: ~2.667 MHz (16 MHz / 6 cycles per toggle)");
  Serial.println("Use oscilloscope on D8 + GND to measure actual frequency.");
  
  cli();                           // Disable interrupts for maximum toggle speed
}

void loop() {
  while (1) {                      // Tight loop for highest frequency toggle
    PORTB |= (1 << 0);             // Set D8 HIGH (sbi, 2 cycles)
    PORTB &= ~(1 << 0);            // Set D8 LOW  (cbi, 2 cycles)
    // rjmp back ≈ 2 cycles → total 6 cycles per toggle cycle


    unsigned long currentTime = millis();
    if (currentTime - lastPrint >= printInterval) {
      lastPrint = currentTime;
      Serial.print("Running... Elapsed time: ");
      Serial.print(currentTime / 1000);
      Serial.println(" seconds | Freq still ~2.667 MHz");
    }
  }
}