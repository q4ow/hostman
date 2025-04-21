#include "database.h"
#include "logging.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static sqlite3 *db = NULL;

static char *db_get_path(void)
{
    char *cache_dir = get_cache_dir();
    if (!cache_dir)
    {
        return NULL;
    }

    size_t len = strlen(cache_dir) + strlen("/history.db") + 1;
    char *path = malloc(len);
    if (!path)
    {
        free(cache_dir);
        return NULL;
    }

    snprintf(path, len, "%s/history.db", cache_dir);
    free(cache_dir);

    return path;
}

bool db_init(void)
{
    if (db)
    {
        return true;
    }

    char *db_path = db_get_path();
    if (!db_path)
    {
        log_error("Failed to get database path");
        return false;
    }

    char *cache_dir = get_cache_dir();
    if (!cache_dir)
    {
        free(db_path);
        return false;
    }

    if (access(cache_dir, F_OK) != 0)
    {
        if (mkdir(cache_dir, 0755) != 0)
        {
            log_error("Failed to create cache directory: %s", cache_dir);
            free(cache_dir);
            free(db_path);
            return false;
        }
    }
    free(cache_dir);

    int result = sqlite3_open(db_path, &db);
    if (result != SQLITE_OK)
    {
        log_error("Failed to open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = NULL;
        free(db_path);
        return false;
    }

    free(db_path);

    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS uploads ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp INTEGER NOT NULL,"
        "host_name TEXT NOT NULL,"
        "local_path TEXT NOT NULL,"
        "remote_url TEXT UNIQUE NOT NULL,"
        "filename TEXT NOT NULL,"
        "size INTEGER NOT NULL"
        ");";

    char *error_msg = NULL;
    result = sqlite3_exec(db, create_table_sql, NULL, NULL, &error_msg);
    if (result != SQLITE_OK)
    {
        log_error("Failed to create table: %s", error_msg);
        sqlite3_free(error_msg);
        sqlite3_close(db);
        db = NULL;
        return false;
    }

    return true;
}

bool db_add_upload(const char *host_name, const char *local_path,
                   const char *remote_url, const char *filename, size_t size)
{
    if (!db && !db_init())
    {
        return false;
    }

    const char *sql = "INSERT INTO uploads (timestamp, host_name, local_path, remote_url, filename, size) "
                      "VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK)
    {
        log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return false;
    }

    time_t now = time(NULL);

    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, host_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, local_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, remote_url, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, filename, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, size);

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE)
    {
        if (result == SQLITE_CONSTRAINT)
        {
            log_warn("Upload already exists in database: %s", remote_url);
            return true;
        }
        else
        {
            log_error("Failed to insert upload: %s", sqlite3_errmsg(db));
            return false;
        }
    }

    log_info("Added upload to database: %s", remote_url);
    return true;
}

upload_record_t **db_get_uploads(const char *host_name, int page, int limit, int *count)
{
    *count = 0;

    if (!db && !db_init())
    {
        return NULL;
    }

    const char *sql;
    sqlite3_stmt *stmt;
    int result;

    if (host_name)
    {
        sql = "SELECT id, timestamp, host_name, local_path, remote_url, filename, size "
              "FROM uploads WHERE host_name = ? "
              "ORDER BY timestamp DESC LIMIT ? OFFSET ?;";

        result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (result != SQLITE_OK)
        {
            log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
            return NULL;
        }

        sqlite3_bind_text(stmt, 1, host_name, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, limit);
        sqlite3_bind_int(stmt, 3, (page - 1) * limit);
    }
    else
    {
        sql = "SELECT id, timestamp, host_name, local_path, remote_url, filename, size "
              "FROM uploads ORDER BY timestamp DESC LIMIT ? OFFSET ?;";

        result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (result != SQLITE_OK)
        {
            log_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
            return NULL;
        }

        sqlite3_bind_int(stmt, 1, limit);
        sqlite3_bind_int(stmt, 2, (page - 1) * limit);
    }

    upload_record_t **records = NULL;
    int capacity = 0;

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if (*count >= capacity)
        {
            capacity = capacity == 0 ? 10 : capacity * 2;
            upload_record_t **new_records = realloc(records, capacity * sizeof(upload_record_t *));
            if (!new_records)
            {
                log_error("Failed to allocate memory for upload records");
                for (int i = 0; i < *count; i++)
                {
                    free(records[i]->host_name);
                    free(records[i]->local_path);
                    free(records[i]->remote_url);
                    free(records[i]->filename);
                    free(records[i]);
                }
                free(records);
                sqlite3_finalize(stmt);
                return NULL;
            }
            records = new_records;
        }

        upload_record_t *record = malloc(sizeof(upload_record_t));
        if (!record)
        {
            log_error("Failed to allocate memory for upload record");
            for (int i = 0; i < *count; i++)
            {
                free(records[i]->host_name);
                free(records[i]->local_path);
                free(records[i]->remote_url);
                free(records[i]->filename);
                free(records[i]);
            }
            free(records);
            sqlite3_finalize(stmt);
            return NULL;
        }

        record->id = sqlite3_column_int(stmt, 0);
        record->timestamp = sqlite3_column_int64(stmt, 1);
        record->host_name = strdup((const char *)sqlite3_column_text(stmt, 2));
        record->local_path = strdup((const char *)sqlite3_column_text(stmt, 3));
        record->remote_url = strdup((const char *)sqlite3_column_text(stmt, 4));
        record->filename = strdup((const char *)sqlite3_column_text(stmt, 5));
        record->size = sqlite3_column_int64(stmt, 6);

        records[*count] = record;
        (*count)++;
    }

    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE)
    {
        log_error("Error retrieving uploads: %s", sqlite3_errmsg(db));
        for (int i = 0; i < *count; i++)
        {
            free(records[i]->host_name);
            free(records[i]->local_path);
            free(records[i]->remote_url);
            free(records[i]->filename);
            free(records[i]);
        }
        free(records);
        return NULL;
    }

    return records;
}

void db_free_records(upload_record_t **records, int count)
{
    if (!records)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        if (records[i])
        {
            free(records[i]->host_name);
            free(records[i]->local_path);
            free(records[i]->remote_url);
            free(records[i]->filename);
            free(records[i]);
        }
    }

    free(records);
}

void db_close(void)
{
    if (db)
    {
        sqlite3_close(db);
        db = NULL;
    }
}
