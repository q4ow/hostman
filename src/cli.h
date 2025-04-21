#ifndef HOSTMAN_CLI_H
#define HOSTMAN_CLI_H

#include <stdbool.h>

typedef enum
{
    CMD_UNKNOWN,
    CMD_UPLOAD,
    CMD_LIST_UPLOADS,
    CMD_LIST_HOSTS,
    CMD_ADD_HOST,
    CMD_REMOVE_HOST,
    CMD_SET_DEFAULT_HOST,
    CMD_CONFIG,
    CMD_HELP
} command_type_t;

typedef struct
{
    command_type_t type;
    char *host_name;
    char *file_path;
    int page;
    int limit;
    bool config_get;
    char *config_key;
    char *config_value;
    char *command_name;
} command_args_t;

command_args_t parse_args(int argc, char *argv[]);
int execute_command(command_args_t *args);
int run_setup_wizard(void);
void free_command_args(command_args_t *args);
void print_command_help(const char *command);

#endif
