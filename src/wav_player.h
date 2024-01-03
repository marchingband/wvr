#ifndef WAV_PLAYER_H
#define WAV_PLAYER_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define ASR_ATTACK 0
#define ASR_HEAD 1
#define ASR_SUSTAIN 2
#define ASR_RELEASE 3

#define PAUSE_NONE 0
#define PAUSE_START 1
#define PAUSE_PAUSED 2
#define PAUSE_RESUMING 3


struct pan_t {
    uint8_t left_vol;
    uint8_t right_vol;
};

#define FADE_NORMAL -2
#define FADE_OUT -1
#define FADE_IN_INIT 0
#define FADE_FACTOR_MULTIPLIER 45 // number of 256 sample loops to wait before inc/dec volume by 1

#define FX_NONE ((struct pan_t) {.left_vol = 127, .right_vol = 127})

struct wav_player_event_t {
    uint8_t code;
    uint8_t voice;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

void current_bank_up(void);
void current_bank_down(void);
void play_wav(uint8_t voice, uint8_t note, uint8_t velocity);
void toggle_wav(uint8_t voice, uint8_t note, uint8_t velocity);
void stop_wav(uint8_t voice, uint8_t note);
void set_mute(bool should_mute);

#ifdef __cplusplus
}
#endif
#endif