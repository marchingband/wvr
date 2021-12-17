#include <wvr_pins.h>
#include <WVR.h>
#include <button.h>
#include <file_system.h>
#include <rgb.h>
#include <pot.h>

WVR wvr;

Button *switchOne;
Button *switchTwo;

void onEncoderDevBoard(bool down)
{
  // static int cnt = 0; // log a running number for the encoder
  // log_i("%d", down ? --cnt : ++cnt);
  uint8_t *channel_lut = get_channel_lut(); // change what voice is used for midi channel 1
  if(down)
  {
    channel_lut[0] -= (channel_lut[0] > 0); // decrement but not below zero
    // log_i("channel %d", channel_lut[0]);
  }
  else
  {
    channel_lut[0] += (channel_lut[0] < 15); // incriment but not above 15
    // log_i("channel %d", channel_lut[0]);
  }
}

void onPotDevBoard(uint32_t raw_val)
{
  static uint8_t val = 0;
  uint8_t temp = raw_val >> 5;
  if(temp != val)
  {
    val = temp;
    wvr.setGlobalVolume(val);
    // log_i("volume %d", val);
  }
}

void switchOneUp(void)
{
  wvr.unmute();
  // log_i("switch one up, unmute");
}

void switchOneDown(void)
{
  wvr.mute();
  // log_i("switch one down, mute");
}

void switchTwoUp(void)
{
  wvr.wifiOn();
  // log_i("switch two up, wifi on");
}

void switchTwoDown(void)
{
  wvr.wifiOff();
  // log_i("switch two down, wifi off");
}

void setup() {
  wvr.useFTDI = false;
  wvr.useUsbMidi = true;
  wvr.begin();
  
  log_i("dev board");

  // connect D13 to RGBLED pin on dev board, make sure pin D13 is set to edge:none in WEB GUI 
  // rgb_init(D13); 
  // rgb_set_color(10,10,10); // very dim white
  
  wvr.resetPin(D9);
  wvr.resetPin(D10);
  wvr.encoderInit(D9, D10);
  pot_init();

  // SWITCH ONE : controls mute on/off
  wvr.resetPin(D3);
  pinMode(D3, INPUT_PULLUP);
  switchOne = new Button(D3, FALLING, 60);
  switchOne->onPress(switchOneUp);
  switchOne->onRelease(switchOneDown);

  // SWITCH TWO : controls wifi on/off
  wvr.resetPin(D4);
  pinMode(D4, INPUT_PULLUP);
  switchTwo = new Button(D4, FALLING, 60);
  switchTwo->onPress(switchTwoUp);
  switchTwo->onRelease(switchTwoDown);

  wvr.wifiIsOn = get_metadata()->wifi_starts_on;
  log_i("wifi is %s", wvr.wifiIsOn ? "on" : "off");

  wvr.onEncoder(onEncoderDevBoard);
  onPot = onPotDevBoard;
}

void loop() {
  // vTaskDelay(portMAX_DELAY);
  vTaskDelete(NULL);
}