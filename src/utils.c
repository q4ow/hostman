#include "utils.h"
#include "logging.h"
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_CJSON
#include <cjson/cJSON.h>
#else
#include <jansson.h>
#endif

char *
get_filename_from_path(const char *path)
{
    if (!path)
    {
        return NULL;
    }

    const char *last_slash = strrchr(path, '/');
    if (last_slash)
    {
        return strdup(last_slash + 1);
    }
    else
    {
        return strdup(path);
    }
}

void
format_file_size(size_t size, char *buffer, size_t buffer_size)
{
    const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit_index = 0;
    double size_double = (double)size;

    while (size_double >= 1024.0 && unit_index < 4)
    {
        size_double /= 1024.0;
        unit_index++;
    }

    if (unit_index == 0)
    {
        snprintf(buffer, buffer_size, "%zu %s", size, units[unit_index]);
    }
    else
    {
        snprintf(buffer, buffer_size, "%.1f %s", size_double, units[unit_index]);
    }
}

char *
get_config_dir(void)
{
    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && *xdg_config)
    {
        size_t len = strlen(xdg_config) + strlen("/hostman") + 1;
        char *dir = malloc(len);
        if (dir)
        {
            snprintf(dir, len, "%s/hostman", xdg_config);
            return dir;
        }
    }

    const char *home = getenv("HOME");
    if (!home || !*home)
    {
        struct passwd *pwd = getpwuid(getuid());
        if (pwd)
        {
            home = pwd->pw_dir;
        }
        else
        {
            return NULL;
        }
    }

    size_t len = strlen(home) + strlen("/.config/hostman") + 1;
    char *dir = malloc(len);
    if (dir)
    {
        snprintf(dir, len, "%s/.config/hostman", home);
    }

    return dir;
}

char *
get_cache_dir(void)
{
    const char *xdg_cache = getenv("XDG_CACHE_HOME");
    if (xdg_cache && *xdg_cache)
    {
        size_t len = strlen(xdg_cache) + strlen("/hostman") + 1;
        char *dir = malloc(len);
        if (dir)
        {
            snprintf(dir, len, "%s/hostman", xdg_cache);
            return dir;
        }
    }

    const char *home = getenv("HOME");
    if (!home || !*home)
    {
        struct passwd *pwd = getpwuid(getuid());
        if (pwd)
        {
            home = pwd->pw_dir;
        }
        else
        {
            return NULL;
        }
    }

    size_t len = strlen(home) + strlen("/.cache/hostman") + 1;
    char *dir = malloc(len);
    if (dir)
    {
        snprintf(dir, len, "%s/.cache/hostman", home);
    }

    return dir;
}

char *
extract_json_string(const char *json, const char *path)
{
    if (!json || !path)
    {
        return NULL;
    }

    char *result = NULL;

#ifdef USE_CJSON
    cJSON *root = cJSON_Parse(json);
    if (!root)
    {
        log_error("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return NULL;
    }

    char *path_copy = strdup(path);
    char *token = strtok(path_copy, ".");
    cJSON *current = root;

    while (token && current)
    {
        current = cJSON_GetObjectItem(current, token);
        token = strtok(NULL, ".");
    }

    if (current && cJSON_IsString(current))
    {
        result = strdup(current->valuestring);
    }

    free(path_copy);
    cJSON_Delete(root);
#else
    json_error_t error;
    json_t *root = json_loads(json, 0, &error);
    if (!root)
    {
        log_error("Failed to parse JSON: %s", error.text);
        return NULL;
    }

    char *path_copy = strdup(path);
    char *token = strtok(path_copy, ".");
    json_t *current = root;

    while (token && current)
    {
        current = json_object_get(current, token);
        token = strtok(NULL, ".");
    }

    if (current && json_is_string(current))
    {
        result = strdup(json_string_value(current));
    }

    free(path_copy);
    json_decref(root);
#endif

    return result;
}

static const char *
detect_clipboard_manager(void)
{
    const char *managers[] = {
        "wl-copy",            // Wayland
        "xclip",              // X11
        "xsel",               // X11 alternative
        "pbcopy",             // macOS
        "clip.exe",           // Windows (i think wsl too)
        "fish_clipboard_copy" // Fish shell (im desperate)
    };

    static char command_found[32] = { 0 };

    if (command_found[0] != '\0')
    {
        return command_found;
    }

    for (size_t i = 0; i < sizeof(managers) / sizeof(managers[0]); i++)
    {
        char which_cmd[128];
        snprintf(which_cmd, sizeof(which_cmd), "which %s >/dev/null 2>&1", managers[i]);

        if (system(which_cmd) == 0)
        {
            strncpy(command_found, managers[i], sizeof(command_found) - 1);
            return command_found;
        }
    }

    return NULL;
}

const char *
get_clipboard_manager_name(void)
{
    return detect_clipboard_manager();
}

bool
copy_to_clipboard(const char *text)
{
    if (!text)
        return false;

    const char *clipboard_cmd = detect_clipboard_manager();
    if (!clipboard_cmd)
    {
        return false;
    }

    char cmd[1024];

    if (strcmp(clipboard_cmd, "wl-copy") == 0)
    {
        snprintf(cmd, sizeof(cmd), "echo -n '%s' | wl-copy", text);
    }
    else if (strcmp(clipboard_cmd, "xclip") == 0)
    {
        snprintf(cmd, sizeof(cmd), "echo -n '%s' | xclip -selection clipboard", text);
    }
    else if (strcmp(clipboard_cmd, "xsel") == 0)
    {
        snprintf(cmd, sizeof(cmd), "echo -n '%s' | xsel -ib", text);
    }
    else if (strcmp(clipboard_cmd, "pbcopy") == 0)
    {
        snprintf(cmd, sizeof(cmd), "echo -n '%s' | pbcopy", text);
    }
    else if (strcmp(clipboard_cmd, "clip.exe") == 0)
    {
        snprintf(cmd, sizeof(cmd), "echo -n '%s' | clip.exe", text);
    }
    else if (strcmp(clipboard_cmd, "fish_clipboard_copy") == 0)
    {
        snprintf(cmd, sizeof(cmd), "echo -n '%s' | fish_clipboard_copy", text);
    }
    else
    {
        return false;
    }

    return (system(cmd) == 0);
}

void
print_version_info(void)
{
    printf("\033[1;36mHOSTMAN %s\033[0m\n\n", HOSTMAN_VERSION);

    printf("\033[1;37mHostman\033[0m - A command-line image host manager\n\n");

    printf("\033[1;33mVersion:\033[0m     v%s\n", HOSTMAN_VERSION);
    printf("\033[1;33mBuilt on:\033[0m    %s\n", HOSTMAN_BUILD_DATE);
    printf("\033[1;33mBuilt at:\033[0m    %s\n", HOSTMAN_BUILD_TIME);

#ifdef __GNUC__
    printf("\033[1;33mCompiler:\033[0m    GCC/G++ %d.%d.%d\n",
           __GNUC__,
           __GNUC_MINOR__,
           __GNUC_PATCHLEVEL__);
#else
    printf("\033[1;33mCompiler:\033[0m    Unknown\n");
#endif

#ifdef __linux__
    printf("\033[1;33mPlatform:\033[0m    Linux\n");
#elif defined(_WIN32) || defined(_WIN64)
    printf("\033[1;33mPlatform:\033[0m    Windows\n");
#elif defined(__APPLE__) && defined(__MACH__)
    printf("\033[1;33mPlatform:\033[0m    macOS\n");
#else
    printf("\033[1;33mPlatform:\033[0m    Unknown\n");
#endif

    printf("\n\033[1;37mMaintained by:\033[0m  %s\n", HOSTMAN_AUTHOR);
    printf("\033[1;37mRepository:\033[0m     %s\n\n", HOSTMAN_HOMEPAGE);

    printf("\033[0;37mLicensed under MIT License.\033[0m\n");
}
