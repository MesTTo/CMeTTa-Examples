/* Purpose: a durable space. &orders is a SQLite database in WAL mode behind
 *   the provider door; a standing query observes each committed order, the
 *   provider is closed, and a second provider over the same file reads the
 *   orders back.
 * Owns resources: both providers are closed and the database file removed.
 * Decides: SQLite owns journal recovery; the subscription sees only commits.
 * Guarantees: two committed orders raise two events and survive reopening
 *   [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "support/sqlite_store.h"
#include <unistd.h>

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
    char path[] = "ai-tmp/orders-XXXXXX";
    int fd = mkstemp(path);
    require("create the database file", fd >= 0 && close(fd) == 0);
    metta *m = open_engine();
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
    check_int("two committed orders, two events", (int64_t)events, 2);
    require("stop watching", mt_unsubscribe(m, "orders"));
    mt_space_close(orders);
    require("close the provider", mt_provider_close(m, "&orders"));

    sql_open(m, "&reopened", path, &released);
    mt_space *reopened = mt_space_open(m, "&reopened");
    require("open &reopened", reopened != NULL);
    check_answers("the orders survive reopening", mt_atoms(reopened),
                  E("Order", 1, 25), E("Order", 2, 40));
    mt_space_close(reopened);
    require("close the second provider", mt_provider_close(m, "&reopened"));
    check_int("both connections were released", (int64_t)released, 2);
    require("remove the database", unlink(path) == 0);
    return done(m);
}
