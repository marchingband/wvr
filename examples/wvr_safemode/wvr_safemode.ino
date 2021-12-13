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

WVR wvr;

void setup() {
  wvr.useFTDI = true;
  wvr.useUsbMidi = false;
  wvr.begin();
  wvr.wifiIsOn = get_metadata()->wifi_starts_on;
  log_i("wifi is %s", wvr.wifiIsOn ? "on" : "off");
}

void loop() {
  // vTaskDelay(portMAX_DELAY);
  vTaskDelete(NULL);
}