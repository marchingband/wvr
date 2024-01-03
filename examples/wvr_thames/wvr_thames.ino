#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <wvr_pins.h>
#include <button_struct.h>
#include <ws_log.h>
#include <wvr.h>
#include <midiXparser.h>
#include <midi_in.h>
#include <wav_player.h>
#include <button.h>
#include <rpc.h>
#include <file_system.h>
#include <wvr_0.3.h>
#include <gpio.h>

#define KICK 40
#define SNARE 41
#define HI_TOM 42
#define LOW_TOM 43
#define RIDE 44
#define BELL 45
#define CRASH 46
#define HIHAT_CLOSED 47
#define HIHAT_OPEN 48
#define HIHAT_PEDAL 49
#define SNARE_CROSS 49

#define LED1 D3
#define LED2 D11
#define LED3 D1
#define LED4 D0

#define LED_FREQ 5000
#define ENABLE_FTDI false
// #define ENABLE_FTDI true

void setButtonFuncs(void);
void setMidiFilter(void);

WVR wvr;

Button *StompA;
Button *StompB;
Button *EncButton;

bool encPushed = false;
bool hold = false;
bool held[127] = {false};
bool damp = false;
bool hihatOpen = false;

void gpioInit(void){
  wvr.resetPin(D4);
  wvr.resetPin(D5);
  wvr.resetPin(D6);
  wvr.resetPin(D12);
  wvr.resetPin(D13);

  if(ENABLE_FTDI == false){
    wvr.resetPin(LED1);
    wvr.resetPin(LED3);
    pinMode(LED1, OUTPUT);
    pinMode(LED3, OUTPUT);
    ledcSetup(0, LED_FREQ, 8);
    ledcSetup(2, LED_FREQ, 8);
    ledcAttachPin(LED1, 0);
    ledcAttachPin(LED3, 2);
  }

  wvr.resetPin(LED2);
  wvr.resetPin(LED4);

  pinMode(D4, INPUT_PULLUP);
  pinMode(D12, INPUT_PULLUP);
  pinMode(D13, INPUT_PULLUP);

  pinMode(LED2, OUTPUT);
  pinMode(LED4, OUTPUT);

  ledcSetup(1, LED_FREQ, 8);
  ledcSetup(3, LED_FREQ, 8);
  ledcAttachPin(LED2, 1);
  ledcAttachPin(LED4, 3);
}

void writeBinaryToLEDs(uint8_t num){
  ledcWrite(0, (num & (0x01 << 0)) ? 255 : 0);
  ledcWrite(1, (num & (0x01 << 1)) ? 255 : 0);
  ledcWrite(2, (num & (0x01 << 2)) ? 255 : 0);
  ledcWrite(3, (num & (0x01 << 3)) ? 255 : 0);
}

void writeVolumeToLEDs(void){
  int volume = wvr.getGlobalVolume();
  int led_1 = (volume > 32) ? 255 : (volume * 8);
  int led_2 = (volume > 64) ? 255 : (volume < 32) ? 0 : ((volume - 32) * 8);
  int led_3 = (volume > 96) ? 255 : (volume < 64) ? 0 : ((volume - 64) * 8);
  int led_4 = (volume < 96) ? 0   : (volume - 96) * 8;
  ledcWrite(0, led_1);
  ledcWrite(1, led_2);
  ledcWrite(2, led_3);
  ledcWrite(3, led_4);
}

void onEncoderThames(bool down){
  int volume = wvr.getGlobalVolume();
  if(down){
    if(encPushed){
      int voice = wvr.getVoice(0);
      voice -= (voice > 0);
      wvr.setVoice(0, voice);
      writeBinaryToLEDs(voice);
      setButtonFuncs();
      setMidiFilter();
    } else {
      volume -= (volume > 0);
      wvr.setGlobalVolume(volume);
      writeVolumeToLEDs();
    }
  } else {
    if(encPushed){
      int voice = wvr.getVoice(0);
      voice += (voice < 15);
      wvr.setVoice(0, voice);
      writeBinaryToLEDs(voice);
    } else {
      volume += (volume < 126);
      wvr.setGlobalVolume(volume);
      writeVolumeToLEDs();
    }
  }
  setButtonFuncs();
  setMidiFilter();
}

void handleReleaseStompADrums(void){
  hihatOpen = true;
}

void handlePressStompADrums(void){
  wvr.stop(1, HIHAT_OPEN);
  wvr.play(1, HIHAT_PEDAL, 127);
  hihatOpen = false;
}

void handlePressStompBDrums(void){
  wvr.play(1, KICK, 80);
}

void handleReleaseStompAKeyboard(void){
  damp = false;
}

void handlePressStompAKeyboard(void){
  damp = true;
}

void handleReleaseStompBKeyboard(void){
  for(int i=0;i<127;i++){
    if(held[i]){
      wvr.stop(0, i);
      held[i] = false;
    }
  }
  hold = false;
}

void handlePressStompBKeyboard(void){
  hold = true;
}

void encButtonUp(void){
  encPushed = false;
  writeVolumeToLEDs();
}

void encButtonDown(void){
  encPushed = true;
  writeBinaryToLEDs(wvr.getVoice(0));
}

void shells(uint8_t * msg){ 
  // this is a custom hook for my own personal electric drum kit
  if (!msg) return; // guard against NULL notes
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  if(code == MIDI_NOTE_ON){
    uint8_t note = msg[1] & 0b01111111;
    note = 
      note == 45 ? SNARE :
      note == 40 ? RIDE :
      note == 42 ? HIHAT_CLOSED :
      note == 43 ? HI_TOM :
      note == 44 ? LOW_TOM :
      note == 47 ? CRASH :
      note;
    msg[1] = note & 0b01111111;
  }
}

void sustain(uint8_t * msg){
  if (!msg) return;
  uint8_t channel = msg[0] & 0b00001111;
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  if((code == MIDI_NOTE_OFF) && hold){
    uint8_t note = msg[1] & 0b01111111;
    // remember that its being held
    held[note] = true;
    *msg = NULL; // erase the message
  }else if((code == MIDI_NOTE_ON) && hold){
    // remove potential note off event from hold
    uint8_t note = msg[1] & 0b01111111;
    held[note] = false;
  }
}

void soft(uint8_t * msg){
  // pass on NULL pointer immidiately
  if (!msg) return;
  uint8_t channel = msg[0] & 0b00001111;
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  if((code == MIDI_NOTE_ON) && damp){
    uint8_t velocity = msg[2] & 0b01111111;
    velocity *= 0.4;
    msg[2] = velocity & 0b01111111; 
  }
}

void midiHookKeyboard(uint8_t * msg){
  sustain(msg);
  soft(msg);
}

void midiHookDrums(uint8_t * msg){
  if (!msg) return; // guard against NULL notes
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  // ignore all note-offs
  if(code == MIDI_NOTE_OFF){
    uint8_t note = msg[1] & 0b01111111;
    if(note == HIHAT_OPEN){
      *msg = NULL; // erase the message
    }
  }else if(code == MIDI_NOTE_ON){
    // msg = shells(msg); // this is a custom hook for my own personal electric drum kit
    uint8_t note = msg[1] & 0b01111111;
    // hihat
    if(note == HIHAT_CLOSED && hihatOpen){
      note = HIHAT_OPEN;
    }
    msg[1] = note & 0b01111111;
  }
}

void midiHookNone(uint8_t * msg){
  return;
}

void setButtonFuncs(){
  switch (wvr.getVoice(0)){
    case 0:
      StompA->onPress(handlePressStompAKeyboard);
      StompA->onRelease(handleReleaseStompAKeyboard);
      StompB->onPress(handlePressStompBKeyboard);
      StompB->onRelease(handleReleaseStompBKeyboard);
      break;
    case 1:
      StompA->onPress(handlePressStompADrums);
      StompA->onRelease(handleReleaseStompADrums);
      StompB->onPress(handlePressStompBDrums);
      StompB->onRelease(NULL);
      break; 
    default:
      StompA->onPress(NULL);
      StompA->onRelease(NULL);
      StompB->onPress(NULL);
      StompB->onRelease(NULL);
      break;
  }
}

void setMidiFilter(){
  switch (wvr.getVoice(0)){
    case 0:
      wvr.setMidiHook(midiHookKeyboard);
      break;
    case 1:
      wvr.setMidiHook(midiHookDrums);
      break;
    default:
      wvr.setMidiHook(midiHookNone);
      break;
  }
}

void flashWifiOn(void* pvParameters){
  bool state = true;
  int cnt = 0;
  unsigned long last = millis();
  for(;;){
    unsigned long time = millis();
    if(time - last > 100){
      last = time;
      writeBinaryToLEDs(state ? 255 : 0);
      state = !state;
      if (++cnt == 10){
        writeVolumeToLEDs();
        break;
      } 
    }
  }
  vTaskDelete(NULL); // delete this task once the loop is exited
}

void checkForWifiOn(void){
  if(digitalRead(D12) == 0){
    wvr.wifiOn();
    // if a function takes a long time, it must run inside a FreerRTOS task
    BaseType_t ret = xTaskCreatePinnedToCore(&flashWifiOn, "flashWifiOn task", 1024, NULL, 1 , NULL, 0);
    if (ret != pdPASS) log_e("failed to create flashWifiOn task");
  }
}

void setup(){
  wvr.useFTDI = ENABLE_FTDI;
  wvr.useUsbMidi = true;
  wvr.begin();
  
  gpioInit();
  wvr.encoderInit(D6, D5);

  checkForWifiOn();

  StompA = new Button(D12, FALLING, 60);
  StompB = new Button(D13, FALLING, 60);
  EncButton = new Button(D4, FALLING, 60);

  setMidiFilter();
  setButtonFuncs();
  EncButton->onPress(encButtonDown);
  EncButton->onRelease(encButtonUp);

  wvr.onEncoder(onEncoderThames);
  writeVolumeToLEDs();
}

void loop(){
  // vTaskDelay(portMAX_DELAY);
  vTaskDelete(NULL);
}