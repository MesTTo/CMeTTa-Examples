/* Purpose: a space that is a SQL table. &sql's atoms are rows; the engine
 *   unifies what the provider's cursor yields, so a repeated variable joins
 *   on equal fields and duplicates stay duplicates; a transaction that fails
 *   rolls the rows back through SQL savepoints; the provider refuses a
 *   reserved head; and a cursor still open keeps the connection alive after
 *   the provider is withdrawn.
 * Owns resources: the provider owns the SQLite connection.
 * Guarantees: each of those behaviours is checked against the SQL state
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "support/sqlite_store.h"

static mt_status abandoned(metta *m, void *user)
{
    (void)m;
    if (!mt_add((mt_space *)user, E("edge", "a", "z"))) return mt_error();
    return MT_FAIL;
}

int main(void)
{
    metta *m = open_engine();
    size_t released = 0;
    sql_open(m, "&sql", ":memory:", &released);
    mt_space *sql = mt_space_open(m, "&sql");
    require("open &sql", sql != NULL);
    require("a row", mt_add(sql, E("edge", "a", "b")));
    require("a loop", mt_add(sql, E("edge", "b", "b")));
    require("a duplicate row", mt_add(sql, E("edge", "a", "b")));

    check_answers("a repeated variable joins on equal fields",
                  mt_eval(sql, E("match", mt_spaceref("&sql"), E("edge", V("x"), V("x")), V("x"))), "b");
    check_answers("duplicates stay duplicates", mt_match(sql, E("edge", "a", "b")),
                  E("edge", "a", "b"), E("edge", "a", "b"));
    check("a failed transaction reports MT_FAIL", mt_transaction(m, abandoned, sql) == MT_FAIL);
    check_none("and its row was rolled back", mt_match(sql, E("edge", "a", "z")));

    mt_clear();
    check("the provider refuses a reserved head", !mt_add(sql, E("system", "private")));
    check("in its own words", mt_errmsg() && strstr(mt_errmsg(), "system facts are reserved"));
    mt_clear();
    require("remove one occurrence", mt_del(sql, E("edge", "a", "b")));
    check_answers("the duplicate survives", mt_match(sql, E("edge", "a", "b")), E("edge", "a", "b"));

    mt_answers *rows = mt_atoms(sql);
    require("hold a cursor open", rows != NULL && mt_next(rows) != NULL);
    require("withdraw the provider", mt_provider_close(m, "&sql"));
    check_int("the open cursor keeps the connection", (int64_t)released, 0);
    mt_answers_free(rows);
    check_int("closing it releases the connection", (int64_t)released, 1);
    mt_space_close(sql);
    return done(m);
}
