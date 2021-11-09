/*
  WVR.h - Library for building WVR firmware.
  Created by Andrew March, May 20, 2021.
  Released into the public domain.
*/
#ifndef WVR_H
#define WVR_H

#include "Arduino.h"

class WVR {
    public:
        WVR();
        void begin(void);
        void play(uint8_t voice, uint8_t note, uint8_t velocity);
        void stop(uint8_t voice, uint8_t note);
        void wifiOff(void);
        void wifiOn(void);
        void toggleWifi(void);
        void setGlobalVolume(uint8_t volume);
        void mute(void);
        void unmute(void);
        void setMidiHook(uint8_t*(*fn)(uint8_t *in));
        void encoderInit(int encA, int encB);
        void onEncoder(void (*handleEncoder)(bool up));
        void resetPin(int pin);
        // int globalVolume;
        // bool mute;
        // bool autoConfigPins;
        bool wifiIsOn;
        bool useFTDI;
        bool useUsbMidi;
        bool forceWifiOn;
        bool checkRecoveryModePin;
};

#endif