#ifndef HOSTMAN_UTILS_H
#define HOSTMAN_UTILS_H

#include <stddef.h>
#include <stdbool.h>

char *get_filename_from_path(const char *path);
void format_file_size(size_t size, char *buffer, size_t buffer_size);
char *get_config_dir(void);
char *get_cache_dir(void);
char *extract_json_string(const char *json, const char *path);

bool copy_to_clipboard(const char *text);
const char *get_clipboard_manager_name(void);

#endif