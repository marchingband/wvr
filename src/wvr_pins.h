#ifndef WVR_PINS_H
#define WVR_PINS_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "driver/touch_pad.h"

#define D0_GPIO GPIO_NUM_3
#define D1_GPIO GPIO_NUM_1
#define D2_GPIO GPIO_NUM_21
#define D3_GPIO GPIO_NUM_19
#define D4_GPIO GPIO_NUM_18
#define D5_GPIO GPIO_NUM_5
#define D6_GPIO GPIO_NUM_0
#define D7_GPIO GPIO_NUM_36 // no pullup
#define D8_GPIO GPIO_NUM_39 // no pullup
#define D9_GPIO GPIO_NUM_34 // no pullup
#define D10_GPIO GPIO_NUM_35 // no pullup
#define D11_GPIO GPIO_NUM_32
#define D12_GPIO GPIO_NUM_33
#define D13_GPIO GPIO_NUM_27

static const gpio_num_t gpio_pins[14] = {
    D0_GPIO,
    D1_GPIO,
    D2_GPIO,
    D3_GPIO,
    D4_GPIO,
    D5_GPIO,
    D6_GPIO,
    D7_GPIO, // no pullup
    D8_GPIO, // no pullup
    D9_GPIO, // no pullup
    D10_GPIO, // no pullup
    D11_GPIO,
    D12_GPIO,
    D13_GPIO
};

#define D0 3
#define D1 1
#define D2 21
#define D3 19
#define D4 18
#define D5 5
#define D6 0
#define D7 36 // no pullup
#define D8 39 // no pullup
#define D9 34 // no pullup
#define D10 35 // no pullup
#define D11 32
#define D12 33
#define D13 27

static const uint8_t wvr_pins[14] = {
    D0,
    D1,
    D2,
    D3,
    D4,
    D5,
    D6,
    D7, // no pullup
    D8, // no pullup
    D9, // no pullup
    D10, // no pullup
    D11,
    D12,
    D13
};


#define A0 D6
#define A1 D7
#define A2 D8
#define A3 D9
#define A4 D10
#define A5 D11
#define A6 D12
#define A7 D13

#define T0 D6
#define T1 D11
#define T2 D12
#define T3 D13

#define PAD1 D13
#define PAD2 D12
#define PAD3 D11
#define PAD4 D10
#define PAD5 D9
#define PAD6 D8
#define PAD7 D7
#define PAD8 D6
#define PAD9 D5
#define PAD10 D4
#define PAD11 D3
#define PAD12 D2
#define PAD13 D1
#define PAD14 D0

int gpioNumToPinNum(int gpio);
touch_pad_t gpioNumToTPNum(int gpio);
gpio_num_t gpioNumToGpioNum_T(int gpio);

#ifdef __cplusplus
}
#endif
#endif