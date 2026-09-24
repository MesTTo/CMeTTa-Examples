/* Purpose: a native owned record, declared and read back as data. The
 *   record's key is one C function of the account, so the declaration in
 *   &metta is that function of a variable and every read is that function of
 *   (Account 1). A read answers the rows as stored, so each expectation is
 *   the row C wrote, its (+ 5 5) as written: C compares data without
 *   evaluating it, which is what the original's (noeval ...) keeps test from
 *   doing. A replacement is an mt_transaction whose body removes the old row
 *   and adds the new one. A second value for the key is refused at the outer
 *   commit, so that mt_transaction answers MT_ERROR and the record keeps its
 *   row.
 * Guarantees: all five claims of the original hold, with its four unasserted
 *   writes checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct ledger {
    mt_space *space;
    mt_atom *ref;
} ledger;

/* (@owned-record &ledger account &ledger (balance account)): one balance per
   account, owned by the account's owned-by row in the same space. TAKES the
   account. */
static mt_atom *record(const ledger *l, mt_atom *account)
{
    return E("@owned-record", mt_keep(l->ref), mt_keep(account), mt_keep(l->ref), E("balance", account));
}

static mt_atom *row(const mt_atom *account, mt_atom *value) { return E("balance", mt_keep(account), value); }

static mt_answers *read_record(metta *m, const ledger *l, const mt_atom *account)
{
    return mt_eval(m, E("owned-record-read", record(l, mt_keep(account))));
}

/* A transaction's body: remove one row if there is one to remove, then add
   another. */
typedef struct change {
    const ledger *l;
    const mt_atom *removed, *added;
} change;

static mt_status apply(metta *m, void *user)
{
    (void)m;
    const change *c = user;
    if (c->removed && !mt_del(c->l->space, mt_keep(c->removed))) return mt_ok() ? MT_FAIL : mt_error();
    return mt_add(c->l->space, mt_keep(c->added)) ? MT_OK : mt_error();
}

int main(void)
{
    metta *m = open_engine();
    ledger l = { .space = mt_space_open(m, "&ledger") };
    require("open &ledger", l.space != NULL);
    l.ref = mt_spaceref(mt_space_name(l.space));
    mt_atom *account = E("Account", 1);

    check("one balance per account", mt_add(mt_catalog(m), record(&l, V("account"))));
    check("the account owns its record", mt_add(l.space, E("owned-by", mt_keep(account))));
    check_answers("an empty record reads ()", read_record(m, &l, account), mt_unit());

    mt_atom *first = row(account, E("+", 5, 5));
    check("a balance is written", mt_add(l.space, mt_keep(first)));
    check_answers("and reads as its whole row, as stored", read_record(m, &l, account), E(mt_keep(first)));

    mt_atom *replaced = row(account, N(12));
    check("a transaction replaces it", mt_transaction(m, apply, &(change){ &l, first, replaced }) == MT_OK);
    check_answers("and the record reads the new row", read_record(m, &l, account), E(mt_keep(replaced)));

    mt_atom *second = row(account, N(13));
    mt_clear();
    check("a second value cannot commit", mt_transaction(m, apply, &(change){ &l, NULL, second }) == MT_ERROR);
    mt_clear();
    check_answers("and the record keeps its row", read_record(m, &l, account), E(mt_keep(replaced)));

    mt_drop(first), mt_drop(replaced), mt_drop(second), mt_drop(account), mt_drop(l.ref);
    mt_space_close(l.space);
    return done(m);
}
