/* Purpose: functions run backwards. mt_solve puts a list on let's pattern
 *   side and reads the unknowns by name: through cons, through f, an
 *   ordinary equation, and through g, whose # arithmetic solves 42 = x + 35.
 *   C splits the same list itself, the head mt_at(list, 0) and the tail a
 *   view over the rest.
 * Guarantees: all three claims of the original hold, and C's split agrees
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static void drop_parent(void *parent) { mt_drop(parent); }

/* The one row's ($Head $Tail) as an expression. */
static mt_atom *split(mt_answers *rows, const char *head, const char *tail)
{
    mt_atom *pair = NULL;
    mt_rows (row, rows) {
        mt_drop(pair);
        pair = E(mt_keep(mt_bound(row, head)), mt_keep(mt_bound(row, tail)));
    }
    return pair;
}

int main(void)
{
    metta *m = open_engine();
    require("f", mt_add(m, E("=", E("f", V("X"), V("Y")), E("append", E(V("X")), V("Y")))));
    require("g", mt_add(m, E("=", E("g", V("X"), V("Y"), V("Z")), E("append", E(E("#+", V("X"), V("Z"))), V("Y")))));

    mt_atom *items = E(1, 2, 3, 4, 5, 6);
    mt_atom *c_split = E(mt_keep(mt_at(items, 0)),
                         mt_expr_ref(mt_len(items) - 1, mt_children(items) + 1, mt_keep(items), drop_parent));
    check_atom("cons runs backwards", split(mt_solve(m, E("cons", V("Head"), V("Tail")), mt_keep(items)), "Head", "Tail"),
               mt_keep(c_split));
    check_atom("so does f", split(mt_solve(m, E("f", V("Head"), V("Tail")), mt_keep(items)), "Head", "Tail"), c_split);

    mt_atom *solved = NULL;
    mt_rows (row, mt_solve(m, E("g", V("X"), V("Y"), 35), E(42, 2, 3)))
        solved = E(mt_keep(mt_bound(row, "X")), mt_keep(mt_bound(row, "Y")), 40);
    check_atom("g solves 42 = x + 35", solved, E(42 - 35, E(2, 3), 40));
    mt_drop(items);
    return done(m);
}
