#ifndef FP_H
#define FP_H

#include "stdint.h"

int32_t fxdiv_s15p16 (int32_t x, int32_t y)
{
    return ((int64_t)x * 65536) / y;
}

/* log2(2**8), ..., log2(2**1), log2(1+2**(-1), ..., log2(1+2**(-16)) */
const uint32_t tab [20] = {0x80000, 0x40000, 0x20000, 0x10000,
                           0x095c1, 0x0526a, 0x02b80, 0x01663,
                           0x00b5d, 0x005b9, 0x002e0, 0x00170, 
                           0x000b8, 0x0005c, 0x0002e, 0x00017, 
                           0x0000b, 0x00006, 0x00003, 0x00001};
const int32_t one_s15p16 = 1 * (1 << 16);
const int32_t neg_fifteen_s15p16 = (-15) * (1 << 16);

int32_t fxexp2_s15p16 (int32_t a) 
{
    uint32_t x, y;
    int32_t t, r;

    if (a <= neg_fifteen_s15p16) return 0; // underflow

    x = (a < 0) ? (-a) : (a);
    y = one_s15p16;
    /* process integer bits */
    if ((t = x - tab [0]) >= 0) { x = t; y = y << 8; }
    if ((t = x - tab [1]) >= 0) { x = t; y = y << 4; }
    if ((t = x - tab [2]) >= 0) { x = t; y = y << 2; }
    if ((t = x - tab [3]) >= 0) { x = t; y = y << 1; }
    /* process fractional bits */
    for (int shift = 1; shift <= 16; shift++) {
        if ((t = x - tab [3 + shift]) >= 0) { x = t; y = y + (y >> shift); }
    }
    r = (a < 0) ? fxdiv_s15p16 (one_s15p16, y) : y;
    return r;
}

#endif