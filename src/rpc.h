#ifndef RPC_H
#define RPC_H
#ifdef __cplusplus
extern "C"
{
#endif


struct rpc_event_t {
    int procedure;
    int arg0;
    int arg1;
    int arg2;
};

enum rpc_procedure {
    RPC_NOTE_ON,
    RPC_NOTE_OFF,
    RPC_VOICE_UP,
    RPC_VOICE_DOWN,
    RPC_WIFI_TOGGLE,
};

void rpc_out(int procedure, int arg0, int arg1, int arg2);
void rpc_init(void);

#ifdef __cplusplus
}
#endif
#endif