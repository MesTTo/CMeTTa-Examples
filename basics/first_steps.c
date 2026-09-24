/* Purpose: a first program. Build a term and evaluate it, publish a C
 *   function the engine calls, store facts and join them with a query, and
 *   take every answer of a nondeterministic expression.
 * Guarantees: each step's answer is checked [tested: make check;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* (double x) answered by C: the engine calls this with its argument. */
static mt_status twice(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "double wants an integer");
    return mt_answer(call, N(2 * x));
}

int main(void)
{
    metta *m = open_engine();

    /* A term is built, not written: (+ 20 22). */
    check_int("(+ 20 22) is 42", mt_one_int(mt_eval(m, E("+", 20, 22))), 42);

    require("publish double", mt_def(m, (mt_op){ .name = "double", .arity = 1,
                                                 .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = twice }));
    check_int("(double 21) is 42", mt_one_int(mt_eval(m, E("double", 21))), 42);

    /* Facts, and a conjunction that joins them on the shared $p. */
    static const char *const parent[][2] = { {"Tom", "Bob"}, {"Bob", "Ann"}, {"Ann", "Zoe"} };
    for (size_t i = 0; i < 3; i++)
        require("store a fact", mt_add(m, E("Parent", parent[i][0], parent[i][1])));
    mt_atom *grandparent = E(",", E("Parent", V("gp"), V("p")),
                                  E("Parent", V("p"), V("gc")));
    size_t rows = 0;
    mt_rows (row, mt_query(m, grandparent, NULL)) {
        if (rows++ == 0) {
            check_atom("the first grandparent is Tom", mt_keep(mt_bound(row, "gp")), S("Tom"));
            check_atom("and the grandchild Ann", mt_keep(mt_bound(row, "gc")), S("Ann"));
        }
    }
    check_int("two grandparent links", (int64_t)rows, 2);

    /* One expression, three answers. */
    check_answers("superpose answers each value",
                  mt_eval(m, E("superpose", E(1, 2, 3))), 1, 2, 3);
    return done(m);
}
