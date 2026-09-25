/* Purpose: each arm of a conditional binds on its own. Four functions whose
 *   arms bind a name with let* are written once as macro bodies over their
 *   operators: expanded with C's ?: and comma operator they are C functions,
 *   and expanded with the atom builders they are the equations mt_add
 *   installs.
 *   The engine's answers must be the C functions' answers.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

/* Comparisons as functions, passed where lowering.h's C_LT and C_GT would
   go, so C compiles a < a as the question the original asks rather than
   folding it as a tautology. */
static bool less(int64_t a, int64_t b) { return a < b; }
static bool greater(int64_t a, int64_t b) { return a > b; }

/* let* ((name value)) body: in C the comma operator runs the binding and
   yields the body; as an atom it is the let* the original writes, binding the
   variable the name spells. */
#define C_LET(name, value, body) ((void)(value), (body))
#define T_LET(name, value, body) E("let*", E(E(V(#name), value)), body)

#define PICK_ELSE(IF, LT, GT, LET, a, b) IF(LT(a, a), LET(c, a, a), b)
#define PICK_THEN(IF, LT, GT, LET, a, b) IF(GT(a, 0), LET(c, a, a), b)
#define BOTH(IF, LT, GT, LET, a, b) IF(GT(a, b), LET(c, 1, a), LET(d, 1, b))

static int64_t pick_else(int64_t a, int64_t b) { return PICK_ELSE(C_IF, less, greater, C_LET, a, b); }
static int64_t pick_then(int64_t a, int64_t b) { return PICK_THEN(C_IF, less, greater, C_LET, a, b); }
static int64_t both(int64_t a, int64_t b) { return BOTH(C_IF, less, greater, C_LET, a, b); }

/* A case over a boolean with a True and a False arm is ?: as well. */
static int64_t case_else(int64_t a, int64_t b) { return less(a, a) ? a : b; }

int main(void)
{
    metta *m = open_engine();
    require("pick-else", mt_add(m, E("=", E("pick-else", V("a"), V("b")), PICK_ELSE(T_IF, T_LT, T_GT, T_LET, V("a"), V("b")))));
    require("pick-then", mt_add(m, E("=", E("pick-then", V("a"), V("b")), PICK_THEN(T_IF, T_LT, T_GT, T_LET, V("a"), V("b")))));
    require("case-else", mt_add(m, E("=", E("case-else", V("a"), V("b")),
                                    E("case", T_LT(V("a"), V("a")), E(E(B(true), T_LET(c, V("a"), V("a"))), E(B(false), V("b")))))));
    require("both", mt_add(m, E("=", E("both", V("a"), V("b")), BOTH(T_IF, T_LT, T_GT, T_LET, V("a"), V("b")))));

    check_answers("(pick-else 1 2)", mt_eval(m, E("pick-else", 1, 2)), pick_else(1, 2));
    check_answers("(pick-then 1 2)", mt_eval(m, E("pick-then", 1, 2)), pick_then(1, 2));
    check_answers("(case-else 3 4)", mt_eval(m, E("case-else", 3, 4)), case_else(3, 4));
    check_answers("(both 5 2)", mt_eval(m, E("both", 5, 2)), both(5, 2));
    check_answers("(both 2 5)", mt_eval(m, E("both", 2, 5)), both(2, 5));
    return done(m);
}
