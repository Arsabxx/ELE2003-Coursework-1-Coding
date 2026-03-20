void setup() {
  pinMode(8, OUTPUT);
  cli();
}

void loop() {
  while (1) {
    PORTB |= (1 << 0);
    PORTB &= ~(1 << 0);
    }
  }
}