#include <wvr_pins.h>
#include <WVR.h>
#include <button.h>
#include <file_system.h>
#include <pot.h>

WVR wvr;

Button *switchOne;
Button *switchTwo;

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

void switchOneUp(void)
{
  // log_i("switch one up");
  wvr.unmute();
}

void switchOneDown(void)
{
  // log_i("switch one down");
  wvr.mute();
}

void switchTwoUp(void)
{
  // log_i("switch two up");
  wvr.wifiOn();
}

void switchTwoDown(void)
{
  // log_i("switch two down");
  wvr.wifiOff();
}

void setup() {
  wvr.useFTDI = false;
  wvr.useUsbMidi = false;
  wvr.begin();

  wvr.encoderInit(D9, D10);
  pot_init();

  gpio_reset_pin(gpioNumToGpioNum_T(D3));
  gpio_reset_pin(gpioNumToGpioNum_T(D4));

  pinMode(D3, INPUT_PULLUP);
  pinMode(D4, INPUT_PULLUP);

  switchOne = new Button(D3, FALLING, 60);
  switchTwo = new Button(D4, FALLING, 60);

  switchOne->onPress(switchOneUp);
  switchOne->onRelease(switchOneDown);
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