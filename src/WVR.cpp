#include "Arduino.h"
#include "WVR.h"
#include "wvr_0.3.h"
#include "wav_player.h"
#include "server.h"
#include "file_system.h"
#include "encoder.h"

WVR::WVR()
{
    this->wifiIsOn = get_wifi_is_on();
    this->useFTDI = false;
    this->useUsbMidi = false;
    this->forceWifiOn = false;
    this->checkRecoveryModePin = true;
}

void WVR::begin()
{
    wvr_init(useFTDI, useUsbMidi, checkRecoveryModePin);
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

void WVR::setGlobalVolume(uint8_t volume)
{
    set_global_volume(volume);
}

void WVR::mute(void)
{
    set_mute(true);
}

void WVR::unmute(void)
{
    set_mute(false);
}

void WVR::setMidiHook(uint8_t*(*fn)(uint8_t *in))
{
    set_midi_hook(fn);
}

void WVR::encoderInit(int encA, int encB)
{
    encoder_init(encA, encB);
}

void WVR::onEncoder(void (*handleEncoder)(bool up))
{
    on_encoder = handleEncoder;
}

void WVR::resetPin(int pin)
{
    gpio_reset_pin(gpioNumToGpioNum_T(pin));
}