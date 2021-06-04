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

WVR wvr;
Button btn1(D2, FALLING, 300);

bool serverOn = true;

void on1(void)
{
  // wvr.play(0,40,127);
  if(serverOn)
  {
    log_i("server pause");
    wvr.serverPause();
    log_i("wifi on %d", get_metadata()->wifi_starts_on);
  }
  else
  {
    log_i("server resume");
    wvr.serverResume();
    log_i("wifi on %d", get_metadata()->wifi_starts_on);
  }
  serverOn = !serverOn;
}

void setup() {
  // put your setup code here, to run once:
  wvr.begin();
  serverOn = get_metadata()->wifi_starts_on;
  log_i("wifi on %d", serverOn);
  pinMode(D2, INPUT_PULLUP);
  btn1.onPress(on1);
}

void loop() {
  // put your main code here, to run repeatedly:
  vTaskDelay(portMAX_DELAY);
}