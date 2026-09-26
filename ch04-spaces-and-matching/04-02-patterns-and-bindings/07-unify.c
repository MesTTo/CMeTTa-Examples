/* Purpose: the matching conditional. (unify a b then else) runs its then
 *   branch once per way a and b match and its else branch when there is
 *   none, and only the chosen branch runs: the two probes are C functions
 *   that count their calls and leave a marker in the space. A space operand
 *   is matched by query, a variable operand binds it whole, and C's own
 *   mt_unify() agrees with the engine on the two-sided binding.
 * Guarantees: every claim of the original holds, and exactly one probe runs
 *   per query [tested 2026-09-27T00:35:58+10:00:
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    /* Ground decisions, 1 matching 1.0 included. */
    assert(answers_are(mt_eval(m, unify(N(1), N(1), S("same"), S("different"))), E("same")) && "1 matches 1");
    assert(answers_are(mt_eval(m, unify(N(1), N(2), S("same"), S("different"))), E("different")) && "1 does not match 2");
    assert(answers_are(mt_eval(m, unify(N(1), R(1.0), S("same"), S("different"))), E("same")) && "1 matches 1.0");
    assert(answers_are(mt_eval(m, unify(T("x"), T("x"), S("same"), S("different"))), E("same")) && "\"x\" matches \"x\"");
    assert(answers_are(mt_eval(m, unify(T("x"), T("y"), S("same"), S("different"))), E("different")) && "\"x\" does not match \"y\"");

    /* Bindings flow into the branch from both sides at once. */
    assert(answers_are(mt_eval(m, unify(E("f", V("x"), "b"), E("f", "a", V("y")), E("pair", V("x"), V("y")), S("nope"))), E(E("pair", "a", "b")))
           && "(f $x b) and (f a $y) bind both");
    mt_atom *left = E("f", V("x"), "b"), *right = E("f", "a", V("y"));
    mt_bindings *both = mt_unify(left, right);
    mt_atom *pair = E("pair", V("x"), V("y"));
    assert(atom_is(both ? mt_substitute(pair, both) : NULL, E("pair", "a", "b")) && "and so does C's own unifier");
    mt_drop(pair); mt_drop(left); mt_drop(right); mt_bindings_free(both);
    assert(answers_are(mt_eval(m, unify(V("x"), E("f", V("x")), S("cyclic"), S("sound"))), E("cyclic"))
           && "no occurs check: $x unifies with (f $x)");

    /* Only the chosen branch runs. */
    probe then = { "then-ran", 3, 0 }, otherwise = { "else-ran", 4, 0 };
    require("publish then-probe", mt_def(m, (mt_op){ .name = "then-probe", .arity = 0,
        .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = run_probe, .user = &then }));
    require("publish else-probe", mt_def(m, (mt_op){ .name = "else-probe", .arity = 0,
        .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = run_probe, .user = &otherwise }));
    assert(mt_one_int(mt_eval(m, unify(S("A"), S("A"),
E("then-probe"), E("else-probe")))) == 3
           && "A matches A: the then branch");
    assert(!mt_first(mt_match(m, E("else-ran"))) && mt_ok() && "and no else marker");
    assert(mt_one_int(mt_eval(m, unify(S("A"), S("B"),
E("then-probe"), E("else-probe")))) == 4
           && "A does not match B: the else branch");
    assert(answers_are(mt_match(m, E("then-ran")), E(E("then-ran"))) && "and the then marker is the first one's");
    assert(then.calls == 1 && otherwise.calls == 1 && "exactly one probe ran per query");

    /* A space operand is matched by query. */
    require("(friend Bob Alice)", mt_add(m, E("friend", "Bob", "Alice")));
    require("(friend Sam Alice)", mt_add(m, E("friend", "Sam", "Alice")));
    assert(answers_are(mt_eval(m, unify(mt_spaceref("&self"), E("friend", V("who"), "Alice"), V("who"), S("no-friends"))), E("Bob", "Sam"))
           && "one then-answer per stored match");
    assert(answers_are(mt_eval(m, unify(mt_spaceref("&self"), E("friend", "Pol", V("who")), V("who"), S("no-friends"))), E("no-friends"))
           && "the else branch when nothing matches");
    assert(answers_are(mt_eval(m, unify(V("s"), mt_spaceref("&self"), S("bound"), S("queried"))), E("bound"))
           && "a variable binds the space whole");
    assert(answers_are(mt_eval(m, E("collapse", unify(S("a"), S("b"), S("then"), S("Empty")))), E(mt_unit()))
           && "Empty in a branch removes it");
    mt_close(m);
    return 0;
}
