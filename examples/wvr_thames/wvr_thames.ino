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
#include "rgb.h"

#define KICK 35
#define SNARE 38
#define LOW_TOM 41
#define HIHAT_CLOSED 42
#define HIHAT_PEDAL 44
#define HIHAT_OPEN 46
#define CRASH 49
#define HI_TOM 50
#define RIDE 51

#define LED1 D3
#define LED2 D11
#define LED3 D1
#define LED4 D0

#define MIDI_KICK 35
#define MIDI_PEDAL_HIHAT 44

#define LED_FREQ 5000
#define ENABLE_FTDI false

void setButtonFuncs(void);
void setMidiFilter(void);

WVR wvr;

Button *StompA;
Button *StompB;
Button *EncButton;

bool encPushed = false;
uint8_t volume = 127;
uint8_t voice = 0;

bool hold = false;
bool held[127] = {false};
bool damp = false;
bool hihatOpen = false;

void gpioInit(void)
{
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

void writeBinaryToLEDs(uint8_t num)
{
  ledcWrite(0, (num & (0x01 << 0)) ? 255 : 0);
  ledcWrite(1, (num & (0x01 << 1)) ? 255 : 0);
  ledcWrite(2, (num & (0x01 << 2)) ? 255 : 0);
  ledcWrite(3, (num & (0x01 << 3)) ? 255 : 0);
}

void writeVolumeToLEDs(void)
{
  int led_1 = (volume > 32) ? 255 : (volume * 8);
  int led_2 = (volume > 64) ? 255 : (volume < 32) ? 0 : ((volume - 32) * 8);
  int led_3 = (volume > 96) ? 255 : (volume < 64) ? 0 : ((volume - 64) * 8);
  int led_4 = (volume < 96) ? 0   : (volume - 96) * 8;
  ledcWrite(0, led_1);
  ledcWrite(1, led_2);
  ledcWrite(2, led_3);
  ledcWrite(3, led_4);
}

void onEncoderThames(bool down)
{
  if(down)
  {
    if(encPushed)
    {
      voice -= (voice > 0);
      writeBinaryToLEDs(voice);
      setButtonFuncs();
      setMidiFilter();
    }
    else
    {
      volume -= (volume > 0);
      wvr.setGlobalVolume(volume);
      writeVolumeToLEDs();
    }
  } 
  else
  {
    if(encPushed)
    {
      voice += (voice < 15);
      writeBinaryToLEDs(encPushed ? voice : (volume / 8));
    }
    else
    {
      volume += (volume < 126);
      wvr.setGlobalVolume(volume);
      writeVolumeToLEDs();
    }
  }
  setButtonFuncs();
  setMidiFilter();
  // writeBinaryToLEDs(encPushed ? voice : (volume / 8));
  // wlog_i("%d", down ? --cnt : ++cnt);
}

void handleReleaseStompADrums(void)
{
  hihatOpen = true;
}

void handlePressStompADrums(void)
{
  // wvr.play(voice, MIDI_PEDAL_HIHAT, 127);
  wvr.stop(0, HIHAT_OPEN);
  wvr.play(0, HIHAT_PEDAL, 127);
  hihatOpen = false;
}

void handlePressStompBDrums(void)
{
  // wvr.play(voice, KICK, 127);
  wvr.play(0, KICK, 80);
}

void handleReleaseStompAKeyboard(void)
{
  for(int i=0;i<127;i++)
  {
    if(held[i])
    {
      // log_i("release %d", i);
      wvr.stop(voice, i);
      held[i] = false;
    }
  }
  hold = false;
}

void handlePressStompAKeyboard(void)
{
  // log_i("switch one down");
  hold = true;
}

void handleReleaseStompBKeyboard(void)
{
  // log_i("switch two up");
  damp = false;
}

void handlePressStompBKeyboard(void)
{
  // log_i("switch two down");
  damp = true;
}

void encButtonUp(void)
{
  // wlog_i("EncButton up");
  encPushed = false;
  writeVolumeToLEDs();
}

void encButtonDown(void)
{
  // wlog_i("EncButton down");
  encPushed = true;
  writeBinaryToLEDs(voice);
}


uint8_t* shells(uint8_t * msg)
{
  if(!msg)return msg; // guard against NULL notes
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  if(code == MIDI_NOTE_ON)
  {
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
  return msg;
}

uint8_t* sustain(uint8_t * msg)
{
  uint8_t channel = msg[0] & 0b00001111;
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  log_i("ch: %d code %d", channel, code);
  if((code == MIDI_NOTE_OFF) && hold)
  {
    uint8_t note = msg[1] & 0b01111111;
    // remember that its being held
    held[note] = true;
    // don't pass it on
    return NULL;
  }
  else if((code == MIDI_NOTE_ON) && hold)
  {
    // remove potential note off event from hold
    uint8_t note = msg[1] & 0b01111111;
    held[note] = false;
  }
  return msg;
}

uint8_t* soft(uint8_t * msg)
{
  // pass on NULL pointer immidiately
  if(!msg)return msg;
  uint8_t channel = msg[0] & 0b00001111;
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  log_i("ch: %d code %d", channel, code);
  if((code == MIDI_NOTE_ON) && damp)
  {
    uint8_t velocity = msg[2] & 0b01111111;
    velocity *= 0.4;
    msg[2] = velocity & 0b01111111; 
  }
  return msg;
}

uint8_t* midiHookKeyboard(uint8_t * msg)
{
  msg = sustain(msg);
  msg = soft(msg);
  return msg;
}

uint8_t* midiHookDrums(uint8_t * msg)
{
  if(!msg)return msg; // guard against NULL notes
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  // ignore all note-offs
  if(code == MIDI_NOTE_OFF)
  {
    // return NULL;
  }
  else if(code == MIDI_NOTE_ON)
  {
    msg = shells(msg);
    uint8_t note = msg[1] & 0b01111111;
    // hihat
    if(note == HIHAT_CLOSED && hihatOpen){
      note = HIHAT_OPEN;
    }
    msg[1] = note & 0b01111111;
  }
  return msg;
}

uint8_t* midiHookNone(uint8_t * msg)
{
  return msg;
}

// uint8_t* midiFilter(uint8_t * msg)
// {
//   msg = sustain(msg);
//   msg = soft(msg);
//   return msg;
// }

void setButtonFuncs()
{
  switch (voice)
  {
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

void setMidiFilter()
{
  wlog_i("set filter %d", voice);
  switch (voice)
  {
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

void setup() {
  wvr.useFTDI = ENABLE_FTDI;
  wvr.useUsbMidi = true;
  // wvr.checkRecoveryModePin = false;
  wvr.begin();

  // wvr.setMidiHook(midiHookKeyboard);
  // wvr.setMidiHook(midiFilter);
  gpioInit();
  wvr.encoderInit(D6, D5);

  StompA = new Button(D12, FALLING, 60);
  StompB = new Button(D13, FALLING, 60);
  EncButton = new Button(D4, FALLING, 60);

  // StompA->onPress(handlePressStompAKeyboard);
  // StompA->onRelease(handleReleaseStompAKeyboard);
  // StompB->onPress(handlePressStompBKeyboard);
  // StompB->onRelease(handleReleaseStompBKeyboard);
  setMidiFilter();
  setButtonFuncs();
  EncButton->onPress(encButtonDown);
  EncButton->onRelease(encButtonUp);

  wvr.wifiIsOn = get_metadata()->wifi_starts_on;
  log_i("wifi is %s", wvr.wifiIsOn ? "on" : "off");

  wvr.onEncoder(onEncoderThames);

  writeVolumeToLEDs();
}

void loop() {
  // vTaskDelay(portMAX_DELAY);
  vTaskDelete(NULL);
}