#include "hosts.h"
#include "config.h"
#include "encryption.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH 512

static char *
read_input(const char *prompt, bool required)
{
    char buffer[MAX_INPUT_LENGTH];
    char *result = NULL;

    do
    {
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            return NULL;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strlen(buffer) == 0)
        {
            if (!required)
            {
                return NULL;
            }
            printf("This field is required. Please try again.\n");
            continue;
        }

        result = strdup(buffer);
        if (!result)
        {
            log_error("Failed to allocate memory for input");
            return NULL;
        }

        break;
    } while (1);

    return result;
}

static char *
read_input_default(const char *prompt, const char *default_value)
{
    char full_prompt[MAX_INPUT_LENGTH * 2];
    snprintf(full_prompt, sizeof(full_prompt), "%s [%s]: ", prompt, default_value);

    char *input = read_input(full_prompt, false);
    if (!input)
    {
        return strdup(default_value);
    }

    return input;
}

int
hosts_add_interactive(void)
{
    printf("Adding a new host configuration...\n");

    char *name = read_input("Host name (unique identifier): ", true);
    if (!name)
    {
        fprintf(stderr, "Error: Failed to read host name\n");
        return EXIT_FAILURE;
    }

    host_config_t *existing = config_get_host(name);
    if (existing)
    {
        fprintf(stderr, "Error: A host with name '%s' already exists\n", name);
        free(name);
        return EXIT_FAILURE;
    }

    char *api_endpoint = read_input("API endpoint URL: ", true);
    if (!api_endpoint)
    {
        fprintf(stderr, "Error: Failed to read API endpoint\n");
        free(name);
        return EXIT_FAILURE;
    }

    printf("Authentication types:\n");
    printf("  1. Bearer token (Authorization: Bearer YOUR_TOKEN)\n");
    printf("  2. API key in header (Custom-Header: YOUR_KEY)\n");
    printf("  3. API key in URL parameter (?api_key=YOUR_KEY)\n");

    char *auth_type_input = read_input("Select authentication type [1]: ", false);
    char *auth_type = NULL;

    if (!auth_type_input || strcmp(auth_type_input, "1") == 0)
    {
        auth_type = strdup("bearer");
    }
    else if (strcmp(auth_type_input, "2") == 0)
    {
        auth_type = strdup("header");
    }
    else if (strcmp(auth_type_input, "3") == 0)
    {
        auth_type = strdup("param");
    }
    else
    {
        fprintf(stderr, "Error: Invalid authentication type\n");
        free(name);
        free(api_endpoint);
        free(auth_type_input);
        return EXIT_FAILURE;
    }

    free(auth_type_input);

    char *api_key_name = NULL;
    char default_key_name[64] = "Authorization";

    if (strcmp(auth_type, "bearer") == 0)
    {
        strcpy(default_key_name, "Authorization");
    }
    else if (strcmp(auth_type, "header") == 0)
    {
        strcpy(default_key_name, "X-API-Key");
    }
    else
    {
        strcpy(default_key_name, "api_key");
    }

    api_key_name = read_input_default("API key header/parameter name", default_key_name);

    char *api_key = read_input("API key or token: ", true);
    if (!api_key)
    {
        fprintf(stderr, "Error: Failed to read API key\n");
        free(name);
        free(api_endpoint);
        free(auth_type);
        free(api_key_name);
        return EXIT_FAILURE;
    }

    char *request_body_format = read_input_default("Request body format", "multipart");
    char *file_form_field = read_input_default("File form field name", "file");
    char *response_url_json_path = read_input_default("JSON path to URL in response", "url");
    char *response_deletion_url_json_path =
      read_input_default("JSON path to deletion URL in response", "deletion_url");
    char **static_field_names = NULL;
    char **static_field_values = NULL;
    int static_field_count = 0;

    printf("Do you want to add static form fields? [y/N]: ");
    char yn_buffer[10];
    if (fgets(yn_buffer, sizeof(yn_buffer), stdin) != NULL)
    {
        yn_buffer[strcspn(yn_buffer, "\n")] = 0;
        if (strcasecmp(yn_buffer, "y") == 0 || strcasecmp(yn_buffer, "yes") == 0)
        {
            printf("Enter static form fields (empty name to finish):\n");

            while (1)
            {
                char field_prompt[64];
                snprintf(
                  field_prompt, sizeof(field_prompt), "Field #%d name: ", static_field_count + 1);

                char *field_name = read_input(field_prompt, false);
                if (!field_name || strlen(field_name) == 0)
                {
                    free(field_name);
                    break;
                }

                snprintf(
                  field_prompt, sizeof(field_prompt), "Field #%d value: ", static_field_count + 1);
                char *field_value = read_input(field_prompt, true);
                if (!field_value)
                {
                    free(field_name);
                    break;
                }

                char **new_names =
                  realloc(static_field_names, (static_field_count + 1) * sizeof(char *));
                char **new_values =
                  realloc(static_field_values, (static_field_count + 1) * sizeof(char *));

                if (!new_names || !new_values)
                {
                    log_error("Failed to allocate memory for static fields");
                    free(field_name);
                    free(field_value);
                    break;
                }

                static_field_names = new_names;
                static_field_values = new_values;

                static_field_names[static_field_count] = field_name;
                static_field_values[static_field_count] = field_value;
                static_field_count++;
            }
        }
    }

    bool result = hosts_add(name,
                            api_endpoint,
                            auth_type,
                            api_key_name,
                            api_key,
                            request_body_format,
                            file_form_field,
                            response_url_json_path,
                            response_deletion_url_json_path,
                            static_field_names,
                            static_field_values,
                            static_field_count);

    hostman_config_t *config = config_load();
    if (result && (!config->default_host || config->host_count == 1))
    {
        printf("Set this host as the default? [Y/n]: ");
        if (fgets(yn_buffer, sizeof(yn_buffer), stdin) != NULL)
        {
            yn_buffer[strcspn(yn_buffer, "\n")] = 0;
            if (strcasecmp(yn_buffer, "n") != 0 && strcasecmp(yn_buffer, "no") != 0)
            {
                config_set_default_host(name);
                printf("Host '%s' set as default.\n", name);
            }
        }
    }

    free(name);
    free(api_endpoint);
    free(auth_type);
    free(api_key_name);
    free(api_key);
    free(request_body_format);
    free(file_form_field);
    free(response_url_json_path);
    free(response_deletion_url_json_path);

    for (int i = 0; i < static_field_count; i++)
    {
        free(static_field_names[i]);
        free(static_field_values[i]);
    }
    free(static_field_names);
    free(static_field_values);

    if (result)
    {
        printf("Host configuration added successfully!\n");
        return EXIT_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Error: Failed to add host configuration\n");
        return EXIT_FAILURE;
    }
}

bool
hosts_add(const char *name,
          const char *api_endpoint,
          const char *auth_type,
          const char *api_key_name,
          const char *api_key,
          const char *request_body_format,
          const char *file_form_field,
          const char *response_url_json_path,
          const char *response_deletion_url_json_path,
          char **static_field_names,
          char **static_field_values,
          int static_field_count)
{

    if (!name || !api_endpoint || !auth_type || !api_key_name || !api_key || !request_body_format ||
        !file_form_field || !response_url_json_path || !response_deletion_url_json_path)
    {
        log_error("Missing required host configuration fields");
        return false;
    }

    host_config_t *existing = config_get_host(name);
    if (existing)
    {
        log_error("Host '%s' already exists", name);
        return false;
    }

    char *encrypted_key = encryption_encrypt_api_key(api_key);
    if (!encrypted_key)
    {
        log_error("Failed to encrypt API key");
        return false;
    }

    host_config_t *host = calloc(1, sizeof(host_config_t));
    if (!host)
    {
        log_error("Failed to allocate memory for host configuration");
        free(encrypted_key);
        return false;
    }

    host->name = strdup(name);
    host->api_endpoint = strdup(api_endpoint);
    host->auth_type = strdup(auth_type);
    host->api_key_name = strdup(api_key_name);
    host->api_key_encrypted = encrypted_key;
    host->request_body_format = strdup(request_body_format);
    host->file_form_field = strdup(file_form_field);
    host->response_url_json_path = strdup(response_url_json_path);
    host->response_deletion_url_json_path = strdup(response_deletion_url_json_path);

    if (static_field_count > 0 && static_field_names && static_field_values)
    {
        host->static_field_count = static_field_count;
        host->static_field_names = calloc(static_field_count, sizeof(char *));
        host->static_field_values = calloc(static_field_count, sizeof(char *));

        if (!host->static_field_names || !host->static_field_values)
        {
            log_error("Failed to allocate memory for static fields");
            free(host->name);
            free(host->api_endpoint);
            free(host->auth_type);
            free(host->api_key_name);
            free(host->api_key_encrypted);
            free(host->request_body_format);
            free(host->file_form_field);
            free(host->response_url_json_path);
            free(host->response_deletion_url_json_path);
            free(host->static_field_names);
            free(host->static_field_values);
            free(host);
            return false;
        }

        for (int i = 0; i < static_field_count; i++)
        {
            host->static_field_names[i] = strdup(static_field_names[i]);
            host->static_field_values[i] = strdup(static_field_values[i]);
        }
    }

    bool result = config_add_host(host);
    if (!result)
    {
        log_error("Failed to add host to configuration");
        free(host->name);
        free(host->api_endpoint);
        free(host->auth_type);
        free(host->api_key_name);
        free(host->api_key_encrypted);
        free(host->request_body_format);
        free(host->file_form_field);
        free(host->response_url_json_path);
        free(host->response_deletion_url_json_path);

        for (int i = 0; i < host->static_field_count; i++)
        {
            free(host->static_field_names[i]);
            free(host->static_field_values[i]);
        }
        free(host->static_field_names);
        free(host->static_field_values);
        free(host);
    }

    return result;
}
