/* Purpose: the SQLite-backed space the SQL examples share.
 * Owns resources: sql_open() hands the connection to mt_provider_open();
 *   sql_exec() borrows it until the provider is closed.
 */
#ifndef SQL_STORE_H
#define SQL_STORE_H
#include "common.h"
#include <sqlite3.h>
typedef struct sql_store { sqlite3 *db; unsigned depth; size_t *released; } sql_store;
sql_store *sql_open(metta *m, const char *name, const char *path, size_t *released);
mt_status sql_exec(sql_store *store, const char *sql);
#endif
