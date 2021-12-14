#ifndef POT_H
#define POT_H
#ifdef __cplusplus 
    extern "C" {
#endif

// #include "Arduino.h"

void (* onPot)(uint32_t);
void pot_init(void);

#ifdef __cplusplus
    }
#endif
#endif