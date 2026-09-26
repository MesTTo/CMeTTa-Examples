/* Purpose: the target of a write is computed. space is a C function
 *   answering the symbol my_space_name, and add-atom evaluates its space
 *   argument, so the write lands in whatever space the function names; any
 *   symbol is a space name the moment it is written to. is-space asks the
 *   narrower question and wants the & prefix.
 * Guarantees: the atom lands in my_space_name, is-space says False for the
 *   bare name and True for &self [tested 2026-09-27T00:35:58+10:00:
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

static mt_status space(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, S("my_space_name"));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish space", mt_def(m, (mt_op){ .name = "space", .arity = 0,
                                                .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = space }));
    require("write through the computed space",
            mt_one_truth(mt_eval(m, E("add-atom", E("space"), E("my", "test", "atom")))));
    assert(answers_are(mt_eval(m, E("match", E("space"), V("a"), V("a"))), E(E("my", "test", "atom")))
           && "the atom is in the space the function named");
    assert(!mt_one_truth(mt_eval(m, E("is-space", "my_space_name"))) && mt_ok() && "is-space wants the & prefix");
    assert(mt_one_truth(mt_eval(m, E("is-space", "&self"))) && "and &self is a space");
    mt_close(m);
    return 0;
}
