#ifndef HOSTMAN_DATABASE_H
#define HOSTMAN_DATABASE_H

#include <sqlite3.h>
#include <stdbool.h>
#include <time.h>

typedef struct
{
    int id;
    time_t timestamp;
    char *host_name;
    char *local_path;
    char *remote_url;
    char *deletion_url;
    char *filename;
    size_t size;
} upload_record_t;

bool
db_init(void);

bool
db_add_upload(const char *host_name,
              const char *local_path,
              const char *remote_url,
              const char *deletion_url,
              const char *filename,
              size_t size);

upload_record_t **
db_get_uploads(const char *host_name, int page, int limit, int *count);

void
db_free_records(upload_record_t **records, int count);

bool
db_delete_upload(int id);

void
db_close(void);

#endif
