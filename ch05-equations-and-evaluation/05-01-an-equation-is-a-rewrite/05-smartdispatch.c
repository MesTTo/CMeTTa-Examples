/* Purpose: which heads run. f is a C function; g answers its arguments
 *   under a head nothing defines, so they stay data; h applies whatever
 *   function it is handed; notjustdata answers the name f, which then
 *   applies; and a data expression with a call inside it has the call
 *   reduced. f is reached by name from every one of them.
 * Guarantees: the five answers of the original, evaluated in one tuple
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

static mt_status twice(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "f doubles an integer");
    return mt_answer(call, N(x * 2));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish f", mt_def(m, (mt_op){ .name = "f", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = twice }));
    require("(= (g $f $x) (justdata $f $x))",
            mt_add(m, E("=", E("g", V("f"), V("x")), E("justdata", V("f"), V("x")))));
    require("(= (h $f $x) ($f $x))", mt_add(m, E("=", E("h", V("f"), V("x")), E(V("f"), V("x")))));
    require("(= (notjustdata $x) f)", mt_add(m, E("=", E("notjustdata", V("x")), "f")));
    require("(= (datawithnondatacomponent) ((lol (f 42))))",
            mt_add(m, E("=", E("datawithnondatacomponent"), E(E("lol", E("f", 42))))));

    assert(answers_are(mt_eval(m, E(E("f", 21), E("g", "f", 2), E("h", "f", 2),
                                    E(E("notjustdata", 42), 21), E("datawithnondatacomponent"))), E(E(42, E("justdata", "f", 2), 4, 42, E(E("lol", 84)))))
           && "each head does what its equation says");
    mt_close(m);
    return 0;
}
