#ifndef MIDI_IN_H
#define MIDI_IN_H
#ifdef __cplusplus
  extern "C"
  {
#endif

#define MIDI_NOTE_OFF 8
#define MIDI_NOTE_ON 9
#define MIDI_PROGRAM_CHANGE 12
#define MIDI_PITCH_BEND 14
#define MIDI_CC 11
#define MIDI_CC_VOLUME 7
#define MIDI_CC_PAN 10
#define MIDI_CC_EXP 11
#define MIDI_CC_SUSTAIN 64
#define MIDI_CC_ATTACK 73
#define MIDI_CC_RELEASE 72
#define MIDI_CC_MUTE 120
#define MIDI_CC_RESET 121

#define MIDI_CC_EQ_BASS 20
#define MIDI_CC_EQ_TREBLE 21

struct midi_event_t {
  uint8_t code;
  uint8_t note;
  uint8_t velocity;
  uint8_t channel;
};

uint8_t *get_channel_lut(void);
void midi_hook_default(uint8_t* in);
void set_midi_hook(void(*fn)(uint8_t *in));
static void handle_midi(uint8_t *msg);

#ifdef __cplusplus
}
#endif
#endif