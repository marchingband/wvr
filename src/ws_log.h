#ifndef WS_LOG_H
#define WS_LOG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp32-hal-log.h"

int w_log_printf(int verbosity, const char *fmt, ...);

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
#define wlog_v(format, ...) w_log_printf(5,ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#define isr_wlog_v(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#else
#define wlog_v(format, ...)
#define isr_wlog_v(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
#define wlog_d(format, ...) w_log_printf(4,ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#define isr_wlog_d(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#else
#define wlog_d(format, ...)
#define isr_wlog_d(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
#define wlog_i(format, ...) w_log_printf(3,ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#define isr_wlog_i(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#else
#define wlog_i(format, ...)
#define isr_wlog_i(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
#define wlog_w(format, ...) w_log_printf(2,ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#define isr_wlog_w(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#else
#define wlog_w(format, ...)
#define isr_wlog_w(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
#define wlog_e(format, ...) w_log_printf(1,ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define isr_wlog_e(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#else
#define wlog_e(format, ...)
#define isr_wlog_e(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_NONE
#define wlog_n(format, ...) w_log_printf(0,ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define isr_wlog_n(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#else
#define wlog_n(format, ...)
#define isr_wlog_n(format, ...)
#endif

#undef log_i
#define log_i wlog_i


#ifdef __cplusplus
}
#endif
#endif /* __WS_LOG_H__ */
