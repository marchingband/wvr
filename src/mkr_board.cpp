#include "Arduino.h"
#include "button.h"
#include "wvr_pins.h"
#include "ws_log.h"

extern "C" void play_sound_ext(int i);

Button p1(D13,FALLING, 50);
Button p2(D12,FALLING, 50);
Button p3(D11,FALLING, 50);
Button p4(D10,FALLING, 50);
Button p5(D9,FALLING, 50);
Button p6(D8,FALLING, 50);
Button p7(D7,FALLING, 50);
Button p8(D6,FALLING, 50);
Button p9(D5,FALLING, 50);
Button p10(D4,FALLING, 50);
Button p11(D3,FALLING, 50);
Button p12(D2,FALLING, 50);
Button p13(D1,FALLING, 50);
Button p14(D0,FALLING, 50);

void logPress(){
  play_sound_ext(40);
  wlog_i("*");
}
void logPress2(){
  wlog_i("*");
  play_sound_ext(41);
}

void mkr_init(){

  button_init();

  gpio_reset_pin(GPIO_NUM_1);

  pinMode(D13,INPUT_PULLUP);
  pinMode(D12,INPUT_PULLUP);
  pinMode(D11,INPUT_PULLUP);
  pinMode(D10,INPUT_PULLUP);
  pinMode(D9,INPUT_PULLUP);
  pinMode(D8,INPUT_PULLUP);
  pinMode(D7,INPUT_PULLUP);
  pinMode(D6,INPUT_PULLUP);
  pinMode(D5,INPUT_PULLUP);
  pinMode(D4,INPUT_PULLUP);
  pinMode(D3,INPUT_PULLUP);
  pinMode(D2,INPUT_PULLUP);
  pinMode(D1,INPUT_PULLUP);
  pinMode(D0,INPUT_PULLUP);

  p1.onPress([](void){wlog_i("1");});
  p2.onPress([](void){wlog_i("2");});
  p3.onPress([](void){wlog_i("3");});
  p4.onPress([](void){wlog_i("4");});
  p5.onPress([](void){wlog_i("5");});
  p6.onPress([](void){wlog_i("6");});
  p7.onPress([](void){wlog_i("7");});
  p8.onPress([](void){wlog_i("8");});
  p9.onPress([](void){wlog_i("9");});
  p10.onPress([](void){wlog_i("10");});
  p11.onPress([](void){wlog_i("11");});
  p12.onPress([](void){wlog_i("12");});
  p13.onPress([](void){wlog_i("13");});
  p14.onPress([](void){wlog_i("14");});
}
