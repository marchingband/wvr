#ifndef GPIO_H
#define GPIO_H

#include "button.h"

extern Button *buttons[14];

void wvr_gpio_init(bool useFTDI, bool useUsbMidi);
void wvr_gpio_start(void);
void on_press(uint8_t i);
void on_release(uint8_t pin);

#endif