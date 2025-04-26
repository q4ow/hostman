#include "hostman/core/logging.h"
#include "hostman/core/config.h"
#include "hostman/core/utils.h"
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static FILE *log_file = NULL;
static log_level_t current_log_level = LOG_LEVEL_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static log_level_t
string_to_log_level(const char *level_str)
{
    if (!level_str)
    {
        return LOG_LEVEL_INFO;
    }

    if (strcasecmp(level_str, "DEBUG") == 0)
    {
        return LOG_LEVEL_DEBUG;
    }
    else if (strcasecmp(level_str, "INFO") == 0)
    {
        return LOG_LEVEL_INFO;
    }
    else if (strcasecmp(level_str, "WARN") == 0)
    {
        return LOG_LEVEL_WARN;
    }
    else if (strcasecmp(level_str, "ERROR") == 0)
    {
        return LOG_LEVEL_ERROR;
    }

    return LOG_LEVEL_INFO;
}

static const char *
log_level_to_string(log_level_t level)
{
    switch (level)
    {
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_WARN:
            return "WARN";
        case LOG_LEVEL_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

bool
logging_init(void)
{
    pthread_mutex_lock(&log_mutex);

    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }

    hostman_config_t *config = config_load();
    if (config)
    {
        if (config->log_level)
        {
            current_log_level = string_to_log_level(config->log_level);
        }

        if (config->log_file)
        {
            char *last_slash = strrchr(config->log_file, '/');
            if (last_slash)
            {
                char dir_path[512];
                size_t dir_len = last_slash - config->log_file;
                strncpy(dir_path, config->log_file, dir_len);
                dir_path[dir_len] = '\0';

                struct stat st;
                if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode))
                {
                    char cmd[550];
                    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dir_path);
                    system(cmd);
                }
            }

            log_file = fopen(config->log_file, "a");
            if (!log_file)
            {
                fprintf(stderr,
                        "Warning: Could not open log file '%s': %s\n",
                        config->log_file,
                        strerror(errno));
            }
        }
    }

    if (!log_file)
    {
        char *cache_dir = get_cache_dir();
        if (cache_dir)
        {
            char log_path[512];
            snprintf(log_path, sizeof(log_path), "%s/hostman.log", cache_dir);

            struct stat st;
            if (stat(cache_dir, &st) != 0 || !S_ISDIR(st.st_mode))
            {
                mkdir(cache_dir, 0755);
            }

            log_file = fopen(log_path, "a");
            if (!log_file)
            {
                fprintf(stderr,
                        "Warning: Could not open default log file '%s': %s\n",
                        log_path,
                        strerror(errno));
            }

            free(cache_dir);
        }
    }

    pthread_mutex_unlock(&log_mutex);

    log_info("Logging system initialized (level: %s)", log_level_to_string(current_log_level));

    return true;
}

void
log_message(log_level_t level,
            const char *file,
            int line,
            const char *function,
            const char *format,
            ...)
{
    if (level < current_log_level)
    {
        return;
    }

    pthread_mutex_lock(&log_mutex);

    if (!log_file && !logging_init())
    {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_now);

    const char *level_str = log_level_to_string(level);
    char *basename = NULL;

    if (file)
    {
        basename = strrchr(file, '/');
        if (basename)
        {
            basename++;
        }
        else
        {
            basename = (char *)file;
        }
    }
    else
    {
        basename = "unknown";
    }

    va_list args;
    va_start(args, format);
    char msg_buffer[4096];
    vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);
    va_end(args);

    if (log_file)
    {
        fprintf(log_file,
                "[%s] [%s] [%s:%d %s] %s\n",
                timestamp,
                level_str,
                basename,
                line,
                function,
                msg_buffer);
        fflush(log_file);
    }

    if (level == LOG_LEVEL_ERROR)
    {
        fprintf(stderr, "[%s] ERROR: %s\n", timestamp, msg_buffer);
    }

    pthread_mutex_unlock(&log_mutex);
}

void
logging_cleanup(void)
{
    pthread_mutex_lock(&log_mutex);

    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }

    pthread_mutex_unlock(&log_mutex);
}