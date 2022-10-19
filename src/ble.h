#ifndef BLE_H
#define BLE_H

void ble_init(void);
void ble_connect_task(void *dummy);
void ble_poll_for_connection(void);

void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp);
void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp);
void handleControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp);
void handleProgramChange(uint8_t channel, uint8_t program, uint16_t timestamp);

void handleMidiIn(uint8_t channel, uint8_t code, uint8_t note, uint8_t velocity);

#endif