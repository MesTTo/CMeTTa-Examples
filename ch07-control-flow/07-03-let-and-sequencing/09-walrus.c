/* Purpose: let* binds where an expression stands, which C does with an
 *   assignment, itself an expression: (big = n * 10, big + big) names the
 *   value and reuses it, sequenced by the comma operator, and
 *   (half = n / 2) < 10 tests the value it has just named. The engine's
 *   let* equations must answer what these C functions return.
 * Guarantees: all three claims of the original hold
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

static int64_t double_used(int64_t n)
{
    int64_t big;
    return (big = n * 10, big + big);
}

static mt_atom *guarded_half(int64_t n)
{
    int64_t half;
    return (half = n / 2) < 10 ? N(half) : S("nope");     /* n >= 0, so / floors */
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("double-used", mt_add(m, E("=", E("double-used", V("n")),
                                      E("let*", E(E(V("big"), E("*", V("n"), 10))), E("+", V("big"), V("big"))))));
    require("guarded-half", mt_add(m, E("=", E("guarded-half", V("n")),
                                       E("let*", E(E(V("half"), E("floor-div", V("n"), 2))),
                                         E("if", E("<", V("half"), 10), V("half"), "nope")))));
    assert(answers_are(mt_eval(m, E("double-used", 3)), E(double_used(3))) && "(double-used 3)");
    assert(answers_are(mt_eval(m, E("guarded-half", 8)), E(guarded_half(8))) && "(guarded-half 8)");
    assert(answers_are(mt_eval(m, E("guarded-half", 40)), E(guarded_half(40))) && "(guarded-half 40)");
    mt_close(m);
    return 0;
}
