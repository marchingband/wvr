#include "Arduino.h"
#include "WVR.h"
#include "wvr_0.3.h"
#include "wav_player.h"
#include "server.h"

WVR::WVR()
{
}

void WVR::begin()
{
    wvr_init();
}
void WVR::play(uint8_t voice, uint8_t note, uint8_t velocity)
{
    play_wav(voice,note,velocity);
}
void WVR::serverPause()
{
    server_pause();
}
void WVR::serverResume()
{
    server_resume();
}