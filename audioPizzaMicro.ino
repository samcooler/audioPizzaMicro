#include <OctoWS2811.h>
#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_MPR121.h"
#include <Adafruit_NeoPixel.h>

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// LED SETUP
const int ledsPerStrip = 60;
const int ledStripOffset = 1;

#ifdef __AVR__
#include <avr/power.h>
#endif

#define LEDPIN 17
Adafruit_NeoPixel leds = Adafruit_NeoPixel(60, LEDPIN, NEO_GRB + NEO_KHZ800);

// TOUCH SENSOR SETUP
Adafruit_MPR121 cap = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched = 0;
uint16_t currtouched = 0;


// ACCELEROMETER SETUP

Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// Adjust this number for the sensitivity of the 'click' force
// this strongly depend on the range! for 16G, try 5-10
// for 8G, try 10-20. for 4G try 20-40. for 2G try 40-80
#define CLICKTHRESHHOLD 80
bool tapThisFrame = false;


// SENSOR TO LIGHT MATRIX
bool touchSensorStates[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const int sensorFromSpotIndex[12] = {0, 3, 7, 8, 9, 10, 1, 11, 6, 5, 4, 2};

// WORLD SETUP
const int numSpots = 12;
const int spotWidth = 1.5;
const float boundary = 0.01;

class Spot
{
  public:
    Spot();
    int color;
    float location;
    float velocity;
    float acceleration;
    float jerk;
    void move();

};

Spot::Spot()
{
  color = 0;
  location = 0.0;
  velocity = 0.0;
  acceleration = 0.0;
  jerk = 0.0;
}

void Spot::move()
{
  acceleration += jerk;
  velocity += acceleration;
  location += velocity;
  //  while(location > 1.0) location -= 1.0;
  //  while(location < 0) location += 1.0;
}


Spot spots[numSpots];
int colors[6] = {0, 60, 100, 240, 300, 340};
float offsetBySpot[12] = {0, 1, 0, 1, 1, 0, -2, -3, -2, -2, -3, -1};

void setup() {
  Serial.begin(57600);
  pinMode(13, OUTPUT);
  randomSeed(analogRead(0));


  // Setup touch sensor
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 (touch sensor) not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");


  // Setup accelerometer
  if (! lis.begin(0x18)) {   // change this to 0x19 for alternative i2c address
    Serial.println("Couldnt start accelerometer");
    while (1) yield();
  }
  Serial.println("LIS3DH found!");

  lis.setRange(LIS3DH_RANGE_2_G);   // 2, 4, 8 or 16 G!

  Serial.print("Range = "); Serial.print(2 << lis.getRange());
  Serial.println("G");

  // 0 = turn off click detection & interrupt
  // 1 = single click only interrupt output
  // 2 = double click only interrupt output, detect single click
  // Adjust threshhold, higher numbers are less sensitive
  lis.setClick(2, CLICKTHRESHHOLD);

  // Setup display spots
  int centerColor = random(360);
  for (int i = 0; i < numSpots; i++) {
    spots[i].location = float(i) / numSpots;
    spots[i].velocity = 0.0;
    spots[i].acceleration = 0.0;
    //    spots[i].color = (centerColor + colors[i % 4] + (2 * random(2) - 1) * random(20)) % 360;
    spots[i].color = (centerColor + colors[i % 6] + (2 * random(2) - 1) * random(20)) % 360;
    //    spots[i].color = random(360);
  }


  leds.begin();
  //  for (int i = 0; i < ledsPerStrip * 8; i++) {
  //    leds.setPixelColor(i, makeColor(centerColor, 100, 50));
  //  }
  //  leds.setBrightness(50);
  //  leds.show();
  //  delay(1000);
  for (int i = 0; i < ledsPerStrip * 8; i++) {
    leds.setPixelColor(i, 0, 0, 0);
  }
  leds.show();
}

void loop() {

  // Get the currently touched pads
  currtouched = cap.touched();

  for (uint8_t i = 0; i < 12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      Serial.print("ST"); Serial.print(i, HEX); Serial.print(1); Serial.println();
      touchSensorStates[i] = true;
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      Serial.print("ST"); Serial.print(i, HEX); Serial.print(0); Serial.println();
      touchSensorStates[i] = false;
    }
  }

  // reset our state
  lasttouched = currtouched;

  // Detect taps on accel
  tapThisFrame = false;
  uint8_t click = lis.getClick();
  if (!(click == 0))
  {
    if (click & 0x30)
    {
      if (click & 0x10) Serial.print("SA0");
      if (click & 0x20) Serial.print("SA1");
      tapThisFrame = true;
      Serial.println();
    }
  }


  //  for (int i = 0; i < ledsPerStrip * 8; i++) {
  //    leds.setPixelColor(i, 0, 0, 0);
  //  }
  for (int i = 0; i < numSpots; i++) {

    float center = spots[i].location * ledsPerStrip + ledStripOffset + offsetBySpot[i];
    int ledLocation = round(center);

    int colorOffset = 0;
    float luminance;

    float luminanceMult = 50.0;
    if (touchSensorStates[sensorFromSpotIndex[i]]) {
      luminanceMult = 120.0;
    }
    if (tapThisFrame) {
      luminanceMult = 30.0;
    }
    // draw across width of spot
    for (int offset = -1 * spotWidth; offset <= spotWidth; offset++) {
      int led = ledLocation + offset;
      if (led >= 0 && led <= ledsPerStrip) {

        luminance = luminanceMult * min(1.3 - abs(center - led) / spotWidth, 1.0);
        colorOffset = 0;//10 * (spotWidth - abs(offset));

        leds.setPixelColor(led, makeColor((spots[i].color + colorOffset) % 360, 100, luminance));
      }
    }
  }
  leds.show();
  delay(10);
}
