#include <wvr_pins.h>
#include <button_struct.h>
#include <ws_log.h>
#include <wvr_ui.h>
#include <WVR.h>
#include <midiXparser.h>
#include <midi_in.h>
#include <wav_player.h>
#include <button.h>
#include <rpc.h>
#include <file_system.h>
#include <wvr_0.3.h>
#include <gpio.h>
#include "rgb.h"
extern "C" {
  #include "encoder.h"
  #include "pot.h"
}

WVR wvr;

Button *switch_one;
Button *switch_two;

void onEncoderDevBoard(bool down)
{
  // log a running number for the encoder
  // static int cnt = 0;
  // log_i("%d", down ? --cnt : ++cnt);

  // change what voice is used for midi channel 1
  uint8_t *channel_lut = get_channel_lut();
  if(down)
  {
    // decrement but not below zero
    channel_lut[0] -= (channel_lut[0] > 0);
  }
  else
  {
    // incriment but not above 15
    channel_lut[0] += (channel_lut[0] < 15);
  }
}

void onPotDevBoard(uint32_t raw_val)
{
  // log a 7 bit number when the pot moves and set the volume
  static uint8_t val = 0;
  uint8_t temp = raw_val >> 5;
  if(temp != val)
  {
    val = temp;
    // log_i("%d", val);
    wvr.setGlobalVolume(val);
  }
}

void switch_one_up(void)
{
  log_i("switch one up");
  wvr.unmute();
  // rgb_set_color(100 /* red */,100 /* green */,100 /* blue */); // makes white
}

void switch_one_down(void)
{
  log_i("switch one down");
  wvr.mute();
  // rgb_set_color(0 ,0 ,0); // turn off RGB LED
}

void switch_two_up(void)
{
  log_i("switch two up");
  wvr.wifiOn();
}

void switch_two_down(void)
{
  log_i("switch two down");
  wvr.wifiOff();
}

void setup() {
  wvr.useFTDI = true;
  wvr.useUsbMidi = true;
  wvr.begin();
  
  // connect D13 to RGBLED pin on dev board, make sure pin D13 is set to edge:none in WEB GUI 
  // rgb_init(D13); 

  gpio_reset_pin(gpioNumToGpioNum_T(D9));
  gpio_reset_pin(gpioNumToGpioNum_T(D10));

  encoder_init(D9, D10);
  log_i("dev board");
  pot_init();


  gpio_reset_pin(gpioNumToGpioNum_T(D3));
  gpio_reset_pin(gpioNumToGpioNum_T(D4));

  pinMode(D3, INPUT_PULLUP);
  pinMode(D4, INPUT_PULLUP);

  switch_one = new Button(D3, FALLING, 60);
  switch_two = new Button(D4, FALLING, 60);

  switch_one->onPress(switch_one_up);
  switch_one->onRelease(switch_one_down);
  switch_two->onPress(switch_two_up);
  switch_two->onRelease(switch_two_down);

  wvr.wifiIsOn = get_metadata()->wifi_starts_on;
  log_i("wifi is %s", wvr.wifiIsOn ? "on" : "off");

  onEncoder = onEncoderDevBoard;
  onPot = onPotDevBoard;
}

void loop() {
  // vTaskDelay(portMAX_DELAY);
  vTaskDelete(NULL);
}