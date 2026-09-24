/* Purpose: an entity is a handle around one of the engine's own tokens, its
 *   fields facts in the class space &Account. Each method is a C function
 *   over the handle, published under the class's prefix. The constructor
 *   runs in mt_transaction, has the engine mint the token as the owner row's
 *   own and writes the field facts; the accessor reads the balance fact; the
 *   writer replaces it in mt_transaction; withdraw refuses an overdraft, so
 *   the mt_transaction around it rolls back and the balance stands. C keeps
 *   its own model of every account's balance, applying each method's rule to
 *   it, and the engine's facts answer the model: each balance read, and the
 *   population, one query over the class space, sorted with qsort.
 * Guarantees: all six claims of the original hold, with its two unasserted
 *   constructions checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct account_class {
    metta *m;
    mt_space *space;
} account_class;

static account_class accounts;

static mt_atom *balance_fact(const mt_atom *handle, mt_atom *n) { return E("_field-balance", mt_keep(handle), n); }

/* Account-balance: the handle's balance fact, or -1 with no such fact. */
static int64_t balance_of(const mt_atom *handle)
{
    mt_atom *fact = mt_first(mt_match(accounts.space, balance_fact(handle, V("n"))));
    int64_t n = fact ? mt_int(mt_at(fact, 2)) : -1;
    mt_drop(fact);
    return n;
}

/* Account-balance!: the old fact goes and the new one lands together. */
typedef struct balance_write {
    const mt_atom *handle;
    int64_t n;
} balance_write;

static mt_status replace_balance(metta *m, void *user)
{
    (void)m;
    const balance_write *w = user;
    if (!mt_del(accounts.space, balance_fact(w->handle, V("old")))) return mt_ok() ? MT_FAIL : mt_error();
    return mt_add(accounts.space, balance_fact(w->handle, N(w->n))) ? MT_OK : mt_error();
}

static mt_status set_balance(const mt_atom *handle, int64_t n)
{
    return mt_transaction(accounts.m, replace_balance, &(balance_write){ handle, n });
}

static mt_status deposit(const mt_atom *handle, int64_t n) { return set_balance(handle, balance_of(handle) + n); }

static mt_status withdraw(const mt_atom *handle, int64_t n)
{
    int64_t held = balance_of(handle);
    if (held < n) return mt_error_set(MT_ERROR, "overdraft");
    return set_balance(handle, held - n);
}

/* make-Account: the engine mints the token as the owner row's own. */
typedef struct creation {
    const mt_atom *owner;
    int64_t balance;
    mt_atom *handle;
} creation;

static mt_status create(metta *m, void *user)
{
    creation *c = user;
    mt_atom *owned = E("owned-by", E("Account", V("id")));
    mt_atom *token = mt_first(mt_eval(m, E("progn", E("add-atom", mt_spaceref(mt_space_name(accounts.space)), owned, V("id")), V("id"))));
    if (!token) return mt_ok() ? MT_FAIL : mt_error();
    c->handle = E("Account", token);
    bool written = mt_add(accounts.space, E("_field-owner", mt_keep(c->handle), mt_keep(c->owner))) &&
                   mt_add(accounts.space, balance_fact(c->handle, N(c->balance)));
    return written ? MT_OK : mt_error();
}

static mt_atom *make_account(const mt_atom *owner, int64_t balance)
{
    creation c = { owner, balance, NULL };
    if (mt_transaction(accounts.m, create, &c) != MT_OK) {
        mt_drop(c.handle);
        return NULL;
    }
    return c.handle;
}

static mt_status answer_status(mt_call *call, mt_status s) { return s == MT_OK ? mt_answer(call, B(true)) : s; }

typedef enum { MAKE, BALANCE, SET_BALANCE, DEPOSIT, WITHDRAW } method;

static mt_status call_method(mt_call *call, void *user)
{
    switch ((method)(intptr_t)user) {
    case MAKE: {
        mt_atom *handle = make_account(mt_arg(call, 0), mt_int(mt_arg(call, 1)));
        return handle ? mt_answer(call, handle) : mt_error();
    }
    case BALANCE: {
        int64_t n = balance_of(mt_arg(call, 0));
        return n < 0 ? MT_FAIL : mt_answer(call, N(n));
    }
    case SET_BALANCE: return answer_status(call, set_balance(mt_arg(call, 0), mt_int(mt_arg(call, 1))));
    case DEPOSIT: return answer_status(call, deposit(mt_arg(call, 0), mt_int(mt_arg(call, 1))));
    case WITHDRAW: break;
    }
    return answer_status(call, withdraw(mt_arg(call, 0), mt_int(mt_arg(call, 1))));
}

/* C's model of one account: what the class's rules say its balance is. */
typedef struct model {
    const char *owner;
    int64_t balance;
    mt_atom *handle;
} model;

static int by_value(const void *a, const void *b) { return (*(const int64_t *)a > *(const int64_t *)b) - (*(const int64_t *)a < *(const int64_t *)b); }

typedef struct overdraft {
    const mt_atom *handle;
    int64_t n;
} overdraft;

static mt_status withdraw_body(metta *m, void *user)
{
    (void)m;
    const overdraft *o = user;
    return withdraw(o->handle, o->n);
}

int main(void)
{
    metta *m = open_engine();
    accounts = (account_class){ m, mt_space_open(m, "&Account") };
    require("open &Account", accounts.space != NULL);
    require("(: Account (-> Atom Account))", mt_add(accounts.space, E(":", "Account", E("->", "Atom", "Account"))));
    require("the internal heads", mt_add(accounts.space, E("internal", "owned-by", "_field-owner", "_field-balance")));
    const struct { const char *name; size_t arity; method method; enum mt_effect_class effect; } methods[] = {
        { "make-Account", 2, MAKE, MT_EFFECT_CLASS_WRITES_STATE },          { "Account-balance", 1, BALANCE, MT_EFFECT_CLASS_READ_ONLY_LOOKUP },
        { "Account-balance!", 2, SET_BALANCE, MT_EFFECT_CLASS_WRITES_STATE }, { "Account-deposit", 2, DEPOSIT, MT_EFFECT_CLASS_WRITES_STATE },
        { "Account-withdraw", 2, WITHDRAW, MT_EFFECT_CLASS_WRITES_STATE },
    };
    for (size_t i = 0; i < sizeof methods / sizeof *methods; i++)
        require(methods[i].name, mt_def(m, (mt_op){ .name = methods[i].name, .arity = methods[i].arity, .effect = methods[i].effect,
                                                    .fn = call_method, .user = (void *)(intptr_t)methods[i].method }));
    require("(from &Account)", mt_add(m, E("from", mt_spaceref(mt_space_name(accounts.space)))));

    model alice = { "alice", 40, NULL }, bob = { "bob", 80, NULL };
    model *all[] = { &alice, &bob };
    const size_t population = sizeof all / sizeof *all;
    for (size_t i = 0; i < population; i++) {
        all[i]->handle = mt_first(mt_eval(m, E("make-Account", all[i]->owner, all[i]->balance)));
        check("make-Account answers a handle", all[i]->handle != NULL);
    }

    const int64_t deposited = 25, overdrawn = 500, withdrawn = 30;
    check_answers("a deposit", mt_eval(m, E("Account-deposit", mt_keep(alice.handle), deposited)), B(true));
    alice.balance += deposited;
    check_answers("lands in the balance", mt_eval(m, E("Account-balance", mt_keep(alice.handle))), N(alice.balance));
    mt_clear();
    check("an overdraft refuses, and the transaction rolls back",
          alice.balance < overdrawn && mt_transaction(m, withdraw_body, &(overdraft){ alice.handle, overdrawn }) == MT_ERROR);
    mt_clear();
    check_answers("so the balance stands", mt_eval(m, E("Account-balance", mt_keep(alice.handle))), N(alice.balance));
    check_answers("a withdrawal within the balance", mt_eval(m, E("Account-withdraw", mt_keep(bob.handle), withdrawn)), B(true));
    bob.balance -= withdrawn;

    mt_list facts = mt_all(mt_match(accounts.space, E("_field-balance", V("account"), V("n"))));
    int64_t *held = malloc((facts.len + 1) * sizeof *held), *modelled = malloc(population * sizeof *modelled);
    require("room for the balances", held && modelled);
    for (size_t i = 0; i < facts.len; i++) held[i] = mt_int(mt_at(facts.items[i], 2));
    for (size_t i = 0; i < population; i++) modelled[i] = all[i]->balance;
    qsort(held, facts.len, sizeof *held, by_value);
    qsort(modelled, population, sizeof *modelled, by_value);
    check("the population is one query", facts.len == population && memcmp(held, modelled, population * sizeof *held) == 0);
    free(held), free(modelled);
    mt_list_free(facts);

    for (size_t i = 0; i < population; i++) mt_drop(all[i]->handle);
    mt_space_close(accounts.space);
    return done(m);
}
