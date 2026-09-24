/* Purpose: a state cell is a value. new-state answers one, and C holds it in
 *   a variable where the original binds a token with bind!, so get-state and
 *   change-state! take the cell itself; each read is the value C last wrote,
 *   and change-state! answers True, as add-atom does, because it runs for its
 *   effect. A cell's type is the signature (: new-state (-> $t (StateMonad
 *   $t))) with $t the type of the value it holds, a Number for 5 and a String
 *   for "hi". A cell needs no name: C's own block scope holds the third one
 *   while it is written and read.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    mt_atom *first = S("rest"), *second = S("active");
    mt_atom *state = new_cell(m, mt_keep(first));
    check_answers("the cell holds its first value", mt_eval(m, E("get-state", mt_keep(state))), mt_keep(first));
    check_answers("a write answers True", mt_eval(m, E("change-state!", mt_keep(state), mt_keep(second))), B(true));
    check_answers("and the cell holds what was written", mt_eval(m, E("get-state", mt_keep(state))), mt_keep(second));
    mt_drop(state), mt_drop(first), mt_drop(second);

    mt_atom *held[] = { N(5), T("hi") };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) {
        check_answers("a cell's type is what it holds", mt_eval(m, E("get-type", E("new-state", mt_keep(held[i])))),
                      cell_type(held[i]));
        mt_drop(held[i]);
    }

    {
        const int64_t built = 1, written = 2;
        mt_atom *cell = new_cell(m, N(built));
        require("write the unnamed cell", mt_one_truth(mt_eval(m, E("change-state!", mt_keep(cell), written))));
        check_answers("a cell needs no name", mt_eval(m, E("get-state", mt_keep(cell))), N(written));
        mt_drop(cell);
    }
    return done(m);
}
