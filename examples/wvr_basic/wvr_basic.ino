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
    wvr.serverPause();
  }
  else
  {
    wvr.serverResume();
  }
  serverOn = !serverOn;
}

void setup() {
  // put your setup code here, to run once:
  wvr.begin();
  pinMode(D2, INPUT_PULLUP);
  btn1.onPress(on1);
}

void loop() {
  // put your main code here, to run repeatedly:
  vTaskDelay(portMAX_DELAY);
}