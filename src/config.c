#include "config.h"
#include "logging.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef USE_CJSON
#include <cjson/cJSON.h>
#else
#include <jansson.h>
#endif

static hostman_config_t *current_config = NULL;

char *
config_get_path(void)
{
    char *config_dir = get_config_dir();
    if (!config_dir)
    {
        return NULL;
    }

    size_t len = strlen(config_dir) + strlen("/config.json") + 1;
    char *path = malloc(len);
    if (!path)
    {
        free(config_dir);
        return NULL;
    }

    snprintf(path, len, "%s/config.json", config_dir);
    free(config_dir);

    return path;
}

#ifdef USE_CJSON
static host_config_t *
parse_host_config(cJSON *host_json, const char *name)
{
    host_config_t *host = calloc(1, sizeof(host_config_t));
    if (!host)
    {
        return NULL;
    }

    host->name = strdup(name);

    cJSON *api_endpoint = cJSON_GetObjectItem(host_json, "api_endpoint");
    if (api_endpoint && cJSON_IsString(api_endpoint))
    {
        host->api_endpoint = strdup(api_endpoint->valuestring);
    }

    cJSON *auth_type = cJSON_GetObjectItem(host_json, "auth_type");
    if (auth_type && cJSON_IsString(auth_type))
    {
        host->auth_type = strdup(auth_type->valuestring);
    }

    cJSON *api_key_name = cJSON_GetObjectItem(host_json, "api_key_name");
    if (api_key_name && cJSON_IsString(api_key_name))
    {
        host->api_key_name = strdup(api_key_name->valuestring);
    }

    cJSON *api_key_encrypted = cJSON_GetObjectItem(host_json, "api_key_encrypted");
    if (api_key_encrypted && cJSON_IsString(api_key_encrypted))
    {
        host->api_key_encrypted = strdup(api_key_encrypted->valuestring);
    }

    cJSON *request_body_format = cJSON_GetObjectItem(host_json, "request_body_format");
    if (request_body_format && cJSON_IsString(request_body_format))
    {
        host->request_body_format = strdup(request_body_format->valuestring);
    }

    cJSON *file_form_field = cJSON_GetObjectItem(host_json, "file_form_field");
    if (file_form_field && cJSON_IsString(file_form_field))
    {
        host->file_form_field = strdup(file_form_field->valuestring);
    }

    cJSON *response_url_json_path = cJSON_GetObjectItem(host_json, "response_url_json_path");
    if (response_url_json_path && cJSON_IsString(response_url_json_path))
    {
        host->response_url_json_path = strdup(response_url_json_path->valuestring);
    }

    cJSON *static_form_fields = cJSON_GetObjectItem(host_json, "static_form_fields");
    if (static_form_fields && cJSON_IsObject(static_form_fields))
    {
        int field_count = cJSON_GetArraySize(static_form_fields);
        if (field_count > 0)
        {
            host->static_field_count = field_count;
            host->static_field_names = calloc(field_count, sizeof(char *));
            host->static_field_values = calloc(field_count, sizeof(char *));

            if (!host->static_field_names || !host->static_field_values)
            {
                free(host->static_field_names);
                free(host->static_field_values);
                host->static_field_count = 0;
            }
            else
            {
                int i = 0;
                cJSON *field;
                cJSON_ArrayForEach(field, static_form_fields)
                {
                    if (cJSON_IsString(field))
                    {
                        host->static_field_names[i] = strdup(field->string);
                        host->static_field_values[i] = strdup(field->valuestring);
                        i++;
                    }
                }
            }
        }
    }

    return host;
}

static hostman_config_t *
parse_config(cJSON *json)
{
    if (!json)
    {
        return NULL;
    }

    hostman_config_t *config = calloc(1, sizeof(hostman_config_t));
    if (!config)
    {
        return NULL;
    }

    cJSON *version = cJSON_GetObjectItem(json, "version");
    if (version && cJSON_IsNumber(version))
    {
        config->version = version->valueint;
    }
    else
    {
        config->version = 1;
    }

    cJSON *default_host = cJSON_GetObjectItem(json, "default_host");
    if (default_host && cJSON_IsString(default_host))
    {
        config->default_host = strdup(default_host->valuestring);
    }

    cJSON *log_level = cJSON_GetObjectItem(json, "log_level");
    if (log_level && cJSON_IsString(log_level))
    {
        config->log_level = strdup(log_level->valuestring);
    }
    else
    {
        config->log_level = strdup("INFO");
    }

    cJSON *log_file = cJSON_GetObjectItem(json, "log_file");
    if (log_file && cJSON_IsString(log_file))
    {
        config->log_file = strdup(log_file->valuestring);
    }
    else
    {
        char *cache_dir = get_cache_dir();
        if (cache_dir)
        {
            size_t len = strlen(cache_dir) + strlen("/hostman.log") + 1;
            config->log_file = malloc(len);
            if (config->log_file)
            {
                snprintf(config->log_file, len, "%s/hostman.log", cache_dir);
            }
            free(cache_dir);
        }
    }

    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    if (hosts && cJSON_IsObject(hosts))
    {
        int host_count = cJSON_GetArraySize(hosts);
        if (host_count > 0)
        {
            config->host_count = host_count;
            config->hosts = calloc(host_count, sizeof(host_config_t *));

            if (!config->hosts)
            {
                config->host_count = 0;
            }
            else
            {
                int i = 0;
                cJSON *host;
                cJSON_ArrayForEach(host, hosts)
                {
                    host_config_t *host_config = parse_host_config(host, host->string);
                    if (host_config)
                    {
                        config->hosts[i++] = host_config;
                    }
                }
            }
        }
    }

    return config;
}

static cJSON *
host_config_to_json(host_config_t *host)
{
    cJSON *json = cJSON_CreateObject();

    if (host->api_endpoint)
    {
        cJSON_AddStringToObject(json, "api_endpoint", host->api_endpoint);
    }

    if (host->auth_type)
    {
        cJSON_AddStringToObject(json, "auth_type", host->auth_type);
    }

    if (host->api_key_name)
    {
        cJSON_AddStringToObject(json, "api_key_name", host->api_key_name);
    }

    if (host->api_key_encrypted)
    {
        cJSON_AddStringToObject(json, "api_key_encrypted", host->api_key_encrypted);
    }

    if (host->request_body_format)
    {
        cJSON_AddStringToObject(json, "request_body_format", host->request_body_format);
    }

    if (host->file_form_field)
    {
        cJSON_AddStringToObject(json, "file_form_field", host->file_form_field);
    }

    if (host->response_url_json_path)
    {
        cJSON_AddStringToObject(json, "response_url_json_path", host->response_url_json_path);
    }

    if (host->static_field_count > 0 && host->static_field_names && host->static_field_values)
    {
        cJSON *static_form_fields = cJSON_CreateObject();
        for (int i = 0; i < host->static_field_count; i++)
        {
            if (host->static_field_names[i] && host->static_field_values[i])
            {
                cJSON_AddStringToObject(
                  static_form_fields, host->static_field_names[i], host->static_field_values[i]);
            }
        }
        cJSON_AddItemToObject(json, "static_form_fields", static_form_fields);
    }

    return json;
}

static cJSON *
config_to_json(hostman_config_t *config)
{
    cJSON *json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "version", config->version);

    if (config->default_host)
    {
        cJSON_AddStringToObject(json, "default_host", config->default_host);
    }

    if (config->log_level)
    {
        cJSON_AddStringToObject(json, "log_level", config->log_level);
    }

    if (config->log_file)
    {
        cJSON_AddStringToObject(json, "log_file", config->log_file);
    }

    cJSON *hosts = cJSON_CreateObject();
    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i] && config->hosts[i]->name)
        {
            cJSON *host_json = host_config_to_json(config->hosts[i]);
            cJSON_AddItemToObject(hosts, config->hosts[i]->name, host_json);
        }
    }
    cJSON_AddItemToObject(json, "hosts", hosts);

    return json;
}

#else

static host_config_t *
parse_host_config(json_t *host_json, const char *name)
{
    host_config_t *host = calloc(1, sizeof(host_config_t));
    if (!host)
    {
        return NULL;
    }

    host->name = strdup(name);

    json_t *api_endpoint = json_object_get(host_json, "api_endpoint");
    if (api_endpoint && json_is_string(api_endpoint))
    {
        host->api_endpoint = strdup(json_string_value(api_endpoint));
    }

    json_t *auth_type = json_object_get(host_json, "auth_type");
    if (auth_type && json_is_string(auth_type))
    {
        host->auth_type = strdup(json_string_value(auth_type));
    }

    json_t *api_key_name = json_object_get(host_json, "api_key_name");
    if (api_key_name && json_is_string(api_key_name))
    {
        host->api_key_name = strdup(json_string_value(api_key_name));
    }

    json_t *api_key_encrypted = json_object_get(host_json, "api_key_encrypted");
    if (api_key_encrypted && json_is_string(api_key_encrypted))
    {
        host->api_key_encrypted = strdup(json_string_value(api_key_encrypted));
    }

    json_t *request_body_format = json_object_get(host_json, "request_body_format");
    if (request_body_format && json_is_string(request_body_format))
    {
        host->request_body_format = strdup(json_string_value(request_body_format));
    }

    json_t *file_form_field = json_object_get(host_json, "file_form_field");
    if (file_form_field && json_is_string(file_form_field))
    {
        host->file_form_field = strdup(json_string_value(file_form_field));
    }

    json_t *response_url_json_path = json_object_get(host_json, "response_url_json_path");
    if (response_url_json_path && json_is_string(response_url_json_path))
    {
        host->response_url_json_path = strdup(json_string_value(response_url_json_path));
    }

    json_t *static_form_fields = json_object_get(host_json, "static_form_fields");
    if (static_form_fields && json_is_object(static_form_fields))
    {
        size_t field_count = json_object_size(static_form_fields);
        if (field_count > 0)
        {
            host->static_field_count = field_count;
            host->static_field_names = calloc(field_count, sizeof(char *));
            host->static_field_values = calloc(field_count, sizeof(char *));

            if (!host->static_field_names || !host->static_field_values)
            {
                free(host->static_field_names);
                free(host->static_field_values);
                host->static_field_count = 0;
            }
            else
            {
                int i = 0;
                const char *key;
                json_t *value;

                json_object_foreach(static_form_fields, key, value)
                {
                    if (json_is_string(value))
                    {
                        host->static_field_names[i] = strdup(key);
                        host->static_field_values[i] = strdup(json_string_value(value));
                        i++;
                    }
                }
            }
        }
    }

    return host;
}

static hostman_config_t *
parse_config(json_t *json)
{
    if (!json)
    {
        return NULL;
    }

    hostman_config_t *config = calloc(1, sizeof(hostman_config_t));
    if (!config)
    {
        return NULL;
    }

    json_t *version = json_object_get(json, "version");
    if (version && json_is_integer(version))
    {
        config->version = json_integer_value(version);
    }
    else
    {
        config->version = 1;
    }

    json_t *default_host = json_object_get(json, "default_host");
    if (default_host && json_is_string(default_host))
    {
        config->default_host = strdup(json_string_value(default_host));
    }

    json_t *log_level = json_object_get(json, "log_level");
    if (log_level && json_is_string(log_level))
    {
        config->log_level = strdup(json_string_value(log_level));
    }
    else
    {
        config->log_level = strdup("INFO");
    }

    json_t *log_file = json_object_get(json, "log_file");
    if (log_file && json_is_string(log_file))
    {
        config->log_file = strdup(json_string_value(log_file));
    }
    else
    {
        char *cache_dir = get_cache_dir();
        if (cache_dir)
        {
            size_t len = strlen(cache_dir) + strlen("/hostman.log") + 1;
            config->log_file = malloc(len);
            if (config->log_file)
            {
                snprintf(config->log_file, len, "%s/hostman.log", cache_dir);
            }
            free(cache_dir);
        }
    }

    json_t *hosts = json_object_get(json, "hosts");
    if (hosts && json_is_object(hosts))
    {
        size_t host_count = json_object_size(hosts);
        if (host_count > 0)
        {
            config->host_count = host_count;
            config->hosts = calloc(host_count, sizeof(host_config_t *));

            if (!config->hosts)
            {
                config->host_count = 0;
            }
            else
            {
                int i = 0;
                const char *key;
                json_t *value;

                json_object_foreach(hosts, key, value)
                {
                    host_config_t *host_config = parse_host_config(value, key);
                    if (host_config)
                    {
                        config->hosts[i++] = host_config;
                    }
                }
            }
        }
    }

    return config;
}

static json_t *
host_config_to_json(host_config_t *host)
{
    json_t *json = json_object();

    if (host->api_endpoint)
    {
        json_object_set_new(json, "api_endpoint", json_string(host->api_endpoint));
    }

    if (host->auth_type)
    {
        json_object_set_new(json, "auth_type", json_string(host->auth_type));
    }

    if (host->api_key_name)
    {
        json_object_set_new(json, "api_key_name", json_string(host->api_key_name));
    }

    if (host->api_key_encrypted)
    {
        json_object_set_new(json, "api_key_encrypted", json_string(host->api_key_encrypted));
    }

    if (host->request_body_format)
    {
        json_object_set_new(json, "request_body_format", json_string(host->request_body_format));
    }

    if (host->file_form_field)
    {
        json_object_set_new(json, "file_form_field", json_string(host->file_form_field));
    }

    if (host->response_url_json_path)
    {
        json_object_set_new(
          json, "response_url_json_path", json_string(host->response_url_json_path));
    }

    if (host->static_field_count > 0 && host->static_field_names && host->static_field_values)
    {
        json_t *static_form_fields = json_object();
        for (int i = 0; i < host->static_field_count; i++)
        {
            if (host->static_field_names[i] && host->static_field_values[i])
            {
                json_object_set_new(static_form_fields,
                                    host->static_field_names[i],
                                    json_string(host->static_field_values[i]));
            }
        }
        json_object_set_new(json, "static_form_fields", static_form_fields);
    }

    return json;
}

static json_t *
config_to_json(hostman_config_t *config)
{
    json_t *json = json_object();

    json_object_set_new(json, "version", json_integer(config->version));

    if (config->default_host)
    {
        json_object_set_new(json, "default_host", json_string(config->default_host));
    }

    if (config->log_level)
    {
        json_object_set_new(json, "log_level", json_string(config->log_level));
    }

    if (config->log_file)
    {
        json_object_set_new(json, "log_file", json_string(config->log_file));
    }

    json_t *hosts = json_object();
    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i] && config->hosts[i]->name)
        {
            json_t *host_json = host_config_to_json(config->hosts[i]);
            json_object_set_new(hosts, config->hosts[i]->name, host_json);
        }
    }
    json_object_set_new(json, "hosts", hosts);

    return json;
}
#endif

hostman_config_t *
config_load(void)
{
    if (current_config)
    {
        return current_config;
    }

    char *path = config_get_path();
    if (!path)
    {
        log_error("Failed to get config path");
        return NULL;
    }

    FILE *file = fopen(path, "r");
    if (!file)
    {
        if (errno != ENOENT)
        {
            log_error("Failed to open config file: %s", path);
        }
        free(path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer)
    {
        log_error("Failed to allocate memory for config file");
        fclose(file);
        free(path);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, size, file);
    fclose(file);

    if (read_size != size)
    {
        log_error("Failed to read config file");
        free(buffer);
        free(path);
        return NULL;
    }

    buffer[size] = '\0';

    hostman_config_t *config = NULL;

#ifdef USE_CJSON
    cJSON *json = cJSON_Parse(buffer);
    if (!json)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr)
        {
            log_error("JSON parse error: %s", error_ptr);
        }
    }
    else
    {
        config = parse_config(json);
        cJSON_Delete(json);
    }
#else
    json_error_t error;
    json_t *json = json_loads(buffer, 0, &error);
    if (!json)
    {
        log_error("JSON parse error: %s", error.text);
    }
    else
    {
        config = parse_config(json);
        json_decref(json);
    }
#endif

    free(buffer);
    free(path);

    if (config)
    {
        current_config = config;
    }

    return config;
}

bool
config_save(hostman_config_t *config)
{
    if (!config)
    {
        return false;
    }

    char *path = config_get_path();
    if (!path)
    {
        log_error("Failed to get config path");
        return false;
    }

    char *dir = get_config_dir();
    if (!dir)
    {
        free(path);
        return false;
    }

    if (access(dir, F_OK) != 0)
    {
        if (mkdir(dir, 0755) != 0)
        {
            log_error("Failed to create config directory: %s", dir);
            free(dir);
            free(path);
            return false;
        }
    }
    free(dir);

    FILE *file = fopen(path, "w");
    if (!file)
    {
        log_error("Failed to open config file for writing: %s", path);
        free(path);
        return false;
    }

    bool success = false;

#ifdef USE_CJSON
    cJSON *json = config_to_json(config);
    if (json)
    {
        char *json_str = cJSON_Print(json);
        if (json_str)
        {
            if (fputs(json_str, file) >= 0)
            {
                success = true;
            }
            free(json_str);
        }
        cJSON_Delete(json);
    }
#else
    json_t *json = config_to_json(config);
    if (json)
    {
        if (json_dumpf(json, file, JSON_INDENT(2)) == 0)
        {
            success = true;
        }
        json_decref(json);
    }
#endif

    fclose(file);
    free(path);

    if (success)
    {
        if (chmod(path, 0600) != 0)
        {
            log_warn("Failed to set restrictive permissions on config file");
        }

        if (current_config && current_config != config)
        {
            config_free(current_config);
        }
        current_config = config;
    }

    return success;
}

char *
config_get_value(const char *key)
{
    if (!key)
    {
        return NULL;
    }

    hostman_config_t *config = config_load();
    if (!config)
    {
        return NULL;
    }

    char *value = NULL;

    if (strcmp(key, "version") == 0)
    {
        value = malloc(16);
        snprintf(value, 16, "%d", config->version);
    }
    else if (strcmp(key, "default_host") == 0)
    {
        if (config->default_host)
        {
            value = strdup(config->default_host);
        }
    }
    else if (strcmp(key, "log_level") == 0)
    {
        if (config->log_level)
        {
            value = strdup(config->log_level);
        }
    }
    else if (strcmp(key, "log_file") == 0)
    {
        if (config->log_file)
        {
            value = strdup(config->log_file);
        }
    }
    else
    {
        if (strncmp(key, "hosts.", 6) == 0)
        {
            const char *host_key = key + 6;
            char *dot = strchr(host_key, '.');
            if (dot)
            {
                size_t host_name_len = dot - host_key;
                char *host_name = malloc(host_name_len + 1);
                strncpy(host_name, host_key, host_name_len);
                host_name[host_name_len] = '\0';

                host_config_t *host = NULL;
                for (int i = 0; i < config->host_count; i++)
                {
                    if (config->hosts[i] && strcmp(config->hosts[i]->name, host_name) == 0)
                    {
                        host = config->hosts[i];
                        break;
                    }
                }

                if (host)
                {
                    const char *prop = dot + 1;
                    if (strcmp(prop, "api_endpoint") == 0)
                    {
                        if (host->api_endpoint)
                        {
                            value = strdup(host->api_endpoint);
                        }
                    }
                    else if (strcmp(prop, "auth_type") == 0)
                    {
                        if (host->auth_type)
                        {
                            value = strdup(host->auth_type);
                        }
                    }
                    else if (strcmp(prop, "api_key_name") == 0)
                    {
                        if (host->api_key_name)
                        {
                            value = strdup(host->api_key_name);
                        }
                    }
                    else if (strcmp(prop, "request_body_format") == 0)
                    {
                        if (host->request_body_format)
                        {
                            value = strdup(host->request_body_format);
                        }
                    }
                    else if (strcmp(prop, "file_form_field") == 0)
                    {
                        if (host->file_form_field)
                        {
                            value = strdup(host->file_form_field);
                        }
                    }
                    else if (strcmp(prop, "response_url_json_path") == 0)
                    {
                        if (host->response_url_json_path)
                        {
                            value = strdup(host->response_url_json_path);
                        }
                    }
                }

                free(host_name);
            }
        }
    }

    return value;
}

bool
config_set_value(const char *key, const char *value)
{
    if (!key || !value)
    {
        return false;
    }

    hostman_config_t *config = config_load();
    if (!config)
    {
        config = calloc(1, sizeof(hostman_config_t));
        if (!config)
        {
            return false;
        }
        config->version = 1;
        config->log_level = strdup("INFO");

        char *cache_dir = get_cache_dir();
        if (cache_dir)
        {
            size_t len = strlen(cache_dir) + strlen("/hostman.log") + 1;
            config->log_file = malloc(len);
            if (config->log_file)
            {
                snprintf(config->log_file, len, "%s/hostman.log", cache_dir);
            }
            free(cache_dir);
        }
    }

    bool changed = false;

    if (strcmp(key, "version") == 0)
    {
        int version = atoi(value);
        if (version > 0)
        {
            config->version = version;
            changed = true;
        }
    }
    else if (strcmp(key, "default_host") == 0)
    {
        bool host_exists = false;
        for (int i = 0; i < config->host_count; i++)
        {
            if (config->hosts[i] && strcmp(config->hosts[i]->name, value) == 0)
            {
                host_exists = true;
                break;
            }
        }

        if (host_exists)
        {
            free(config->default_host);
            config->default_host = strdup(value);
            changed = true;
        }
        else
        {
            log_error("Host '%s' does not exist", value);
        }
    }
    else if (strcmp(key, "log_level") == 0)
    {
        if (strcmp(value, "DEBUG") == 0 || strcmp(value, "INFO") == 0 ||
            strcmp(value, "WARN") == 0 || strcmp(value, "ERROR") == 0)
        {
            free(config->log_level);
            config->log_level = strdup(value);
            changed = true;
        }
        else
        {
            log_error("Invalid log level: %s", value);
        }
    }
    else if (strcmp(key, "log_file") == 0)
    {
        free(config->log_file);
        config->log_file = strdup(value);
        changed = true;
    }
    else
    {
        if (strncmp(key, "hosts.", 6) == 0)
        {
            const char *host_key = key + 6;
            char *dot = strchr(host_key, '.');
            if (dot)
            {
                size_t host_name_len = dot - host_key;
                char *host_name = malloc(host_name_len + 1);
                strncpy(host_name, host_key, host_name_len);
                host_name[host_name_len] = '\0';

                host_config_t *host = NULL;
                for (int i = 0; i < config->host_count; i++)
                {
                    if (config->hosts[i] && strcmp(config->hosts[i]->name, host_name) == 0)
                    {
                        host = config->hosts[i];
                        break;
                    }
                }

                if (host)
                {
                    const char *prop = dot + 1;
                    if (strcmp(prop, "api_endpoint") == 0)
                    {
                        free(host->api_endpoint);
                        host->api_endpoint = strdup(value);
                        changed = true;
                    }
                    else if (strcmp(prop, "auth_type") == 0)
                    {
                        free(host->auth_type);
                        host->auth_type = strdup(value);
                        changed = true;
                    }
                    else if (strcmp(prop, "api_key_name") == 0)
                    {
                        free(host->api_key_name);
                        host->api_key_name = strdup(value);
                        changed = true;
                    }
                    else if (strcmp(prop, "request_body_format") == 0)
                    {
                        free(host->request_body_format);
                        host->request_body_format = strdup(value);
                        changed = true;
                    }
                    else if (strcmp(prop, "file_form_field") == 0)
                    {
                        free(host->file_form_field);
                        host->file_form_field = strdup(value);
                        changed = true;
                    }
                    else if (strcmp(prop, "response_url_json_path") == 0)
                    {
                        free(host->response_url_json_path);
                        host->response_url_json_path = strdup(value);
                        changed = true;
                    }
                }
                else
                {
                    log_error("Host '%s' does not exist", host_name);
                }

                free(host_name);
            }
        }
    }

    if (changed)
    {
        if (!config_save(config))
        {
            log_error("Failed to save configuration after setting value");
            return false;
        }
    }

    return changed;
}

bool
config_add_host(host_config_t *host)
{
    if (!host || !host->name)
    {
        return false;
    }

    hostman_config_t *config = config_load();
    if (!config)
    {
        config = calloc(1, sizeof(hostman_config_t));
        if (!config)
        {
            return false;
        }
        config->version = 1;
        config->log_level = strdup("INFO");

        char *cache_dir = get_cache_dir();
        if (cache_dir)
        {
            size_t len = strlen(cache_dir) + strlen("/hostman.log") + 1;
            config->log_file = malloc(len);
            if (config->log_file)
            {
                snprintf(config->log_file, len, "%s/hostman.log", cache_dir);
            }
            free(cache_dir);
        }
    }

    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i] && strcmp(config->hosts[i]->name, host->name) == 0)
        {
            log_error("Host '%s' already exists", host->name);
            return false;
        }
    }

    host_config_t **new_hosts =
      realloc(config->hosts, (config->host_count + 1) * sizeof(host_config_t *));
    if (!new_hosts)
    {
        log_error("Failed to allocate memory for new host");
        return false;
    }

    config->hosts = new_hosts;
    config->hosts[config->host_count] = host;
    config->host_count++;

    if (config->host_count == 1 && !config->default_host)
    {
        config->default_host = strdup(host->name);
    }

    return config_save(config);
}

bool
config_remove_host(const char *host_name)
{
    if (!host_name)
    {
        return false;
    }

    hostman_config_t *config = config_load();
    if (!config)
    {
        return false;
    }

    bool found = false;
    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i] && strcmp(config->hosts[i]->name, host_name) == 0)
        {
            free(config->hosts[i]->name);
            free(config->hosts[i]->api_endpoint);
            free(config->hosts[i]->auth_type);
            free(config->hosts[i]->api_key_name);
            free(config->hosts[i]->api_key_encrypted);
            free(config->hosts[i]->request_body_format);
            free(config->hosts[i]->file_form_field);
            free(config->hosts[i]->response_url_json_path);

            if (config->hosts[i]->static_field_count > 0)
            {
                for (int j = 0; j < config->hosts[i]->static_field_count; j++)
                {
                    free(config->hosts[i]->static_field_names[j]);
                    free(config->hosts[i]->static_field_values[j]);
                }
                free(config->hosts[i]->static_field_names);
                free(config->hosts[i]->static_field_values);
            }

            free(config->hosts[i]);

            for (int j = i; j < config->host_count - 1; j++)
            {
                config->hosts[j] = config->hosts[j + 1];
            }

            config->host_count--;
            found = true;
            break;
        }
    }

    if (!found)
    {
        log_error("Host '%s' not found", host_name);
        return false;
    }

    if (config->default_host && strcmp(config->default_host, host_name) == 0)
    {
        free(config->default_host);
        config->default_host = NULL;

        if (config->host_count > 0 && config->hosts[0])
        {
            config->default_host = strdup(config->hosts[0]->name);
        }
    }

    return config_save(config);
}

bool
config_set_default_host(const char *host_name)
{
    if (!host_name)
    {
        return false;
    }

    hostman_config_t *config = config_load();
    if (!config)
    {
        return false;
    }

    bool found = false;
    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i] && strcmp(config->hosts[i]->name, host_name) == 0)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        log_error("Host '%s' not found", host_name);
        return false;
    }

    free(config->default_host);
    config->default_host = strdup(host_name);

    return config_save(config);
}

host_config_t *
config_get_default_host(void)
{
    hostman_config_t *config = config_load();
    if (!config || !config->default_host)
    {
        return NULL;
    }

    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i] && strcmp(config->hosts[i]->name, config->default_host) == 0)
        {
            return config->hosts[i];
        }
    }

    return NULL;
}

host_config_t *
config_get_host(const char *host_name)
{
    if (!host_name)
    {
        return NULL;
    }

    hostman_config_t *config = config_load();
    if (!config)
    {
        return NULL;
    }

    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i] && strcmp(config->hosts[i]->name, host_name) == 0)
        {
            return config->hosts[i];
        }
    }

    return NULL;
}

void
config_free(hostman_config_t *config)
{
    if (!config)
    {
        return;
    }

    free(config->default_host);
    free(config->log_level);
    free(config->log_file);

    for (int i = 0; i < config->host_count; i++)
    {
        if (config->hosts[i])
        {
            free(config->hosts[i]->name);
            free(config->hosts[i]->api_endpoint);
            free(config->hosts[i]->auth_type);
            free(config->hosts[i]->api_key_name);
            free(config->hosts[i]->api_key_encrypted);
            free(config->hosts[i]->request_body_format);
            free(config->hosts[i]->file_form_field);
            free(config->hosts[i]->response_url_json_path);

            if (config->hosts[i]->static_field_count > 0)
            {
                for (int j = 0; j < config->hosts[i]->static_field_count; j++)
                {
                    free(config->hosts[i]->static_field_names[j]);
                    free(config->hosts[i]->static_field_values[j]);
                }
                free(config->hosts[i]->static_field_names);
                free(config->hosts[i]->static_field_values);
            }

            free(config->hosts[i]);
        }
    }

    free(config->hosts);
    free(config);

    if (current_config == config)
    {
        current_config = NULL;
    }
}
