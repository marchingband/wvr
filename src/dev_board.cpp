#include "Arduino.h"
#include "button.h"
#include "wvr_pins.h"
#include "ws_log.h"

extern "C" void play_sound_ext(int i);
extern "C" void encoder_init(void);
void rgb_init(void);
extern "C" void pot_init(void);

Button b1(D5,FALLING, 50);
Button b2(D6,FALLING, 50);
Button b3(D7,FALLING, 50);
Button s1(D3,FALLING, 50);
Button s2(D4,FALLING, 50);

void logB1(){
    wlog_i("1");
    play_sound_ext(40);
}
void logB2(){
    wlog_i("2");
  play_sound_ext(41);
}
void logB3(){
    wlog_i("3");
}
void logSw1Up(){
    wlog_i("1up");
    digitalWrite(D0,HIGH);
    digitalWrite(D1,HIGH);
    digitalWrite(D2,HIGH);
}
void logSw1Down(){
    wlog_i("1down");
    digitalWrite(D0,LOW);
    digitalWrite(D1,LOW);
    digitalWrite(D2,LOW);
}
void logSw2Up(){
    wlog_i("2up");
}
void logSw2Down(){
    wlog_i("2down");
}

void dev_board_init(){

    button_init();
    encoder_init();
    rgb_init();
    pot_init();
    // gpio_reset_pin(GPIO_NUM_3);
    // gpio_reset_pin(GPIO_NUM_1);

    // pinMode(D0,OUTPUT);
    // pinMode(D1,OUTPUT);
    pinMode(D2,OUTPUT);

    // digitalWrite(D0,LOW);
    // digitalWrite(D1,LOW);
    digitalWrite(D2,LOW);

    pinMode(D3,INPUT_PULLUP);
    pinMode(D4,INPUT_PULLUP);
    pinMode(D5,INPUT_PULLUP);
    pinMode(D6,INPUT_PULLUP);
    pinMode(D7,INPUT_PULLUP);

    // b1.onPress(logB1);
    // b2.onPress(logB2);
    // b3.onPress(logB3);
    // s1.onPress(logSw1Up);
    // s1.onRelease(logSw1Down);
    // s2.onPress(logSw2Up);
    // s2.onRelease(logSw2Down);
}
