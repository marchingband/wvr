#ifndef ENCODER_H
#define ENCODER_H

void (*onEncoder)(bool);
// void encoder_init(void);
void encoder_init(int enc_a, int enc_b);

#endif