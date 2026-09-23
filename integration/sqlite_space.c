/* Purpose: query a SQL bag, reject a reserved write and roll back a transaction.
 * Owns resources: provider owns SQLite; cursor retention delays its release.
 * Guarantees: actual SQL state and MeTTa answers agree [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "support/sqlite_store.h"
static mt_status abandoned(metta *m, void *user)
{
    (void)m;
    if (!mt_add((mt_space *)user, mt_expr("edge", "a", "z"))) return mt_error();
    return MT_FAIL;
}
int main(void)
{
    metta *m = open_engine(); size_t released = 0;
    sql_open(m, "&sql", ":memory:", &released);
    mt_space *space = mt_space_open(m, "&sql"); check("space handle", space != NULL);
    check("first row", mt_add(space, mt_expr("edge", "a", "b")));
    check("second row", mt_add(space, mt_expr("edge", "b", "b")));
    check("duplicate row", mt_add(space, mt_expr("edge", "a", "b")));
    check_answers("diagonal unification", mt_eval(space, mt_parse("(match &sql (edge $x $x) $x)")), "b");
    check_answers("SQL bag", mt_match(space, mt_parse("(edge a b)")), "(edge a b) (edge a b)");
    check("rollback status", mt_transaction(m, abandoned, space) == MT_FAIL);
    check_answers("rollback removed row", mt_match(space, mt_parse("(edge a z)")), "");
    check("policy rejects write", !mt_add(space, mt_expr("system", "private")));
    check("policy reason survives", mt_errmsg() && strstr(mt_errmsg(), "system facts are reserved")); mt_clear();
    check("remove one occurrence", mt_del(space, mt_expr("edge", "a", "b")));
    check_answers("duplicate survives", mt_match(space, mt_parse("(edge a b)")), "(edge a b)");
    mt_answers *rows = mt_atoms(space); check("open retained SQL cursor", rows != NULL && mt_next(rows));
    check("withdraw provider", mt_provider_close(m, "&sql"));
    check("cursor retains connection", released == 0);
    mt_answers_free(rows); check("abandoned cursor releases connection", released == 1);
    mt_space_close(space);
    return done(m, "sqlite_space");
}
