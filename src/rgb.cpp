#include <Adafruit_NeoPixel.h>
#include "Arduino.h"
#include "wvr_pins.h"

#define NUMPIXELS 1
#define RGB_LED_PIN D2

Adafruit_NeoPixel *rgb;

void rgb_init(uint16_t pin){
    rgb = new Adafruit_NeoPixel(NUMPIXELS, pin, NEO_GRB + NEO_KHZ800);
    rgb->begin();
    rgb->clear();
}

void rgb_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    rgb->setPixelColor(0, rgb->Color(red, green, blue));
    rgb->show();
}