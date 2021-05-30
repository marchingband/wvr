#include <Adafruit_NeoPixel.h>
#include "Arduino.h"

#define NUMPIXELS 1
#define PIN 21

void rgb_init(){
    Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
    pixels.begin();
    pixels.clear();
    pixels.setPixelColor(0, pixels.Color(0, 150, 0));
    pixels.show();
    delay(1000);
    pixels.setPixelColor(0, pixels.Color(150, 0, 0));
    pixels.show();
    delay(1000);
    pixels.setPixelColor(0, pixels.Color(0, 0, 150));
    pixels.show();
    delay(1000);
}