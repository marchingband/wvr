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
        void begin();
        void play(uint8_t voice, uint8_t note, uint8_t velocity);
        void serverPause();
        void serverResume();
};

#endif