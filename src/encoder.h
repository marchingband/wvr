#ifndef ENCODER_H
#define ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

void (*on_encoder)(bool);
void encoder_init(int enc_a, int enc_b);

#ifdef __cplusplus
}
#endif

#endif