/* Purpose: migrate a durable schema in SQL. A fact stored through &old is
 *   renamed by one SQL transaction that also bumps the schema version, and
 *   the database reopened as &new holds only the new spelling.
 * Owns resources: both providers are closed and the database removed.
 * Guarantees: (old value) becomes (new value) with no alias left behind
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "support/sqlite_store.h"
#include <unistd.h>

int main(void)
{
    char path[] = "ai-tmp/migration-XXXXXX";
    int fd = mkstemp(path);
    require("create the database file", fd >= 0 && close(fd) == 0);
    metta *m = open_engine();
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
    check_answers("the new schema reopens", mt_atoms(current), E("new", "value"));
    check_none("and the old spelling is gone", mt_match(current, E("old", V("x"))));
    mt_space_close(current);
    require("close &new", mt_provider_close(m, "&new"));
    check_int("both connections were released", (int64_t)released, 2);
    require("remove the database", unlink(path) == 0);
    return done(m);
}
