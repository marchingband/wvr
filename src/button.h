#ifndef BUTTON_H
#define BUTTON_H

#include "button_struct.h"

class Button {
    public:
        int pin;
        int dbnc;
        int last;
        int mode;
        bool touch;
        bool pressed;
        void (*handlePress)();
        void (*handleRelease)();
        button_event_t event;
        Button(int pin, int mode, int dbnc);
        Button(int pin, int mode, int dbnc, bool touch);
        ~Button();
        void onPress(void(*handlePress)());
        void onRelease(void(*handlePress)());
        void handleChange(int val);
};

void button_init(void);
void init_touch(void);
void init_touch_pad(int pin, void *event);

#endif