/* Purpose: the Empty branch is the one a key with no answers takes. wu's key
 *   is (empty), so it takes Empty; wu2's key is (f), a C function answering
 *   42, so it takes the 42 branch instead.
 * Guarantees: both claims of the original hold
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

static mt_status f(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(42));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish f", mt_def(m, (mt_op){ .name = "f", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = f }));
    require("wu", mt_add(m, E("=", E("wu"), E("case", E("empty"), E(E(1, 2), E("Empty", 42))))));
    require("wu2", mt_add(m, E("=", E("wu2"), E("case", E("f"), E(E(42, "ok"), E("Empty", "nok"))))));
    assert(answers_are(mt_eval(m, E("wu")), E(42)) && "a key with no answers takes Empty");
    assert(answers_are(mt_eval(m, E("wu2")), E("ok")) && "a key that answers takes its own branch");
    mt_close(m);
    return 0;
}
