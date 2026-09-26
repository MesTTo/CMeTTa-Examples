/* Purpose: a first program. Build a term and evaluate it, publish a C
 *   function the engine calls, store facts and join them with a query, and
 *   take every answer of a nondeterministic expression.
 * Guarantees: each step's answer is checked [tested 2026-09-27T00:35:58+10:00:
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    /* A term is built, not written: (+ 20 22). */
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "(+ 20 22) is 42");

    require("publish double", mt_def(m, (mt_op){ .name = "double", .arity = 1,
                                                 .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = twice }));
    assert(mt_one_int(mt_eval(m, E("double", 21))) == 42 && "(double 21) is 42");

    /* Facts, and a conjunction that joins them on the shared $p. */
    static const char *const parent[][2] = { {"Tom", "Bob"}, {"Bob", "Ann"}, {"Ann", "Zoe"} };
    for (size_t i = 0; i < 3; i++)
        require("store a fact", mt_add(m, E("Parent", parent[i][0], parent[i][1])));
    mt_atom *grandparent = E(",", E("Parent", V("gp"), V("p")),
                                  E("Parent", V("p"), V("gc")));
    size_t rows = 0;
    mt_rows (row, mt_query(m, grandparent, NULL)) {
        if (rows++ == 0) {
            assert(atom_is(mt_keep(mt_bound(row, "gp")), S("Tom")) && "the first grandparent is Tom");
            assert(atom_is(mt_keep(mt_bound(row, "gc")), S("Ann")) && "and the grandchild Ann");
        }
    }
    assert((int64_t)rows == 2 && "two grandparent links");

    /* One expression, three answers. */
    assert(answers_are(mt_eval(m, E("superpose", E(1, 2, 3))), E(1, 2, 3))
           && "superpose answers each value");
    mt_close(m);
    return 0;
}
