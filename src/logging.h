#ifndef HOSTMAN_LOGGING_H
#define HOSTMAN_LOGGING_H

#include <stdbool.h>

typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

bool
logging_init(void);

void
log_message(log_level_t level,
            const char *file,
            int line,
            const char *function,
            const char *format,
            ...);

#define log_debug(format, ...)                                                                     \
    log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define log_info(format, ...)                                                                      \
    log_message(LOG_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define log_warn(format, ...)                                                                      \
    log_message(LOG_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define log_error(format, ...)                                                                     \
    log_message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

void
logging_cleanup(void);

#endif