/* Purpose: lib_spaces, held against C's own model of every space: a C array
 *   of atoms matched with mt_unify, cmetta's unification, which runs in C
 *   without the engine. The model keeps the engine's rules, measured on its
 *   spaces: a space is a multiset, so adding an atom twice holds it twice;
 *   remove-atom removes every atom unifying with its argument and answers
 *   True whether or not one was there; and a match reads the space as it
 *   stood when the match began, so an operation's own writes never feed it.
 *   Each bulk operation is then its library equation spelled over the model,
 *   answering once per atom it touched, and C compares the engine's collapsed
 *   answers with the model's.
 * Guarantees: all twenty-seven claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

enum { MOST = 16 };

/* C's picture of one space. */
typedef struct model {
    mt_atom *at[MOST];
    size_t n;
} model;

static void hold(model *s, mt_atom *atom)
{
    require("room in the model", s->n < MOST);
    s->at[s->n++] = atom;
}

/* remove-atom: every atom that unifies with `atom` goes. */
static void remove_all(model *s, const mt_atom *atom)
{
    size_t kept = 0;
    for (size_t i = 0; i < s->n; i++) {
        mt_bindings *b = mt_unify(atom, s->at[i]);
        if (b) mt_drop(s->at[i]);
        else s->at[kept++] = s->at[i];
        mt_bindings_free(b);
    }
    s->n = kept;
}

/* The instances of a pattern over the atoms the space held when the match
   began, in the space's order. */
static size_t instances(const model *s, const mt_atom *pattern, const mt_atom *shape, mt_atom **out)
{
    size_t n = 0;
    for (size_t i = 0; i < s->n; i++) {
        mt_bindings *b = mt_unify(pattern, s->at[i]);
        if (b) out[n++] = mt_substitute(shape, b);
        mt_bindings_free(b);
    }
    return n;
}

/* One answer per instance, each what `answer` makes of it. */
static mt_atom *answers(mt_atom **each, size_t n, mt_atom *(*answer)(mt_atom *))
{
    for (size_t i = 0; i < n; i++) each[i] = answer(each[i]);
    return mt_exprv(n, each);
}

static mt_atom *verdict(mt_atom *instance)
{
    mt_drop(instance);
    return B(true);
}

static mt_atom *itself(mt_atom *instance) { return instance; }

static int64_t count(const model *s, const mt_atom *pattern)
{
    mt_atom *each[MOST];
    size_t n = instances(s, pattern, pattern, each);
    for (size_t i = 0; i < n; i++) mt_drop(each[i]);
    return (int64_t)n;
}

/* (match $space $pattern $shape) */
static mt_atom *matched(const model *s, const mt_atom *pattern, const mt_atom *shape)
{
    mt_atom *each[MOST];
    return answers(each, instances(s, pattern, shape, each), itself);
}

/* (match $from $pattern (add-atom $to $pattern)) */
static mt_atom *copied(const model *from, model *to, const mt_atom *pattern)
{
    mt_atom *each[MOST];
    size_t n = instances(from, pattern, pattern, each);
    for (size_t i = 0; i < n; i++) hold(to, mt_keep(each[i]));
    return answers(each, n, verdict);
}

/* (match $from $pattern (let $added (add-atom $to $pattern) (remove-atom $from $pattern))) */
static mt_atom *moved(model *from, model *to, const mt_atom *pattern)
{
    mt_atom *each[MOST];
    size_t n = instances(from, pattern, pattern, each);
    for (size_t i = 0; i < n; i++) {
        hold(to, mt_keep(each[i]));
        remove_all(from, each[i]);
    }
    return answers(each, n, verdict);
}

/* Upstream's migrateAtoms names the source on both sides:
   (match $From $Pattern ((add-atom $From $Pattern) (remove-atom $From $Pattern))) */
static mt_atom *both_verdicts(mt_atom *instance)
{
    mt_drop(instance);
    return E(B(true), B(true));
}

static mt_atom *migrated(model *from, const mt_atom *pattern)
{
    mt_atom *each[MOST];
    size_t n = instances(from, pattern, pattern, each);
    for (size_t i = 0; i < n; i++) {
        hold(from, mt_keep(each[i]));
        remove_all(from, each[i]);
    }
    return answers(each, n, both_verdicts);
}

/* (match $space $pattern (let $gone (remove-atom $space $pattern) $pattern)) */
static mt_atom *drained(model *s, const mt_atom *pattern)
{
    mt_atom *each[MOST];
    size_t n = instances(s, pattern, pattern, each);
    for (size_t i = 0; i < n; i++) remove_all(s, each[i]);
    return answers(each, n, itself);
}

/* (match $removals $atom (remove-atom $space $atom)) */
static mt_atom *subtracted(model *s, const model *removals)
{
    mt_atom *each[MOST], *any = V("atom");
    size_t n = instances(removals, any, any, each);
    mt_drop(any);
    for (size_t i = 0; i < n; i++) remove_all(s, each[i]);
    return answers(each, n, verdict);
}

/* (collapse (match $space $x (remove-atom $space $x))): one answer, the
   expression of every removal's verdict. */
static mt_atom *cleared(model *s)
{
    mt_atom *each[MOST], *any = V("x");
    size_t n = instances(s, any, any, each);
    mt_drop(any);
    for (size_t i = 0; i < n; i++) remove_all(s, each[i]);
    return E(answers(each, n, verdict));
}

static mt_atom *atoms_of(const model *s)
{
    mt_atom *each[MOST];
    for (size_t i = 0; i < s->n; i++) each[i] = mt_keep(s->at[i]);
    return mt_exprv(s->n, each);
}

static void model_free(model *s)
{
    while (s->n) mt_drop(s->at[--s->n]);
}

static mt_atom *collapsed(mt_atom *goal) { return E("collapse", goal); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_spaces", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));

    mt_space *ledger = mt_space_open(m, "&ledger");
    require("&ledger", ledger != NULL);
    model books = { .n = 0 }, audit = { .n = 0 }, everything = { .n = 0 }, archive = { .n = 0 }, nowhere = { .n = 0 };
    mt_atom *entries[] = { E("entry", "rent", 1200), E("entry", "food", 300), E("note", T("checked")) };
    for (size_t i = 0; i < sizeof entries / sizeof *entries; i++) {
        require("add to the ledger", mt_add(ledger, mt_keep(entries[i])));
        hold(&books, entries[i]);
    }
#define L mt_spaceref("&ledger")

    /* match-count folds over the matches, so no match counts 0. */
    mt_atom *entry = E("entry", V("what"), V("amount")), *invoice = E("invoice", V("n"));
    check_answers("match-count", mt_eval(m, E("match-count", L, mt_keep(entry))), count(&books, entry));
    check_answers("no match counts 0", mt_eval(m, E("match-count", L, mt_keep(invoice))), count(&books, invoice));
    mt_atom *rent = E("entry", "rent", V("amount")), *car = E("entry", "car", V("amount"));
    check_answers("find", mt_eval(m, E("find", L, mt_keep(rent))), B(count(&books, rent) > 0));
    check_answers("find nothing", mt_eval(m, E("find", L, mt_keep(car))), B(count(&books, car) > 0));
    check_answers("a Prolog-shaped call", mt_eval(m, E("succeedsPredicate", E(L, "entry", "rent", V("amount")))),
                  B(count(&books, rent) > 0));
    mt_drop(invoice);
    mt_drop(car);

    /* space-copy leaves the source alone; $any copies everything. */
    check_answers("space-copy", mt_eval(m, collapsed(E("space-copy", L, mt_spaceref("&audit"), mt_keep(entry)))),
                  copied(&books, &audit, entry));
    check_answers("the copies arrived", mt_eval(m, E("match-count", mt_spaceref("&audit"), mt_keep(entry))), count(&audit, entry));
    check_answers("the source kept them", mt_eval(m, E("match-count", L, mt_keep(entry))), count(&books, entry));
    mt_atom *any = V("any");
    check_answers("copying everything", mt_eval(m, collapsed(E("space-copy", L, mt_spaceref("&everything"), mt_keep(any)))),
                  copied(&books, &everything, any));
    check_answers("which merges", mt_eval(m, E("space-atom-count", mt_spaceref("&everything"))), (int64_t)everything.n);

    /* space-snapshot mints its destination, so later writes miss it. */
    mt_atom *before = mt_one(mt_eval(m, E("space-snapshot", L)));
    require("a snapshot", before != NULL);
    model frozen = { .n = 0 };
    for (size_t i = 0; i < books.n; i++) hold(&frozen, mt_keep(books.at[i]));
    check_answers("a snapshot holds what the space held", mt_eval(m, E("space-atom-count", mt_keep(before))), (int64_t)frozen.n);
    mt_atom *travel = E("entry", "travel", 90);
    require("add travel", mt_add(ledger, mt_keep(travel)));
    hold(&books, travel);
    check_answers("and keeps it after a write", mt_eval(m, E("space-atom-count", mt_keep(before))), (int64_t)frozen.n);
    check_answers("which the space sees", mt_eval(m, E("space-atom-count", L)), (int64_t)books.n);

    /* move-atoms moves; migrateAtoms, upstream's equation, drains. */
    mt_atom *travelled = E("entry", "travel", V("amount")), *food = E("entry", "food", V("amount"));
    check_answers("move-atoms", mt_eval(m, collapsed(E("move-atoms", L, mt_spaceref("&archive"), mt_keep(travelled)))),
                  moved(&books, &archive, travelled));
    mt_atom *pair = E(V("what"), V("amount"));
    check_answers("the atoms arrive", mt_eval(m, collapsed(E("match", mt_spaceref("&archive"), mt_keep(entry), mt_keep(pair)))),
                  matched(&archive, entry, pair));
    check_answers("and leave", mt_eval(m, E("match-count", L, mt_keep(travelled))), count(&books, travelled));
    check_answers("migrateAtoms", mt_eval(m, collapsed(E("migrateAtoms", L, mt_spaceref("&nowhere"), mt_keep(food)))),
                  migrated(&books, food));
    check_answers("writes no destination", mt_eval(m, collapsed(E("get-atoms", mt_spaceref("&nowhere")))), atoms_of(&nowhere));
    check_answers("and drains its source", mt_eval(m, E("match-count", L, mt_keep(food))), count(&books, food));

    /* space-drain answers what left; space-subtract removes another space's
       atoms; remove-all-atoms clears. */
    mt_atom *note = E("note", V("text"));
    check_answers("space-drain", mt_eval(m, collapsed(E("space-drain", L, mt_keep(note)))), drained(&books, note));
    check_answers("what stays", mt_eval(m, collapsed(E("get-atoms", L))), atoms_of(&books));
    check_answers("space-subtract", mt_eval(m, collapsed(E("space-subtract", mt_spaceref("&everything"), mt_spaceref("&audit")))),
                  subtracted(&everything, &audit));
    check_answers("what is left", mt_eval(m, collapsed(E("get-atoms", mt_spaceref("&everything")))), atoms_of(&everything));
    check_answers("an atom not held answers the usual verdict",
                  mt_eval(m, collapsed(E("space-subtract", mt_spaceref("&everything"), mt_spaceref("&audit")))),
                  subtracted(&everything, &audit));
    check_answers("remove-all-atoms", mt_eval(m, collapsed(E("remove-all-atoms", mt_spaceref("&everything")))), cleared(&everything));
    check_answers("empties", mt_eval(m, collapsed(E("get-atoms", mt_spaceref("&everything")))), atoms_of(&everything));
    check_answers("and a second clear answers the empty expression",
                  mt_eval(m, collapsed(E("remove-all-atoms", mt_spaceref("&everything")))), cleared(&everything));
#undef L

    mt_atom *held[] = { entry, rent, any, travelled, food, pair, note, before };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    model *models[] = { &books, &audit, &everything, &archive, &nowhere, &frozen };
    for (size_t i = 0; i < sizeof models / sizeof *models; i++) model_free(models[i]);
    mt_space_close(ledger);
    return done(m);
}
