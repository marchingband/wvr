#include "wvr_pins.h"
#include "driver/touch_pad.h"

touch_pad_t gpioNumToTPNum(int gpio)
{
    switch(gpio){
        case T0:
            return TOUCH_PAD_NUM1;
        case T1:
            return TOUCH_PAD_NUM9;
        case T2:
            return TOUCH_PAD_NUM8;
        case T3:
            return TOUCH_PAD_NUM7;
        // default: 
        //     return -1;
    }
}

int gpioNumToPinNum(int gpio)
{
    switch(gpio){
        case D0:
            return 0;
        case D1:
            return 1;
        case D2:
            return 2;
        case D3:
            return 3;
        case D4:
            return 4;
        case D5:
            return 5;
        case D6:
            return 6;
        case D7:
            return 7;
        case D8:
            return 8;
        case D9:
            return 9;
        case D10:
            return 10;
        case D11:
            return 11;
        case D12:
            return 12;
        case D13:
            return 13;
        default:
            return -1;
    }
}

gpio_num_t gpioNumToGpioNum_T(int gpio)
{
    switch(gpio){
        case D0:
            return D0_GPIO;
        case D1:
            return D1_GPIO;
        case D2:
            return D2_GPIO;
        case D3:
            return D3_GPIO;
        case D4:
            return D4_GPIO;
        case D5:
            return D5_GPIO;
        case D6:
            return D6_GPIO;
        case D7:
            return D7_GPIO;
        case D8:
            return D8_GPIO;
        case D9:
            return D9_GPIO;
        case D10:
            return D10_GPIO;
        case D11:
            return D11_GPIO;
        case D12:
            return D12_GPIO;
        case D13:
            return D13_GPIO;
        // default:
        //     return -1;
    }
}
