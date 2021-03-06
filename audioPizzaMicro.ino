#include <Wire.h>
//#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_MPR121.h"
#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <RH_RF69.h>

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// COMMUNICATION SETUP
const int comMode = 0; // 0 is serial, 1 is MIDI over USB
// the MIDI channel number to send messages
//const int midiChannel = 1;

// RF
#define RF69_FREQ 915.0
#define RFM69_CS      8 // Feather M0 w/Radio
#define RFM69_INT     3
#define RFM69_RST     4
#define LED           13

RH_RF69 rf69(RFM69_CS, RFM69_INT);

// LED SETUP
const int ledsPerStrip = 60;
const int ledStripOffset = 1;


Adafruit_DotStar leds(ledsPerStrip, DOTSTAR_BRG);


// TOUCH SENSOR SETUP
Adafruit_MPR121 cap1 = Adafruit_MPR121();
Adafruit_MPR121 cap2 = Adafruit_MPR121();

// Keeps track of the last pins touched
// so we know when buttons are 'released'
uint16_t lasttouched1 = 0;
uint16_t lasttouched2 = 0;
uint16_t currtouched1 = 0;
uint16_t currtouched2 = 0;


// ACCELEROMETER SETUP

//Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// Adjust this number for the sensitivity of the 'click' force
// this strongly depend on the range! for 16G, try 5-10
// for 8G, try 10-20. for 4G try 20-40. for 2G try 40-80
//#define CLICKTHRESHHOLD 40
//bool tapThisFrame = false;


// SENSOR TO LIGHT MATRIX
bool touchSensorStates[24] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//const int sensorFromSpotIndex[12] = {0, 3, 7, 8, 9, 10, 1, 11, 6, 5, 4, 2};
//const int sensorToOutputIndex[12] = {0, 3, 7, 8, 9, 10, 1, 11, 6, 5, 4, 2};

// WORLD SETUP
const int numSpots = 24;
const int spotWidth = 1;
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
//float offsetBySpot[12] = {0, 1, 0, 1, 1, 0, -2, -3, -2, -2, -3, -1};
float offsetBySpot[numSpots] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void setup() {
  Serial.begin(57600);

  //  while (!Serial) {
  //    delay(1);  // wait until serial console is open, remove if not tethered to computer
  //  }

  Serial.println();
  Serial.println();
  delay(4000);

  Serial.println("Fantasmophone apm begins");

  // RF Communication setup
  randomSeed(analogRead(0));

  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  // manual reset
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);

  if (!rf69.init()) {
    Serial.println("RFM69 radio init failed");
    while (1);
  }
  Serial.println("RFM69 radio init OK!");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  }

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(20, true);  // range from 14-20 for power, 2nd arg must be true for 69HCW

  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
                  };
  rf69.setEncryptionKey(key);


  // Setup touch sensor
  // Default address is 0x5A, if tied to 3.3V its 0x5B
  // If tied to SDA its 0x5C and if SCL then 0x5D
  if (!cap1.begin(0x5A)) {
    Serial.println("MPR121 1 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 1 found!");

  if (!cap2.begin(0x5B)) {
    Serial.println("MPR121 2 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 2 found!");


  // Setup accelerometer
  //  if (! lis.begin(0x18)) {   // change this to 0x19 for alternative i2c address
  //    Serial.println("Couldnt start accelerometer");
  //    while (1) yield();
  //  }
  //  Serial.println("LIS3DH found!");

  //  lis.setRange(LIS3DH_RANGE_2_G);   // 2, 4, 8 or 16 G!

  //  Serial.print("Range = "); Serial.print(2 << lis.getRange());
  //  Serial.println("G");

  // 0 = turn off click detection & interrupt
  // 1 = single click only interrupt output
  // 2 = double click only interrupt output, detect single click
  // Adjust threshhold, higher numbers are less sensitive
  //  lis.setClick(2, CLICKTHRESHHOLD);

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
  char message[5] = "";

  // Get the currently touched pads
  currtouched1 = cap1.touched();

  //  int noteOffset = 40;

  for (uint8_t i = 0; i < 12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched1 & _BV(i)) && !(lasttouched1 & _BV(i)) ) {

      sprintf(message, "S1%x", i);
      Serial.println(message);
      
      //      rf69.send((uint8_t *)radiopacket, strlen(radiopacket));
      //      rf69.waitPacketSent();

      touchSensorStates[i] = true;
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched1 & _BV(i)) && (lasttouched1 & _BV(i)) ) {
      sprintf(message, "S0%x", i);
      Serial.println(message);
      
      //      rf69.send((uint8_t *)radiopacket, strlen(radiopacket));
      //      rf69.waitPacketSent();

      touchSensorStates[i] = false;
    }


    currtouched2 = cap2.touched();
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched2 & _BV(i)) && !(lasttouched2 & _BV(i)) ) {


      sprintf(message, "S1%x", i + 12);
      Serial.println(message);

      //      rf69.send((uint8_t *)radiopacket, strlen(radiopacket));
      //      rf69.waitPacketSent();

      touchSensorStates[i + 12] = true;
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched2 & _BV(i)) && (lasttouched2 & _BV(i)) ) {
      //      Serial.print("ST"); Serial.print(sensorToOutputIndex[i] + 12, HEX); Serial.print(0); Serial.println();

      sprintf(message, "S0%x", i + 12);
      Serial.println(message);

      //      rf69.send((uint8_t *)radiopacket, strlen(radiopacket));
      //      rf69.waitPacketSent();

      touchSensorStates[i + 12] = false;
    }

  }


  // reset our state
  lasttouched1 = currtouched1;
  lasttouched2 = currtouched2;


  // Radio send sensor states
//  sprintf(message, "S1%x", i + 12);

  // Detect taps on accel
  //  tapThisFrame = false;
  //  uint8_t click = lis.getClick();
  //  if (!(click == 0))
  //  {
  //    if (click & 0x30)
  //    {
  //      if (click & 0x10) Serial.print("SAT");
  //      if (click & 0x20) Serial.print("SAD");
  //      tapThisFrame = true;
  //      Serial.println();
  //    }
  //  }


  //  for (int i = 0; i < ledsPerStrip * 8; i++) {
  //    leds.setPixelColor(i, 0, 0, 0);
  //  }
  for (int i = 0; i < numSpots; i++) {

    float center = spots[i].location * ledsPerStrip + ledStripOffset + offsetBySpot[i];
    int ledLocation = round(center);

    int colorOffset = 0;
    float luminance;

    float luminanceMult = 10.0;
    if (touchSensorStates[i]) {
      luminanceMult = 120.0;
    }
    //    else if (tapThisFrame) {
    //      luminanceMult = 40.0;
    //    }

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

  delay(20);
}
