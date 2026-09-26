/* Purpose: counting by folding ones. countitem answers 1 per foo fact, and
 *   folding those ones with merge, a C function, counts the facts; C counts
 *   them itself by walking the match cursor.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
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

static mt_status merge(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(mt_int(mt_arg(call, 0)) + mt_int(mt_arg(call, 1))));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (int64_t n = 1; n <= 3; n++) require("(foo n)", mt_add(m, E("foo", n)));
    require("countitem", mt_add(m, E("=", E("countitem"), E("let", V("x"), E("match", "&self", E("foo", V("1")), E("foo", V("1"))), 1))));
    require("publish merge", mt_def(m, (mt_op){ .name = "merge", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = merge }));
    require("spacecount", mt_add(m, E("=", E("spacecount", V("x")), E("foldall", "merge", E("countitem"), 0))));

    int64_t facts = 0;
    mt_each (fact, mt_match(m, E("foo", V("n")))) {
        (void)fact;
        facts++;
    }
    assert(answers_are(mt_eval(m, E("foldall", "merge", E("countitem"), 0)), E(facts)) && "folding ones counts the facts");
    mt_close(m);
    return 0;
}
