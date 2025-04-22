#ifndef HOSTMAN_NETWORK_H
#define HOSTMAN_NETWORK_H

#include "config.h"
#include <curl/curl.h>
#include <stdbool.h>

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
} upload_response_t;

bool network_init(void);
upload_response_t *network_upload_file(const char *file_path, host_config_t *host);
void network_free_response(upload_response_t *response);
void network_cleanup(void);

#endif
