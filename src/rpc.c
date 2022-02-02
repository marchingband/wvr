// #include "esp32-hal-log.h"
#include "esp_err.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp32-hal-log.h"

#include "ws_log.h"
#include "midi_in.h"
#include "rpc.h"
#include "wav_player.h"
#include "file_system.h"

// QueueHandle_t rpc_in_queue;
QueueHandle_t rpc_out_queue;
// TaskHandle_t rpc_in_task_handle;
TaskHandle_t rpc_out_task_handle;
BaseType_t task_return;

#define NO_RPC_OUT 1

void sendWSMsg(char* msg);

void rpc_out(int procedure, int arg0, int arg1, int arg2)
{
    if(ARDUHAL_LOG_LEVEL == ARDUHAL_LOG_LEVEL_NONE) return;
    if(NO_RPC_OUT) return;
    wlog_d("rpc_out called");
    struct rpc_event_t rpc_event_out;
    rpc_event_out.procedure = procedure;
    rpc_event_out.arg0 = arg0;
    rpc_event_out.arg1 = arg1;
    rpc_event_out.arg2 = arg2;
    xQueueSendToBack(rpc_out_queue, (void *)&rpc_event_out, 0);
}

char* on_rpc_in(cJSON *json)
{
    enum rpc_procedure procedure = cJSON_GetObjectItemCaseSensitive(json, "procedure")->valueint;
    switch (procedure)
    {
    case RPC_NOTE_ON:
        {
            uint8_t voice = cJSON_GetObjectItemCaseSensitive(json, "voice")->valueint;
            uint8_t note = cJSON_GetObjectItemCaseSensitive(json, "note")->valueint;
            uint8_t velocity = cJSON_GetObjectItemCaseSensitive(json, "velocity")->valueint;
            toggle_wav(voice, note, velocity);
            char * res = "started wav";
            return(res);
            break;
        }
    case RPC_NOTE_OFF:
        {
            uint8_t voice = cJSON_GetObjectItemCaseSensitive(json, "voice")->valueint;
            uint8_t note = cJSON_GetObjectItemCaseSensitive(json, "note")->valueint;
            stop_wav(voice, note);
            char * res = "stopped wav";
            return(res);
            break;
        }
    default:
        break;
    }
}

extern struct metadata_t metadata;

void rpc_out_task(void* pvParameters)
{
    struct rpc_event_t event;
    for(;;){
        if(xQueueReceive(rpc_out_queue, (void *)&event, (portTickType)portMAX_DELAY))
        {

            log_d("rpc_out_task received event");
            if(metadata.wlog_verbosity == 0)
            {
                // dont do RPC if UI log level is "NONE"
                continue;
            }
            cJSON *rpc_root = cJSON_CreateObject();
            switch(event.procedure){
                case RPC_NOTE_ON:
                    wlog_d("rpc note on");
                    cJSON_AddNumberToObject(rpc_root, "procedure", RPC_NOTE_ON);
                    cJSON_AddNumberToObject(rpc_root, "voice", event.arg0);
                    cJSON_AddNumberToObject(rpc_root, "note", event.arg1);
                    cJSON_AddNumberToObject(rpc_root, "velocity", event.arg2);
                    break;
                case RPC_NOTE_OFF:
                    cJSON_AddNumberToObject(rpc_root, "procedure", RPC_NOTE_OFF);
                    cJSON_AddNumberToObject(rpc_root, "voice", event.arg0);
                    cJSON_AddNumberToObject(rpc_root, "note", event.arg1);
                    break;
                default: break;
            }
            char *json = cJSON_PrintUnformatted(rpc_root);
            log_v(" rpc json: %s",json);
            sendWSMsg(json);
            cJSON_Delete(rpc_root);
            // sendWSMsg("hit");
        }
    }
}

void rpc_init(void)
{
    // rpc_in_queue = xQueueCreate(20, sizeof(struct rpc_event_t));
    rpc_out_queue = xQueueCreate(20, sizeof(struct rpc_event_t));
    // task_return = xTaskCreate(rpc_in_task,"rpc_in_task", 1024, NULL, 1, rpc_in_task_handle);
    task_return = xTaskCreatePinnedToCore(rpc_out_task,"rpc_out_task", 1024 * 4, NULL, 2, rpc_out_task_handle,0);
}