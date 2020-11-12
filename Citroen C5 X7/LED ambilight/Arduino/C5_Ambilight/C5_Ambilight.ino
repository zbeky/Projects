#include "src/FastLED/FastLED.h"
#include "src/LowPower/LowPower.h"
#include "src/interval/interval.h"

//Color definition when headlights are on
byte lightsRedComponent = 240;
byte lightsGreenComponent = 20;
byte lightsBlueComponent = 0;
int lightsBrightness = 15;
bool headlights = false;

//Color definition when doors are opened
byte doorsRedComponent = 255;
byte doorsGreenComponent = 210;
byte doorsBlueComponent = 90;
int doorsBrightness = 0;

//LED strip setup
#define LED_PIN     6         //data pin for the led strip
#define NUM_LEDS    26        //number of LED chips
#define BRIGHTNESS  100       //default brightness
#define LED_TYPE    WS2812B   //used type of LED
#define COLOR_ORDER GRB       //color order
CRGB leds[NUM_LEDS];

//Input/output setup
#define doorsPinD   2           //pin detekce otevřených dveří
#define doorsPinA  A0           //Analogový pin detekce otevřených dveří
#define lightsPin   3           //pin detekce rozsvícených potkávaček
#define LEDPin      12           //pin ovládání napájení pásku

//Misc variables
Interval allowedTime;         //časovač uspání
const long wakeupLimit = 30000;     //doba do uspání v ms 
byte count = 0;

void setup() {

  Serial.begin(57600);
  pinMode(doorsPinD, INPUT);
  pinMode(doorsPinA, INPUT);
  pinMode(lightsPin, INPUT);
  pinMode(LEDPin, OUTPUT);
  digitalWrite(LEDPin, HIGH);
  delay(2000); // power-up safety delay
  digitalWrite(LEDPin, LOW);
  delay(500);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setDither(0); //Prevent flikering
  FastLED.setBrightness(BRIGHTNESS);
  dark();
  FastLED.show();

}

void loop() {

  checkState();

  if (doorsBrightness > 0) {
    doorsAreOpened();
    allowedTime.set(wakeupLimit);
  } else if (headlights) { 
    headlightsAreOn();
    allowedTime.set(wakeupLimit);
  } else {
    dark();
  }

  Serial.println(doorsBrightness);

  FastLED.show();

  if (allowedTime.expired()) {
    sleep();
  }
  
}

void sleep() {
	
  digitalWrite(LEDPin, HIGH);
  
  attachInterrupt(digitalPinToInterrupt(doorsPinD), wake, CHANGE);
  attachInterrupt(digitalPinToInterrupt(lightsPin), wake, CHANGE);

  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
  
  detachInterrupt(digitalPinToInterrupt(doorsPinD));
  detachInterrupt(digitalPinToInterrupt(lightsPin));

  digitalWrite(LEDPin, LOW);
  delay(50);
  
}

void wake() {
  //help function
}

void headlightsAreOn() {
	
  FastLED.setBrightness(lightsBrightness);
  FastLED.setTemperature(Tungsten100W);
  
  for(int i = 0 ; i < NUM_LEDS; i++ ) {
      leds[i].setRGB(lightsRedComponent, lightsGreenComponent, lightsBlueComponent);
  }
  
}

void doorsAreOpened() {
	
  FastLED.setBrightness(doorsBrightness);
  FastLED.setTemperature(Tungsten40W);
  
  for(int i = 0 ; i < NUM_LEDS; i++ ) {
      leds[i].setRGB(doorsRedComponent, doorsGreenComponent, doorsBlueComponent);
  }
  
}

void dark() {
	
  for(int i = 0 ; i < NUM_LEDS; i++ ) {
      leds[i] = CRGB::Black;
  }
  
}

void checkState() {

  doorsBrightness = map(analogRead(doorsPinA), 200, 930, 0, 100);
  if (doorsBrightness<0) doorsBrightness=0;
  if (doorsBrightness>100) doorsBrightness=100;

  headlights = digitalRead(lightsPin);
  
}

