#ifndef HOSTMAN_UTILS_H
#define HOSTMAN_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#define HOSTMAN_VERSION "1.1.1"
#define HOSTMAN_BUILD_DATE __DATE__
#define HOSTMAN_BUILD_TIME __TIME__
#define HOSTMAN_AUTHOR "Keiran"
#define HOSTMAN_HOMEPAGE "https://github.com/q4ow/hostman"

char *
get_filename_from_path(const char *path);
void
format_file_size(size_t size, char *buffer, size_t buffer_size);
char *
get_config_dir(void);
char *
get_cache_dir(void);
char *
extract_json_string(const char *json, const char *path);

bool
copy_to_clipboard(const char *text);
const char *
get_clipboard_manager_name(void);

void
print_version_info(void);

#endif