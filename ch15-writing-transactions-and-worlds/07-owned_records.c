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
 *   writes checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    ledger l = { .space = mt_space_open(m, "&ledger") };
    require("open &ledger", l.space != NULL);
    l.ref = mt_spaceref(mt_space_name(l.space));
    mt_atom *account = E("Account", 1);

    assert(mt_add(mt_catalog(m), record(&l, V("account"))) && "one balance per account");
    assert(mt_add(l.space, E("owned-by", mt_keep(account))) && "the account owns its record");
    assert(answers_are(read_record(m, &l, account), E(mt_unit())) && "an empty record reads ()");

    mt_atom *first = row(account, E("+", 5, 5));
    assert(mt_add(l.space, mt_keep(first)) && "a balance is written");
    assert(answers_are(read_record(m, &l, account), E(E(mt_keep(first)))) && "and reads as its whole row, as stored");

    mt_atom *replaced = row(account, N(12));
    assert(mt_transaction(m, apply, &(change){ &l, first, replaced }) == MT_OK && "a transaction replaces it");
    assert(answers_are(read_record(m, &l, account), E(E(mt_keep(replaced)))) && "and the record reads the new row");

    mt_atom *second = row(account, N(13));
    mt_clear();
    assert(mt_transaction(m, apply, &(change){ &l, NULL, second }) == MT_ERROR && "a second value cannot commit");
    mt_clear();
    assert(answers_are(read_record(m, &l, account), E(E(mt_keep(replaced)))) && "and the record keeps its row");

    mt_drop(first), mt_drop(replaced), mt_drop(second), mt_drop(account), mt_drop(l.ref);
    mt_space_close(l.space);
    mt_close(m);
    return 0;
}
