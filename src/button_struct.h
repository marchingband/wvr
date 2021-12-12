// this exists because of circular deps in Button class

#ifndef BUTTON_STRUCT_H
#define BUTTON_STRUCT_H

class Button;

typedef struct {
    Button *button;
    int val;
    int pin;
} button_event_t;

#endif