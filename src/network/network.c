#include "hostman/network/network.h"
#include "hostman/core/logging.h"
#include "hostman/core/utils.h"
#include "hostman/crypto/encryption.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MIN_PROGRESS_UPDATE_MS 100

static network_config_t global_config = { .timeout_seconds = DEFAULT_TIMEOUT_SECONDS,
                                          .max_retries = DEFAULT_MAX_RETRIES,
                                          .retry_delay_ms = DEFAULT_RETRY_DELAY_MS,
                                          .enable_http2 = true,
                                          .proxy_url = NULL,
                                          .verbose = false };

typedef struct
{
    char *data;
    size_t size;
} response_data_t;

static size_t
write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t real_size = size * nmemb;
    response_data_t *resp = (response_data_t *)userp;

    char *ptr = realloc(resp->data, resp->size + real_size + 1);
    if (!ptr)
    {
        log_error("Failed to allocate memory for response data");
        return 0;
    }

    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, real_size);
    resp->size += real_size;
    resp->data[resp->size] = 0;

    return real_size;
}

static int
progress_callback(void *clientp,
                  curl_off_t dltotal,
                  curl_off_t dlnow,
                  curl_off_t ultotal,
                  curl_off_t ulnow)
{
    if (ultotal == 0)
        return 0;

    progress_data_t *prog = (progress_data_t *)clientp;
    double percent = (double)ulnow / (double)ultotal * 100.0;

    time_t now = time(NULL);
    if (percent != prog->last_percent &&
        (difftime(now, prog->last_time) * 1000 >= MIN_PROGRESS_UPDATE_MS || percent == 100.0 ||
         prog->last_percent == 0.0))
    {

        double speed = 0.0;
        if (difftime(now, prog->last_time) > 0)
        {
            speed = (ulnow - prog->last_bytes) / difftime(now, prog->last_time);
        }

        fprintf(stderr, "\r\033[K");
        fprintf(stderr, "Uploading: [");

        int bar_width = 30;
        int pos = bar_width * percent / 100.0;

        for (int i = 0; i < bar_width; i++)
        {
            if (i < pos)
                fprintf(stderr, "=");
            else if (i == pos)
                fprintf(stderr, ">");
            else
                fprintf(stderr, " ");
        }

        fprintf(stderr, "] %.1f%% ", percent);

        char progress_str[32];
        format_file_size(ulnow, progress_str, sizeof(progress_str));

        char total_str[32];
        format_file_size(ultotal, total_str, sizeof(total_str));

        fprintf(stderr, "(%s / %s)", progress_str, total_str);

        if (speed > 0)
        {
            char speed_str[32];
            format_file_size(speed, speed_str, sizeof(speed_str));
            fprintf(stderr, " - %s/s", speed_str);
        }

        prog->last_percent = percent;
        prog->last_bytes = ulnow;
        prog->last_time = now;
    }

    return 0;
}

bool
network_init(void)
{
    curl_version_info_data *version_info = curl_version_info(CURLVERSION_NOW);
    if (version_info->features & CURL_VERSION_HTTP2)
    {
        log_info("HTTP/2 support enabled");
    }
    else
    {
        log_warn("HTTP/2 not supported by libcurl, falling back to HTTP/1.1");
        global_config.enable_http2 = false;
    }
    return (curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK);
}

void
network_set_config(network_config_t *config)
{
    if (config)
    {
        global_config.timeout_seconds = config->timeout_seconds;
        global_config.max_retries = config->max_retries;
        global_config.retry_delay_ms = config->retry_delay_ms;
        global_config.enable_http2 = config->enable_http2;
        if (global_config.proxy_url)
        {
            free(global_config.proxy_url);
        }
        global_config.proxy_url = config->proxy_url ? strdup(config->proxy_url) : NULL;
        global_config.verbose = config->verbose;
    }
}

static void
configure_curl_handle(CURL *curl,
                      struct curl_slist *headers,
                      response_data_t *response_data,
                      progress_data_t *prog_data,
                      const char *url)
{
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_data);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, prog_data);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, global_config.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, global_config.timeout_seconds);

    if (global_config.enable_http2)
    {
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    }

    if (global_config.proxy_url)
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, global_config.proxy_url);
    }

    if (global_config.verbose)
    {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
}

upload_response_t *
network_upload_file(const char *file_path, host_config_t *host)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    response_data_t response_data = { 0 };
    progress_data_t prog_data = { 0 };
    upload_response_t *response = malloc(sizeof(upload_response_t));
    int retry_count = 0;

    if (!response)
    {
        log_error("Failed to allocate memory for upload response");
        return NULL;
    }

    response->success = false;
    response->url = NULL;
    response->deletion_url = NULL;
    response->error_message = NULL;
    response->request_time_ms = 0.0;
    response->retry_count = 0;
    response->http_code = 0;

    if (access(file_path, R_OK) != 0)
    {
        response->error_message = strdup("File not found or not readable");
        return response;
    }

    struct stat file_stat;
    if (stat(file_path, &file_stat) != 0)
    {
        response->error_message = strdup("Failed to get file information");
        return response;
    }

    do
    {
        if (retry_count > 0)
        {
            log_info(
              "Retrying upload (attempt %d of %d)", retry_count + 1, global_config.max_retries);
            usleep(global_config.retry_delay_ms * 1000);
        }

        curl = curl_easy_init();
        if (!curl)
        {
            response->error_message = strdup("Failed to initialize curl");
            return response;
        }

        free(response_data.data);
        response_data.data = NULL;
        response_data.size = 0;

        curl_mime *mime = curl_mime_init(curl);
        if (!mime)
        {
            response->error_message = strdup("Failed to initialize mime form");
            curl_easy_cleanup(curl);
            return response;
        }

        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_name(part, host->file_form_field);
        curl_mime_filedata(part, file_path);

        for (int i = 0; i < host->static_field_count; i++)
        {
            part = curl_mime_addpart(mime);
            curl_mime_name(part, host->static_field_names[i]);
            curl_mime_data(part, host->static_field_values[i], CURL_ZERO_TERMINATED);
        }

        if (strcmp(host->auth_type, "bearer") == 0)
        {
            char *api_key = encryption_decrypt_api_key(host->api_key_encrypted);
            if (api_key)
            {
                char auth_header[1024];
                snprintf(
                  auth_header, sizeof(auth_header), "%s: Bearer %s", host->api_key_name, api_key);
                headers = curl_slist_append(headers, auth_header);
                free(api_key);
            }
            else
            {
                response->error_message = strdup("Failed to decrypt API key");
                curl_easy_cleanup(curl);
                curl_mime_free(mime);
                return response;
            }
        }
        else if (strcmp(host->auth_type, "header") == 0)
        {
            char *api_key = encryption_decrypt_api_key(host->api_key_encrypted);
            if (api_key)
            {
                char auth_header[1024];
                snprintf(auth_header, sizeof(auth_header), "%s: %s", host->api_key_name, api_key);
                headers = curl_slist_append(headers, auth_header);
                free(api_key);
            }
            else
            {
                response->error_message = strdup("Failed to decrypt API key");
                curl_easy_cleanup(curl);
                curl_mime_free(mime);
                return response;
            }
        }

        configure_curl_handle(curl, headers, &response_data, &prog_data, host->api_endpoint);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        prog_data.last_time = time(NULL);

        log_info("Connecting to host: %s (attempt %d)", host->api_endpoint, retry_count + 1);
        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        res = curl_easy_perform(curl);

        clock_gettime(CLOCK_MONOTONIC, &end_time);
        double time_taken_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
        time_taken_ms += (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
        response->request_time_ms = time_taken_ms;

        fprintf(stderr, "\r\033[K");

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->http_code);

        if (res != CURLE_OK)
        {
            free(response->error_message);
            response->error_message = strdup(curl_easy_strerror(res));
            log_error("Upload failed: %s", response->error_message);
        }
        else if (response->http_code >= 200 && response->http_code < 300)
        {
            char *url = extract_json_string(response_data.data, host->response_url_json_path);
            if (url)
            {
                response->success = true;
                response->url = url;
                log_info("Upload successful, URL: %s", url);

                if (host->response_deletion_url_json_path &&
                    strlen(host->response_deletion_url_json_path) > 0)
                {
                    char *deletion_url = extract_json_string(response_data.data,
                                                             host->response_deletion_url_json_path);
                    if (deletion_url)
                    {
                        response->deletion_url = deletion_url;
                        log_info("Deletion URL extracted: %s", deletion_url);
                    }
                    else
                    {
                        log_warn("Could not extract deletion URL using path: %s",
                                 host->response_deletion_url_json_path);
                    }
                }
                break;
            }
            else
            {
                free(response->error_message);
                response->error_message = strdup("Failed to extract URL from response");
                log_error("Failed to extract URL from response: %s", response_data.data);
            }
        }

        curl_easy_cleanup(curl);
        curl_mime_free(mime);
        curl_slist_free_all(headers);
        headers = NULL;

        retry_count++;
    } while (retry_count < global_config.max_retries && !response->success);

    response->retry_count = retry_count;
    free(response_data.data);

    return response;
}

void
network_free_response(upload_response_t *response)
{
    if (response)
    {
        free(response->url);
        free(response->deletion_url);
        free(response->error_message);
        free(response);
    }
}

void
network_cleanup(void)
{
    if (global_config.proxy_url)
    {
        free(global_config.proxy_url);
        global_config.proxy_url = NULL;
    }
    curl_global_cleanup();
}
