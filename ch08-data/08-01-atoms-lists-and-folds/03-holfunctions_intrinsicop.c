/* Purpose: a builtin half applied. mymap maps a function over a list, and
 *   the half-applied builtin (== 1) must map exactly as (eq 1) does, eq
 *   being a C function; C maps its own comparison over the same array for
 *   the list both must answer.
 * Guarantees: the original's claim holds, and both agree with C's map
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

static mt_status eq(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, B(mt_eq(mt_arg(call, 0), mt_arg(call, 1))));
}

static const int64_t XS[] = { 1, 2, 3 };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (mymap $f ()) ())", mt_add(m, E("=", E("mymap", V("f"), mt_unit()), mt_unit())));
    require("mymap's cons case", mt_add(m, E("=", E("mymap", V("f"), E("cons", V("x"), V("xs"))),
                                            E("let", V("head"), E(V("f"), V("x")),
                                              E("let", V("rest"), E("mymap", V("f"), V("xs")), E("cons", V("head"), V("rest")))))));
    require("publish eq", mt_def(m, (mt_op){ .name = "eq", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = eq }));

    mt_atom *mapped[3];
    for (size_t i = 0; i < 3; i++) mapped[i] = B(XS[i] == 1);
    mt_atom *expected = mt_exprv(3, mapped);
    assert(atom_is(mt_one(mt_eval(m, E("mymap", E("==", 1), E(1, 2, 3)))), mt_one(mt_eval(m, E("mymap", E("eq", 1), E(1, 2, 3)))))
           && "(== 1) maps as (eq 1)");
    assert(answers_are(mt_eval(m, E("mymap", E("eq", 1), E(1, 2, 3))), E(expected)) && "and both as C's comparison");
    mt_close(m);
    return 0;
}
