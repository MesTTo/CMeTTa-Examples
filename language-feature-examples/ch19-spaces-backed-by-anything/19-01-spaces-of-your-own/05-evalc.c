/* Purpose: evaluation in a space you name. &self measures in feet and
 *   &metric in metres, one body DISTANCE over lowering.h's operators and a
 *   unit, built as each space's equation and compiled to the C function C holds
 *   each answer to. evalc is mt_eval with a space's handle as its target,
 *   and the goal crosses unevaluated, so (distance (+ 1 1)) is reduced in
 *   the space named. context-space answers the space evaluating it. A space
 *   an equation answers is a space C opens by the name it answered. A space
 *   is named with a leading ampersand: the handle door refuses anything else
 *   before the engine is asked, and the engine's own evalc refuses 7 as a
 *   type error. Removing &metric's equation, named as an atom through
 *   lowering.h's atom spellings, leaves &self's to answer there.
 * Guarantees: all ten claims of the original hold, and the handle door's
 *   own refusal [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define DISTANCE(MUL, x, unit) MUL(x, unit)
#define FEET 5280
#define METRES 1000

static int64_t distance(int64_t x, int64_t unit) { return DISTANCE(C_MUL, x, unit); }

int main(void)
{
    metta *m = open_engine();
    mt_space *metric = mt_space_open(m, "&metric");
    require("open &metric", metric != NULL);
    require("&metric's distance", mt_add(metric, E("=", E("distance", V("x")), DISTANCE(T_MUL, V("x"), METRES))));
    require("&self's", mt_add(m, E("=", E("distance", V("x")), DISTANCE(T_MUL, V("x"), FEET))));

    check_answers("&self answers in feet", mt_eval(m, E("distance", 2)), distance(2, FEET));
    check_answers("&metric in metres", mt_eval(metric, E("distance", 2)), distance(2, METRES));
    check_answers("naming &self is evaluating there", mt_eval(mt_self(m), E("+", 5, 5)), 5 + 5);
    check_answers("which is eval", mt_eval(m, E("eval", E("+", 5, 5))), 5 + 5);
    check_answers("the goal crosses unevaluated", mt_eval(metric, E("distance", E("+", 1, 1))), distance(1 + 1, METRES));
    check_answers("context-space is &self here", mt_eval(m, E("context-space")), mt_spaceref("&self"));
    check_answers("and &metric there", mt_eval(metric, E("context-space")), mt_spaceref("&metric"));

    require("a function answering a space", mt_add(m, E("=", E("preferred-space"), "&metric")));
    mt_atom *preferred = mt_first(mt_eval(m, E("preferred-space")));
    require("it answers one", preferred != NULL);
    mt_space *chosen = mt_space_open(m, mt_name(preferred));
    mt_drop(preferred);
    require("open the space it named", chosen != NULL);
    check_answers("the answered space is where it runs", mt_eval(chosen, E("distance", 2)), distance(2, METRES));
    mt_space_close(chosen);

    mt_clear();
    check("the handle door refuses a name without an ampersand",
          mt_space_open(m, "7") == NULL && mt_error() == MT_MISUSE);
    mt_clear();
    check_answers("and evalc refuses 7 as a type error", mt_eval(m, E("catch", E("evalc", E("distance", 2), 7))),
                  E("Error", E("type_error", "SpaceType", 7), E("context", "evalc", "invalid MeTTa operation argument")));

    require("remove &metric's equation",
            mt_del(metric, E("=", E("distance", V("x")), DISTANCE(T_MUL, V("x"), METRES))));
    check_answers("&self's is what answers there now", mt_eval(metric, E("distance", 2)), distance(2, FEET));
    mt_space_close(metric);
    return done(m);
}
