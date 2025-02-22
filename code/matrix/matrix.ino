#include <FastLED.h>
#include <Adafruit_MCP3008.h>
#include <MegunoLink.h>
#include <Filter.h>

// Pin definitions (fixed)
const int LED_DATA_PIN = 8;
const int IR_LED[8] = {6, 12, 9, 4, 18, 2, 19, 3};
const int ADC0_CS_PIN = 10;
const int ADC1_CS_PIN = 11;
const int EDGE_CLK = 7;
const int EDGE_DATA [2] = {19, 18}; // left, right

const int NUM_LEDS = 16;
const int BRIGHTNESS = 255;
CRGB leds[NUM_LEDS];

Adafruit_MCP3008 adc0;
Adafruit_MCP3008 adc1;

const int FILTER_WEIGHT = 60; // 0 -> full smoothing, 100 -> no smoothing
ExponentialFilter<long> * IRVals[16];
int IRThresholds[16];

const float NOISE_THRESHOLD = 1.40;

enum mode {
  toggle = 0,
  distanceColor = 1,
  distanceBrightness = 2,
  distanceColorBrightness = 3,
  heatMap = 4,
  snake = 5,
  paint = 6,
  paintNeg = 7
};
// paint on color light
// paint with off
// game to try to turn off all the lights as they slowly fade

mode MoveMode = paintNeg;

void setup() { 
	Serial.begin(115200);

  for (int i = 0; i < 8; i++) {
    pinMode(IR_LED[i], OUTPUT);
  }
  
	FastLED.addLeds<WS2812B,LED_DATA_PIN,GRB>(leds, NUM_LEDS);
	FastLED.setBrightness(BRIGHTNESS);

  adc0.begin(ADC0_CS_PIN);
  adc1.begin(ADC1_CS_PIN);

  for (int i = 0; i < 16; i++) {
    IRVals[i] = new ExponentialFilter<long>(FILTER_WEIGHT, 0);
  }

  calibrateIRThreshold();
}

unsigned long hueTimer = 0;
unsigned long snakeTimer = 0;

void loop() { 
  static uint8_t hue = 0;

	for(int i = 0; i < NUM_LEDS; i++) {
    // Account for LED sequence
    int index = i;
    if (i % 8 >= 4) {
      index = i > 11 ? 15 - i % 12 : 7 - i % 4;
    }

    if (IRVals[i]->Current() > IRThresholds[i]) {
      if (millis() - hueTimer > 10) {
        hueTimer = millis();
        hue++;
      }

      int value = 255 * (IRVals[i]->Current() - IRThresholds[i]) / (1023 - IRThresholds[i]);

      switch (MoveMode) {
        case toggle:
          leds[index] = CHSV(hue, 255, 255);
          break;
        case distanceColor:
          leds[index] = CHSV(value, 255, 255);
          break;
        case distanceBrightness:
          leds[index] = CHSV(hue, 255, value);
          break;
        case distanceColorBrightness:
          leds[index] = CHSV(value, 255, value);
          break;
        case heatMap: 
          leds[index] = CHSV(90 - min((90 * value / 255) * 1.85, 90), 255, 255);
          break;
        case paintNeg:
          leds[index] = CHSV(hue, 255, 255-value);
          break;
        default:
          leds[index] = CHSV(hue, 255, 255);
          break;
      }
    }

    if (MoveMode == snake){
      leds[(index + 1) % NUM_LEDS] = leds[index];
      delay(5);
    } else if (MoveMode == paintNeg){
      brightenLEDs(hue); 
    }
	}

  if (MoveMode != paint && MoveMode != paintNeg){
    fadeLEDs();
  } 

  FastLED.show();
  
  for (int i = 0; i < 4; i ++) {
    captureIR(i, 0);
    captureIR(i, 1);
  }
}

void fadeLEDs() { 
  for(int i = 0; i < NUM_LEDS; i++) { 
    leds[i].nscale8(254.9); 
  } 
}

void brightenLEDs(uint8_t h){
  for(int i = 0; i < NUM_LEDS; i++) { 
    leds[i].nscale8(257);
  } 
}

void calibrateIRThreshold(){
  setFilterWeight(10);

  for (int i = 0; i < 4; i++) {
    setIR(i, 0);
    setIR(i, 1);   
  }

  for (int j = 0; j < 100; j++){
    for (int i = 0; i < 4; i ++) {
      captureIR(i, 0);
      captureIR(i, 1);
    }
  }

  Serial.println("Calibration Results");
  for (int i = 0; i < NUM_LEDS; i++){
    IRThresholds[i] = IRVals[i]->Current() * NOISE_THRESHOLD;
    Serial.println(i + " " + IRThresholds[i]);
  }

  setFilterWeight(FILTER_WEIGHT);
}

void setFilterWeight(int filterWeight) {
  for (int i = 0; i < 16; i++) {
    IRVals[i]->SetWeight(filterWeight);
  }
}

void setIR(int index, int adc) {
  index *= 2;
  int offset = 2 * adc;

  IRVals[index * 2 + offset]->SetCurrent(readADC(index, adc));
  IRVals[index * 2 + 1 + offset]->SetCurrent(readADC(index + 1, adc));  
}

void captureIR(int index, int adc) {
  index *= 2;
  int offset = 2 * adc;

  digitalWrite(IR_LED[index + adc], HIGH);
  IRVals[index * 2 + offset]->Filter(readADC(index, adc));
  IRVals[index * 2 + 1 + offset]->Filter(readADC(index + 1, adc));
  digitalWrite(IR_LED[index + adc], LOW);
}

int readADC(int index, int adc) {
  return adc == 1 ? adc1.readADC(7 - index) : adc0.readADC(index);
}