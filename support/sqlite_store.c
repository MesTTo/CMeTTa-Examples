/* Purpose: a space whose atoms are SQLite rows, with nested transactions as
 *   savepoints, shared by the SQL examples.
 * Owns resources: a provider owns its connection; each cursor finalizes its
 *   statement on exhaustion, error or abandonment; release closes the database.
 * Guarded by: the examples use a provider from the thread that owns it.
 * Decides: rows keep bag multiplicity, and a match yields every row as a
 *   candidate, leaving unification to the engine.
 * text: a row stores an atom as its MeTTa source spelling, written by
 *   mt_write_dup() and read back by mt_parse(), which is what a text column
 *   holding atoms is.
 * Guarantees: rollback and retained-cursor cleanup are checked by
 *   integration/sqlite_space.c [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#include "sqlite_store.h"
typedef struct sql_cursor { sql_store *store; sqlite3_stmt *statement; } sql_cursor;

static mt_status failure(sql_store *store)
{ mt_error_set(MT_ERROR, sqlite3_errmsg(store->db)); return MT_ERROR; }

mt_status sql_exec(sql_store *store, const char *sql)
{ return sqlite3_exec(store->db, sql, NULL, NULL, NULL) == SQLITE_OK ? MT_OK : failure(store); }

static mt_status edit(sql_store *store, const char *sql, const mt_atom *atom)
{
    sqlite3_stmt *statement = NULL;
    mt_string text = mt_write_dup(atom);
    if (!text.data) return mt_error();
    int status = sqlite3_prepare_v2(store->db, sql, -1, &statement, NULL);
    if (status == SQLITE_OK) status = sqlite3_bind_text64(statement, 1, text.data, text.len, SQLITE_TRANSIENT, SQLITE_UTF8);
    mt_free(text.data);
    if (status == SQLITE_OK) status = sqlite3_step(statement);
    sqlite3_finalize(statement);
    return status == SQLITE_DONE ? MT_OK : failure(store);
}

static mt_status add(void *user, const mt_atom *atom)
{
    const char *head = mt_name(mt_at(atom, 0));
    if (head && strcmp(head, "system") == 0) {
        mt_error_set(MT_ERROR, "system facts are reserved"); return MT_ERROR;
    }
    return edit(user, "INSERT INTO facts(atom) VALUES(?)", atom);
}

static mt_status remove_one(void *user, const mt_atom *atom, bool *removed)
{
    sql_store *store = user;
    mt_status status = edit(store,
        "DELETE FROM facts WHERE rowid=(SELECT rowid FROM facts WHERE atom=? ORDER BY rowid LIMIT 1)", atom);
    *removed = status == MT_OK && sqlite3_changes(store->db) == 1;
    return status;
}

static mt_status next(void *user, mt_atom **out)
{
    sql_cursor *cursor = user;
    int status = sqlite3_step(cursor->statement);
    if (status == SQLITE_DONE) return MT_DONE;
    if (status != SQLITE_ROW) return failure(cursor->store);
    *out = mt_parse((const char *)sqlite3_column_text(cursor->statement, 0));
    return *out ? MT_ROW : mt_error();
}

static void close_cursor(void *user)
{ sql_cursor *cursor = user; sqlite3_finalize(cursor->statement); free(cursor); }

/* O(N) candidates, O(1) cursor memory; the engine owns filtering and limits. */
static mt_status match(void *user, const mt_atom *pattern, size_t limit, mt_iterator *out)
{
    (void)pattern; (void)limit;
    sql_store *store = user;
    sql_cursor *cursor = calloc(1, sizeof(*cursor));
    if (!cursor) { mt_error_set(MT_NOMEM, "allocate SQL cursor"); return MT_NOMEM; }
    cursor->store = store;
    if (sqlite3_prepare_v2(store->db, "SELECT atom FROM facts ORDER BY rowid", -1,
                         &cursor->statement, NULL) != SQLITE_OK) {
        close_cursor(cursor); return failure(store);
    }
    *out = (mt_iterator){cursor, next, close_cursor}; return MT_OK;
}

static mt_status clear(void *user) { return sql_exec(user, "DELETE FROM facts"); }
static mt_status savepoint(sql_store *store, const char *verb, unsigned depth)
{
    char sql[96];
    int length = snprintf(sql, sizeof(sql), "%s cmetta_%u", verb, depth);
    if (length < 0 || (size_t)length >= sizeof(sql)) {
        mt_error_set(MT_ERROR, "savepoint name overflow"); return MT_ERROR;
    }
    return sql_exec(store, sql);
}
static mt_status begin(void *user)
{
    sql_store *store = user;
    mt_status status = savepoint(store, "SAVEPOINT", store->depth);
    if (status == MT_OK) ++store->depth;
    return status;
}
static mt_status commit(void *user)
{ sql_store *store = user; return savepoint(store, "RELEASE", --store->depth); }
static mt_status rollback(void *user)
{
    sql_store *store = user; --store->depth;
    mt_status status = savepoint(store, "ROLLBACK TO", store->depth);
    mt_status released = savepoint(store, "RELEASE", store->depth);
    return status == MT_OK ? released : status;
}
static void release(void *user)
{
    sql_store *store = user;
    require("close the database with no statement open", sqlite3_close(store->db) == SQLITE_OK);
    if (store->released) ++*store->released;
    free(store);
}

sql_store *sql_open(metta *m, const char *name, const char *path, size_t *released)
{
    sql_store *store = calloc(1, sizeof(*store));
    require("allocate the SQL provider", store != NULL);
    require("open the database", sqlite3_open(path, &store->db) == SQLITE_OK);
    store->released = released;
    require("create the bag table", sql_exec(store, "CREATE TABLE IF NOT EXISTS facts(atom TEXT NOT NULL)") == MT_OK);
    require("publish the SQL provider", mt_provider_open(m, name, (mt_provider){
        .user=store, .add=add, .remove=remove_one, .match=match, .clear=clear,
        .begin=begin, .commit=commit, .rollback=rollback, .release=release}));
    return store;
}
