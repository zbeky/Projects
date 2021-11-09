int interval = 30;

void setup() {
  pinMode(PIN_PA6, OUTPUT);
  digitalWrite(PIN_PA6, HIGH);
  pinMode(PIN_PA5, OUTPUT);
  digitalWrite(PIN_PA5, LOW);
  pinMode(PIN_PA4, OUTPUT);
  digitalWrite(PIN_PA4, LOW);
  pinMode(PIN_PA3, OUTPUT);
  digitalWrite(PIN_PA3, LOW);
  pinMode(PIN_PA2, OUTPUT);
  digitalWrite(PIN_PA2, LOW);
  pinMode(PIN_PA1, OUTPUT);
  digitalWrite(PIN_PA1, LOW);
  pinMode(PIN_PA0, OUTPUT);
  digitalWrite(PIN_PA0, LOW);
    
  delay(interval);

  digitalWrite(PIN_PA5, HIGH);
  delay(interval);
  digitalWrite(PIN_PA4, HIGH);
  delay(interval);
  digitalWrite(PIN_PA3, HIGH);
  delay(interval);
  digitalWrite(PIN_PA2, HIGH);
  delay(interval);
  digitalWrite(PIN_PA1, HIGH);
  delay(interval);
  digitalWrite(PIN_PA0, HIGH);
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
