/* Purpose: a space that is a SQL table. &sql's atoms are rows; the engine
 *   unifies what the provider's cursor yields, so a repeated variable joins
 *   on equal fields and duplicates stay duplicates; a transaction that fails
 *   rolls the rows back through SQL savepoints; the provider refuses a
 *   reserved head; and a cursor still open keeps the connection alive after
 *   the provider is withdrawn.
 * Build: cc sqlite_space.c $(pkg-config --cflags --libs cmetta sqlite3)
 * Owns resources: the provider owns the SQLite connection.
 * Guarantees: each of those behaviours is checked against the SQL state
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<sqlite3.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>
#include "_fixtures/sqlite_store.h"

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

static mt_status abandoned(metta *m, void *user)
{
    (void)m;
    if (!mt_add((mt_space *)user, E("edge", "a", "z"))) return mt_error();
    return MT_FAIL;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    size_t released = 0;
    sql_open(m, "&sql", ":memory:", &released);
    mt_space *sql = mt_space_open(m, "&sql");
    require("open &sql", sql != NULL);
    require("a row", mt_add(sql, E("edge", "a", "b")));
    require("a loop", mt_add(sql, E("edge", "b", "b")));
    require("a duplicate row", mt_add(sql, E("edge", "a", "b")));

    assert(answers_are(mt_eval(sql, E("match", mt_spaceref("&sql"), E("edge", V("x"), V("x")), V("x"))), E("b"))
           && "a repeated variable joins on equal fields");
    assert(answers_are(mt_match(sql, E("edge", "a", "b")), E(E("edge", "a", "b"), E("edge", "a", "b")))
           && "duplicates stay duplicates");
    assert(mt_transaction(m, abandoned, sql) == MT_FAIL && "a failed transaction reports MT_FAIL");
    assert(!mt_first(mt_match(sql, E("edge", "a", "z"))) && mt_ok() && "and its row was rolled back");

    mt_clear();
    assert(!mt_add(sql, E("system", "private")) && "the provider refuses a reserved head");
    assert(mt_errmsg() && strstr(mt_errmsg(), "system facts are reserved") && "in its own words");
    mt_clear();
    require("remove one occurrence", mt_del(sql, E("edge", "a", "b")));
    assert(answers_are(mt_match(sql, E("edge", "a", "b")), E(E("edge", "a", "b"))) && "the duplicate survives");

    mt_answers *rows = mt_atoms(sql);
    require("hold a cursor open", rows != NULL && mt_next(rows) != NULL);
    require("withdraw the provider", mt_provider_close(m, "&sql"));
    assert((int64_t)released == 0 && "the open cursor keeps the connection");
    mt_answers_free(rows);
    assert((int64_t)released == 1 && "closing it releases the connection");
    mt_space_close(sql);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without SQLite's headers the program only says what it needs. */
int main(void)
{
    fputs("sqlite_space.c needs SQLite: install its development files, then build with\n"
          "cc sqlite_space.c $(pkg-config --cflags --libs cmetta sqlite3)\n", stderr);
    return 77;
}
#endif
