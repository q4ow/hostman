#ifndef HOSTMAN_HOSTS_H
#define HOSTMAN_HOSTS_H

#include "config.h"
#include <stdbool.h>

int hosts_add_interactive(void);

bool hosts_add(const char *name, const char *api_endpoint, const char *auth_type,
               const char *api_key_name, const char *api_key,
               const char *request_body_format, const char *file_form_field,
               const char *response_url_json_path,
               char **static_field_names, char **static_field_values, int static_field_count);

#endif
