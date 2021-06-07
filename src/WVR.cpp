#include "Arduino.h"
#include "WVR.h"
#include "wvr_0.3.h"
#include "wav_player.h"
#include "server.h"

WVR::WVR()
{
    // this->autoConfigPins = true;
    this->wifiIsOn = get_wifi_is_on();
    // this->mute = false;
    // this->globalVolume = 127;
}

void WVR::begin()
{
    wvr_init();
}

void WVR::play(uint8_t voice, uint8_t note, uint8_t velocity)
{
    play_wav(voice,note,velocity);
}

void WVR::stop(uint8_t voice, uint8_t note)
{
    stop_wav(voice,note);
}

void WVR::wifiOff()
{
    server_pause();
    this->wifiIsOn = get_wifi_is_on();
}

void WVR::wifiOn()
{
    server_resume();
    this->wifiIsOn = get_wifi_is_on();
}

void WVR::toggleWifi()
{
    this->wifiIsOn ? wifiOff() : wifiOn();
    // wifiIsOn = !wifiIsOn;
    this->wifiIsOn = get_wifi_is_on();
}