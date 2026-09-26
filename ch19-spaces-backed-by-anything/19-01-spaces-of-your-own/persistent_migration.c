/* Purpose: migrate a durable schema in SQL. A fact stored through &old is
 *   renamed by one SQL transaction that also bumps the schema version, and
 *   the database reopened as &new holds only the new spelling.
 * Build: cc persistent_migration.c $(pkg-config --cflags --libs cmetta
 *   sqlite3)
 * Owns resources: both providers are closed and the database removed.
 * Guarantees: (old value) becomes (new value) with no alias left behind
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#if __has_include(<sqlite3.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "_fixtures/sqlite_store.h"
#include <unistd.h>

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

int main(void)
{
    /* The database is a scratch file in the temporary directory. */
    char path[PATH_MAX];
    const char *tmp = getenv("TMPDIR");
    snprintf(path, sizeof path, "%s/migration-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    int fd = mkstemp(path);
    require("create the database file", fd >= 0 && close(fd) == 0);
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    size_t released = 0;

    sql_store *store = sql_open(m, "&old", path, &released);
    mt_space *old = mt_space_open(m, "&old");
    require("open &old", old != NULL);
    require("store under the old schema", mt_add(old, E("old", "value")));
    require("migrate in one SQL transaction", sql_exec(store,
        "BEGIN; UPDATE facts SET atom='(new value)' WHERE atom='(old value)'; "
        "PRAGMA user_version=2; COMMIT") == MT_OK);
    mt_space_close(old);
    require("close &old", mt_provider_close(m, "&old"));

    sql_open(m, "&new", path, &released);
    mt_space *current = mt_space_open(m, "&new");
    require("open &new", current != NULL);
    assert(answers_are(mt_atoms(current), E(E("new", "value"))) && "the new schema reopens");
    assert(!mt_first(mt_match(current, E("old", V("x")))) && mt_ok() && "and the old spelling is gone");
    mt_space_close(current);
    require("close &new", mt_provider_close(m, "&new"));
    assert((int64_t)released == 2 && "both connections were released");
    require("remove the database", unlink(path) == 0);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without SQLite's headers the program only says what it needs. */
int main(void)
{
    fputs("persistent_migration.c needs SQLite: install its development files, then build with\n"
          "cc persistent_migration.c $(pkg-config --cflags --libs cmetta sqlite3)\n", stderr);
    return 77;
}
#endif
