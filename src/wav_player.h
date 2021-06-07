#ifndef WAV_PLAYER_H
#define WAV_PLAYER_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct wav_player_event_t {
    uint8_t code;
    uint8_t voice;
    uint8_t note;
    uint8_t velocity;
};

void current_bank_up(void);
void current_bank_down(void);
void play_wav(uint8_t voice, uint8_t note, uint8_t velocity);
void stop_wav(uint8_t voice, uint8_t note);


#ifdef __cplusplus
}
#endif
#endif