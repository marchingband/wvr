#ifndef OSCX_H
#define OSCX_H


#include <OSCBundle.h>

#ifdef __cplusplus
extern "C"
{
#endif

static void handle_OSC(OSCMessage * msg);

void set_osc_hook(void(*fn)(OSCMessage *in));
void osc_hook_default(OSCMessage * in);
#ifdef __cplusplus
}
#endif
#endif