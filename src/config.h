#ifndef HOSTMAN_CONFIG_H
#define HOSTMAN_CONFIG_H

#include <stdbool.h>

typedef struct
{
    char *name;
    char *api_endpoint;
    char *auth_type;
    char *api_key_name;
    char *api_key_encrypted;
    char *request_body_format;
    char *file_form_field;
    char *response_url_json_path;
    char *response_deletion_url_json_path;
    char **static_field_names;
    char **static_field_values;
    int static_field_count;
} host_config_t;

typedef struct
{
    int version;
    char *default_host;
    char *log_level;
    char *log_file;
    host_config_t **hosts;
    int host_count;
} hostman_config_t;

hostman_config_t *config_load(void);

bool config_save(hostman_config_t *config);
char *config_get_path(void);
char *config_get_value(const char *key);
bool config_set_value(const char *key, const char *value);
bool config_add_host(host_config_t *host);
bool config_remove_host(const char *host_name);
bool config_set_default_host(const char *host_name);
host_config_t *config_get_default_host(void);
host_config_t *config_get_host(const char *host_name);
void config_free(hostman_config_t *config);

#endif
