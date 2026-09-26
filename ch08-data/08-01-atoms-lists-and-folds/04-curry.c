/* Purpose: too few arguments, and too many. A call short of arguments is a
 *   partial application, which arrives as the expression (partial F Args) of
 *   the wire grammar every seat reads, so C compares it as the term it builds;
 *   applied to the rest inside the engine it answers. A call with too many is
 *   refused, which C sees as MT_ERROR on the cursor with the engine's words
 *   naming the arities the function has and the count it was given; a head
 *   that names nothing stays as written.
 * Guarantees: all twelve claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* (partial f (args...)), the expression a call short of arguments answers. */
static mt_atom *partial(const char *f, mt_atom *args) { return E("partial", f, args); }

/* The call is refused, and the engine's words name `arities` and `found`. */
static void refused(metta *m, const char *claim, mt_atom *call, const char *arities, const char *found)
{
    mt_clear();
    mt_list got = mt_all(mt_eval(m, call));
    const char *why = mt_errmsg();
    assert(got.len == 0 && mt_error() == MT_ERROR && why && strstr(why, arities) && strstr(why, found) && claim);
    mt_list_free(got);
    mt_clear();
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("f", mt_add(m, E("=", E("f", V("a"), V("b")), E("+", V("a"), V("b")))));
    require("g", mt_add(m, E("=", E("g", V("a"), V("b"), V("c")), E("+", V("c"), E("+", V("a"), V("b"))))));
    require("show", mt_add(m, E("=", E("show"), E("repr", E("f", 1)))));
    require("h", mt_add(m, E("=", E("h", V("A"), V("B")), E("append", E(V("A")), V("B")))));
    require("overloaded-curry/1", mt_add(m, E("=", E("overloaded-curry", V("a")), V("a"))));
    require("overloaded-curry/3", mt_add(m, E("=", E("overloaded-curry", V("a"), V("b"), V("c")), E("+", V("a"), E("+", V("b"), V("c"))))));

    assert(answers_are(mt_eval(m, E("f", 1)), E(partial("f", E(1)))) && "(f 1) is a partial");
    assert(answers_are(mt_eval(m, E(E("f", 1), 2)), E(3)) && "applied to the rest it answers");
    assert(answers_are(mt_eval(m, E("g", 1, 2)), E(partial("g", E(1, 2)))) && "(g 1 2) too");
    assert(answers_are(mt_eval(m, E(E("h", 42), E(1, 2, 3))), E(E(42, 1, 2, 3))) && "((h 42) (1 2 3))");
    assert(answers_are(mt_eval(m, E("h", 42)), E(partial("h", E(42)))) && "(h 42)");
    assert(answers_are(mt_eval(m, E("map-atom", E(1, 2, 3), E("+", 1))), E(E(2, 3, 4))) && "a half-applied builtin maps");

    refused(m, "(+ 1 2 3) is refused", E("+", 1, 2, 3), "function_input_arities(+,[2])", "found `3'");
    refused(m, "through reduce too", E("reduce", E("+", 1, 2, 3)), "function_input_arities(+,[2])", "found `3'");
    refused(m, "(empty 1 2) too", E("empty", 1, 2), "function_input_arities(empty,[0])", "found `2'");
    assert(answers_are(mt_eval(m, E("nosuchfn", 1, 2, 3)), E(E("nosuchfn", 1, 2, 3))) && "a head naming nothing stays");
    assert(answers_are(mt_eval(m, E("overloaded-curry", 1, 2)), E(partial("overloaded-curry", E(1, 2))))
           && "a gap between arities is a partial");
    mt_close(m);
    return 0;
}
