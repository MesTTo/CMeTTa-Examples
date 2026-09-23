/* Purpose: migrate a durable atom head once and reopen the resulting schema.
 * Owns resources: closes both providers and removes the temporary database.
 * Guarantees: reopening needs no alias [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "support/sqlite_store.h"
#include <unistd.h>
int main(void)
{
    char path[] = "ai-tmp/migration-XXXXXX";
    int fd = mkstemp(path); check("temporary database", fd >= 0 && close(fd) == 0);
    metta *m = open_engine(); size_t released = 0;
    sql_store *store = sql_open(m, "&old", path, &released);
    mt_space *space = mt_space_open(m, "&old");
    check("store old schema", mt_add(space, mt_expr("old", "value")));
    check("atomic schema migration", sql_exec(store,
        "BEGIN; UPDATE facts SET atom='(new value)' WHERE atom='(old value)'; PRAGMA user_version=2; COMMIT") == MT_OK);
    mt_space_close(space); check("close migrated database", mt_provider_close(m, "&old"));
    sql_open(m, "&new", path, &released); space = mt_space_open(m, "&new");
    check_answers("new schema reopens", mt_atoms(space), "(new value)");
    check_answers("old name is absent", mt_match(space, mt_parse("(old $x)")), "");
    mt_space_close(space); check("close current database", mt_provider_close(m, "&new"));
    check("both connections closed", released == 2); check("remove database", unlink(path) == 0);
    return done(m, "persistent_migration");
}
