#ifndef OSCX_H
#define OSCX_H


#include <OSCBundle.h>

#ifdef __cplusplus
extern "C"
{
#endif

static void OSC_handle(OSCBundle *bundle);

void set_osc_hook(void(*fn)(OSCBundle *in));
#ifdef __cplusplus
}
#endif
#endif