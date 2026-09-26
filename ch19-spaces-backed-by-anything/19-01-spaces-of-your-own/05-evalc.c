/* Purpose: evaluation in a space you name. &self measures in feet and
 *   &metric in metres, one body DISTANCE over the C_ and T_ operators and a
 *   unit, built as each space's equation and compiled to the C function C holds
 *   each answer to. evalc is mt_eval with a space's handle as its target,
 *   and the goal crosses unevaluated, so (distance (+ 1 1)) is reduced in
 *   the space named. context-space answers the space evaluating it. A space
 *   an equation answers is a space C opens by the name it answered. A space
 *   is named with a leading ampersand: the handle door refuses anything else
 *   before the engine is asked, and the engine's own evalc refuses 7 as a
 *   type error. Removing &metric's equation, named as an atom through
 *   the T_ atom spellings, leaves &self's to answer there.
 * Guarantees: all ten claims of the original hold, and the handle door's
 *   own refusal [tested 2026-09-27T00:35:58+10:00:
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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define DISTANCE(MUL, x, unit) MUL(x, unit)
#define FEET 5280
#define METRES 1000

static int64_t distance(int64_t x, int64_t unit) { return DISTANCE(C_MUL, x, unit); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *metric = mt_space_open(m, "&metric");
    require("open &metric", metric != NULL);
    require("&metric's distance", mt_add(metric, E("=", E("distance", V("x")), DISTANCE(T_MUL, V("x"), METRES))));
    require("&self's", mt_add(m, E("=", E("distance", V("x")), DISTANCE(T_MUL, V("x"), FEET))));

    assert(answers_are(mt_eval(m, E("distance", 2)), E(distance(2, FEET))) && "&self answers in feet");
    assert(answers_are(mt_eval(metric, E("distance", 2)), E(distance(2, METRES))) && "&metric in metres");
    assert(answers_are(mt_eval(mt_self(m), E("+", 5, 5)), E(5 + 5)) && "naming &self is evaluating there");
    assert(answers_are(mt_eval(m, E("eval", E("+", 5, 5))), E(5 + 5)) && "which is eval");
    assert(answers_are(mt_eval(metric, E("distance", E("+", 1, 1))), E(distance(1 + 1, METRES))) && "the goal crosses unevaluated");
    assert(answers_are(mt_eval(m, E("context-space")), E(mt_spaceref("&self"))) && "context-space is &self here");
    assert(answers_are(mt_eval(metric, E("context-space")), E(mt_spaceref("&metric"))) && "and &metric there");

    require("a function answering a space", mt_add(m, E("=", E("preferred-space"), "&metric")));
    mt_atom *preferred = mt_first(mt_eval(m, E("preferred-space")));
    require("it answers one", preferred != NULL);
    mt_space *chosen = mt_space_open(m, mt_name(preferred));
    mt_drop(preferred);
    require("open the space it named", chosen != NULL);
    assert(answers_are(mt_eval(chosen, E("distance", 2)), E(distance(2, METRES))) && "the answered space is where it runs");
    mt_space_close(chosen);

    mt_clear();
    assert(mt_space_open(m, "7") == NULL && mt_error() == MT_MISUSE
           && "the handle door refuses a name without an ampersand");
    mt_clear();
    assert(answers_are(mt_eval(m, E("catch", E("evalc", E("distance", 2), 7))), E(E("Error", E("type_error", "SpaceType", 7), E("context", "evalc", "invalid MeTTa operation argument"))))
           && "and evalc refuses 7 as a type error");

    require("remove &metric's equation",
            mt_del(metric, E("=", E("distance", V("x")), DISTANCE(T_MUL, V("x"), METRES))));
    assert(answers_are(mt_eval(metric, E("distance", 2)), E(distance(2, FEET))) && "&self's is what answers there now");
    mt_space_close(metric);
    mt_close(m);
    return 0;
}
