/* Purpose: the matching conditional. (unify a b then else) runs its then
 *   branch once per way a and b match and its else branch when there is
 *   none, and only the chosen branch runs: the two probes are C functions
 *   that count their calls and leave a marker in the space. A space operand
 *   is matched by query, a variable operand binds it whole, and C's own
 *   mt_unify() agrees with the engine on the two-sided binding.
 * Guarantees: every claim of the original holds, and exactly one probe runs
 *   per query [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct probe { const char *marker; int64_t answer; unsigned calls; } probe;

static mt_status run_probe(mt_call *call, void *user)
{
    probe *p = user;
    p->calls++;
    if (!mt_add(mt_of(call), E(p->marker))) return mt_error();
    return mt_answer(call, N(p->answer));
}

static mt_atom *unify(mt_atom *a, mt_atom *b, mt_atom *then, mt_atom *otherwise)
{
    return E("unify", a, b, then, otherwise);
}

int main(void)
{
    metta *m = open_engine();
    /* Ground decisions, 1 matching 1.0 included. */
    check_answers("1 matches 1", mt_eval(m, unify(N(1), N(1), S("same"), S("different"))), "same");
    check_answers("1 does not match 2", mt_eval(m, unify(N(1), N(2), S("same"), S("different"))), "different");
    check_answers("1 matches 1.0", mt_eval(m, unify(N(1), R(1.0), S("same"), S("different"))), "same");
    check_answers("\"x\" matches \"x\"", mt_eval(m, unify(T("x"), T("x"), S("same"), S("different"))), "same");
    check_answers("\"x\" does not match \"y\"", mt_eval(m, unify(T("x"), T("y"), S("same"), S("different"))), "different");

    /* Bindings flow into the branch from both sides at once. */
    check_answers("(f $x b) and (f a $y) bind both",
                  mt_eval(m, unify(E("f", V("x"), "b"), E("f", "a", V("y")), E("pair", V("x"), V("y")), S("nope"))),
                  E("pair", "a", "b"));
    mt_atom *left = E("f", V("x"), "b"), *right = E("f", "a", V("y"));
    mt_bindings *both = mt_unify(left, right);
    mt_atom *pair = E("pair", V("x"), V("y"));
    check_atom("and so does C's own unifier", both ? mt_substitute(pair, both) : NULL, E("pair", "a", "b"));
    mt_drop(pair); mt_drop(left); mt_drop(right); mt_bindings_free(both);
    check_answers("no occurs check: $x unifies with (f $x)",
                  mt_eval(m, unify(V("x"), E("f", V("x")), S("cyclic"), S("sound"))), "cyclic");

    /* Only the chosen branch runs. */
    probe then = { "then-ran", 3, 0 }, otherwise = { "else-ran", 4, 0 };
    require("publish then-probe", mt_def(m, (mt_op){ .name = "then-probe", .arity = 0,
        .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = run_probe, .user = &then }));
    require("publish else-probe", mt_def(m, (mt_op){ .name = "else-probe", .arity = 0,
        .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = run_probe, .user = &otherwise }));
    check_int("A matches A: the then branch", mt_one_int(mt_eval(m, unify(S("A"), S("A"),
              E("then-probe"), E("else-probe")))), 3);
    check_none("and no else marker", mt_match(m, E("else-ran")));
    check_int("A does not match B: the else branch", mt_one_int(mt_eval(m, unify(S("A"), S("B"),
              E("then-probe"), E("else-probe")))), 4);
    check_answers("and the then marker is the first one's", mt_match(m, E("then-ran")), E("then-ran"));
    check("exactly one probe ran per query", then.calls == 1 && otherwise.calls == 1);

    /* A space operand is matched by query. */
    require("(friend Bob Alice)", mt_add(m, E("friend", "Bob", "Alice")));
    require("(friend Sam Alice)", mt_add(m, E("friend", "Sam", "Alice")));
    check_answers("one then-answer per stored match",
                  mt_eval(m, unify(mt_spaceref("&self"), E("friend", V("who"), "Alice"), V("who"), S("no-friends"))),
                  "Bob", "Sam");
    check_answers("the else branch when nothing matches",
                  mt_eval(m, unify(mt_spaceref("&self"), E("friend", "Pol", V("who")), V("who"), S("no-friends"))),
                  "no-friends");
    check_answers("a variable binds the space whole",
                  mt_eval(m, unify(V("s"), mt_spaceref("&self"), S("bound"), S("queried"))), "bound");
    check_answers("Empty in a branch removes it",
                  mt_eval(m, E("collapse", unify(S("a"), S("b"), S("then"), S("Empty")))), mt_unit());
    return done(m);
}
