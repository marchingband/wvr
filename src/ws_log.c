#include "esp32-hal.h"
#include "Arduino.h"
#include <stdarg.h>
#include "file_system.h"

int log_printf(const char *fmt, ...);
void sendWSLog(char* msg);

struct metadata_t metadata;

int w_log_printf(int verbosity, const char *format, ...)
{
    static char loc_buf[64];
    char * temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, arg);
    va_end(copy);
    if(len >= sizeof(loc_buf)){
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len+1, format, arg);

    log_printf(temp);
    if(metadata.wlog_verbosity >= verbosity)
    {
        sendWSLog(temp);
    }
// #if !CONFIG_DISABLE_HAL_LOCKS
//     if(_uart_bus_array[s_uart_debug_nr].lock){
//         xSemaphoreTake(_uart_bus_array[s_uart_debug_nr].lock, portMAX_DELAY);
//         ets_printf("%s", temp);
//         xSemaphoreGive(_uart_bus_array[s_uart_debug_nr].lock);
//     } else {
//         ets_printf("%s", temp);
//     }
// #else
//     ets_printf("%s", temp);
// #endif
    va_end(arg);
    if(len >= sizeof(loc_buf)){
        free(temp);
    }
    return len;
}
