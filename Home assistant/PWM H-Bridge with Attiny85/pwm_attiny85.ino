byte dutyCycleA = 0;
byte dutyCycleB = 0;

#define PB0 0 //OUT1
#define PB1 1 //OUT2
#define PB2 1 //IN1
#define PB4 2 //IN2

void setup() {
  
  pinMode(PB0, OUTPUT);    // Timer 0 "B" output: OC2B
  pinMode(PB1, OUTPUT);    // Timer 0 "A" output
  pinMode(3, INPUT);
  pinMode(4, INPUT);

  // Set OC0A on Compare Match when up-counting.
  // Clear OC0B on Compare Match when up-counting.
  TCCR0A = bit (WGM00) | bit (COM0B1) | bit (COM0A1) | bit (COM0A0);       
  TCCR0B = bit (CS01) | bit (CS00);         // phase correct PWM, prescaler of 64
  OCR0A = dutyCycleA;           // duty cycle out of 255 
  OCR0B = 255 - dutyCycleB;     // duty cycle out of 255
  
} 

void loop() {

  dutyCycleA = analogRead(PB2)*0.185; //10bit analog read to 8bit duty (1024->256 = /8), 3V3->5V compensation *1,5
  if (dutyCycleA < 6) {
	dutyCycleA = 0;
  }	
  dutyCycleB = analogRead(PB4)*0.185; //10bit analog read to 8bit duty (1024->256 = /8), 3V3->5V compensation *1,5
  if (dutyCycleB < 6) {
	dutyCycleB = 0;
  }	
  OCR0B = 255 - dutyCycleB;
  OCR0A = dutyCycleA;

}
