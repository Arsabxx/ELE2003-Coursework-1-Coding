unsigned long lastPrint = 0;
const unsigned long interval = 1000;  // Print interval: 1000 ms = 1 second

void setup() {
  pinMode(8, OUTPUT);          // Set D8 (PB0) as output
  Serial.begin(115200);        // Start Serial Monitor at 115200 baud
  cli();                       // Disable interrupts to avoid timing jitter
}

void loop() {
  while (1) {                  // Tight loop for maximum toggle speed
    PORTB |= (1 << 0);         // Set D8 HIGH (2 clock cycles)
    PORTB &= ~(1 << 0);        // Set D8 LOW  (2 clock cycles)
    // + rjmp overhead ≈ 2 cycles → total 6 cycles → 16 MHz / 6 = 2.667 MHz

    // Non-blocking check: print frequency every second
    unsigned long now = millis();
    if (now - lastPrint >= interval) {
      lastPrint = now;
      Serial.println("2.667");   // Theoretical frequency in MHz
      // Change to your measured value, e.g. Serial.println("2.58");
    }
  }
}