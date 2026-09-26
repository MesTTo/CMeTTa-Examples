/* Purpose: a state cell is a value. new-state answers one, and C holds it in
 *   a variable where the original binds a token with bind!, so get-state and
 *   change-state! take the cell itself; each read is the value C last wrote,
 *   and change-state! answers True, as add-atom does, because it runs for its
 *   effect. A cell's type is the signature (: new-state (-> $t (StateMonad
 *   $t))) with $t the type of the value it holds, a Number for 5 and a String
 *   for "hi". A cell needs no name: C's own block scope holds the third one
 *   while it is written and read.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

/* A literal carries its type: Number for a number, String for a text. */
static mt_atom *literal_type(const mt_atom *x) { return S(mt_kind_of(x) == MT_TEXT ? "String" : "Number"); }

/* The type of a cell holding x. */
static mt_atom *cell_type(const mt_atom *x) { return E("StateMonad", literal_type(x)); }

static mt_atom *new_cell(metta *m, mt_atom *value)
{
    mt_atom *cell = mt_first(mt_eval(m, E("new-state", value)));
    require("new-state answers a cell", cell != NULL);
    return cell;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *first = S("rest"), *second = S("active");
    mt_atom *state = new_cell(m, mt_keep(first));
    assert(answers_are(mt_eval(m, E("get-state", mt_keep(state))), E(mt_keep(first))) && "the cell holds its first value");
    assert(answers_are(mt_eval(m, E("change-state!", mt_keep(state), mt_keep(second))), E(B(true))) && "a write answers True");
    assert(answers_are(mt_eval(m, E("get-state", mt_keep(state))), E(mt_keep(second))) && "and the cell holds what was written");
    mt_drop(state), mt_drop(first), mt_drop(second);

    mt_atom *held[] = { N(5), T("hi") };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) {
        assert(answers_are(mt_eval(m, E("get-type", E("new-state", mt_keep(held[i])))), E(cell_type(held[i])))
               && "a cell's type is what it holds");
        mt_drop(held[i]);
    }

    {
        const int64_t built = 1, written = 2;
        mt_atom *cell = new_cell(m, N(built));
        require("write the unnamed cell", mt_one_truth(mt_eval(m, E("change-state!", mt_keep(cell), written))));
        assert(answers_are(mt_eval(m, E("get-state", mt_keep(cell))), E(N(written))) && "a cell needs no name");
        mt_drop(cell);
    }
    mt_close(m);
    return 0;
}
