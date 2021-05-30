#ifndef BUTTON_H
#define BUTTON_H

#include "button_struct.h"

class Button {
    public:
        int pin;
        int dbnc;
        int last;
        int mode;
        void (*handlePress)();
        void (*handleRelease)();
        button_event_t event;
        Button(int pin, int mode, int dbnc);
        ~Button();
        void onPress(void(*handlePress)());
        void onRelease(void(*handlePress)());
        void handleChange(int val);
};

void button_init();

#endif