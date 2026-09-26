/* Purpose: a durable space. &orders is a SQLite database in WAL mode behind
 *   the provider door; a standing query observes each committed order, the
 *   provider is closed, and a second provider over the same file reads the
 *   orders back.
 * Build: cc journaled_observed_store.c $(pkg-config --cflags --libs cmetta
 *   sqlite3)
 * Owns resources: both providers are closed and the database file removed.
 * Decides: SQLite owns journal recovery; the subscription sees only commits.
 * Guarantees: two committed orders raise two events and survive reopening
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
#include "../ch19-spaces-backed-by-anything/19-01-spaces-of-your-own/_fixtures/sqlite_store.h"
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

static mt_status store_two_orders(metta *m, void *user)
{
    (void)m;
    mt_space *orders = user;
    if (!mt_add(orders, E("Order", 1, 25)) || !mt_add(orders, E("Order", 2, 40)))
        return mt_error();
    return MT_OK;
}

static mt_status noticed(void *user, bool added, const mt_atom *order)
{
    if (added && mt_len(order) == 3) ++*(size_t *)user;
    return MT_OK;
}

int main(void)
{
    /* The database is a scratch file in the temporary directory. */
    char path[PATH_MAX];
    const char *tmp = getenv("TMPDIR");
    snprintf(path, sizeof path, "%s/orders-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    int fd = mkstemp(path);
    require("create the database file", fd >= 0 && close(fd) == 0);
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    size_t released = 0, events = 0;

    sql_store *store = sql_open(m, "&orders", path, &released);
    require("journal in WAL mode",
            sql_exec(store, "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL") == MT_OK);
    mt_space *orders = mt_space_open(m, "&orders");
    require("open &orders", orders != NULL);
    require("declare ordered per-write events",
            mt_add(mt_catalog(m), E("events", mt_spaceref("&orders"), "per-write-exactly", "ordered")));
    require("watch orders", mt_subscribe(m, "orders", (mt_subscription){
        .space = "&orders", .pattern = E("Order", V("id"), V("total")),
        .notify = noticed, .user = &events }));

    require("commit two orders", mt_transaction(m, store_two_orders, orders) == MT_OK);
    assert((int64_t)events == 2 && "two committed orders, two events");
    require("stop watching", mt_unsubscribe(m, "orders"));
    mt_space_close(orders);
    require("close the provider", mt_provider_close(m, "&orders"));

    sql_open(m, "&reopened", path, &released);
    mt_space *reopened = mt_space_open(m, "&reopened");
    require("open &reopened", reopened != NULL);
    assert(answers_are(mt_atoms(reopened), E(E("Order", 1, 25), E("Order", 2, 40)))
           && "the orders survive reopening");
    mt_space_close(reopened);
    require("close the second provider", mt_provider_close(m, "&reopened"));
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
    fputs("journaled_observed_store.c needs SQLite: install its development files, then build with\n"
          "cc journaled_observed_store.c $(pkg-config --cflags --libs cmetta sqlite3)\n", stderr);
    return 77;
}
#endif
