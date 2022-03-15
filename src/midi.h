#ifndef MIDI_H
#define MIDI_H

void midi_parser_init(void);

#ifdef __cplusplus

extern "C" {
#endif

uint8_t* midi_parse(uint8_t in);
uint8_t* usb_midi_parse(uint8_t in);
uint8_t* web_midi_parse(uint8_t in);
void midi_hook_default(uint8_t *in);

#ifdef __cplusplus
}
#endif

#endif