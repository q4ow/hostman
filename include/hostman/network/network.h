#ifndef HOSTMAN_NETWORK_H
#define HOSTMAN_NETWORK_H

#include "hostman/core/config.h"
#include <curl/curl.h>
#include <stdbool.h>

#define DEFAULT_TIMEOUT_SECONDS 30
#define DEFAULT_MAX_RETRIES 3
#define DEFAULT_RETRY_DELAY_MS 1000

typedef struct
{
    long timeout_seconds;
    int max_retries;
    long retry_delay_ms;
    bool enable_http2;
    char *proxy_url;
    bool verbose;
} network_config_t;

typedef struct
{
    double last_percent;
    curl_off_t last_bytes;
    time_t last_time;
} progress_data_t;

typedef struct
{
    bool success;
    char *url;
    char *deletion_url;
    char *error_message;
    double request_time_ms;
    int retry_count;
    long http_code;
} upload_response_t;

bool
network_init(void);
void
network_set_config(network_config_t *config);
upload_response_t *
network_upload_file(const char *file_path, host_config_t *host);
void
network_free_response(upload_response_t *response);
void
network_cleanup(void);

#endif
