#include <wvr_pins.h>
#include <button_struct.h>
#include <ws_log.h>
#include <WVR.h>
#include <midiXparser.h>
#include <midi_in.h>
#include <wav_player.h>
#include <button.h>
#include <rpc.h>
#include <file_system.h>
#include <wvr_0.3.h>
#include <gpio.h>
#include "midi_in.h"

WVR wvr;

void midiHookMS20(uint8_t *msg){
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  if(code == MIDI_CC)
  {
    uint8_t CC = msg[1] & 0b01111111;
    uint8_t val = msg[2] & 0b01111111;
    if(CC == 77)
    {
      int selected_voice = (val >= 0 && val < 21) ? 0 :
        (val >= 21 && val < 64) ? 1 :
        (val >= 64 && val < 106) ? 2 : 3;
      wvr.setVoice(0, selected_voice);
    }
  }
}

void setup() {
  wvr.useFTDI = false;
  wvr.useUsbMidi = true;
  wvr.begin();
  wvr.setMidiHook(midiHookMS20); // setMidiHook must run after wvr.begin()
  wvr.wifiIsOn = get_metadata()->wifi_starts_on;
  log_i("wifi is %s", wvr.wifiIsOn ? "on" : "off");
}

void loop() {
  // vTaskDelay(portMAX_DELAY);
  vTaskDelete(NULL);
}