/* Purpose: too few arguments, and too many. A call short of arguments is a
 *   partial application, which arrives as the expression (partial F Args) of
 *   the wire grammar every seat reads, so C compares it as the term it builds;
 *   applied to the rest inside the engine it answers. A call with too many is
 *   refused, which C sees as MT_ERROR on the cursor with the engine's words
 *   naming the arities the function has and the count it was given; a head
 *   that names nothing stays as written.
 * Guarantees: all twelve claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* (partial f (args...)), the expression a call short of arguments answers. */
static mt_atom *partial(const char *f, mt_atom *args) { return E("partial", f, args); }

/* The call is refused, and the engine's words name `arities` and `found`. */
static void refused(metta *m, const char *claim, mt_atom *call, const char *arities, const char *found)
{
    mt_clear();
    mt_list got = mt_all(mt_eval(m, call));
    const char *why = mt_errmsg();
    check(claim, got.len == 0 && mt_error() == MT_ERROR && why && strstr(why, arities) && strstr(why, found));
    mt_list_free(got);
    mt_clear();
}

int main(void)
{
    metta *m = open_engine();
    require("f", mt_lower(m, (f $a $b), (+ $a $b)));
    require("g", mt_lower(m, (g $a $b $c), (+ $c (+ $a $b))));
    require("show", mt_lower(m, (show), (repr (f 1))));
    require("h", mt_lower(m, (h $A $B), (append ($A) $B)));
    require("overloaded-curry/1", mt_lower(m, (overloaded-curry $a), $a));
    require("overloaded-curry/3", mt_lower(m, (overloaded-curry $a $b $c), (+ $a (+ $b $c))));

    check_answers("(f 1) is a partial", mt_eval(m, E("f", 1)), partial("f", E(1)));
    check_answers("applied to the rest it answers", mt_eval(m, E(E("f", 1), 2)), 3);
    check_answers("(g 1 2) too", mt_eval(m, E("g", 1, 2)), partial("g", E(1, 2)));
    check_answers("((h 42) (1 2 3))", mt_eval(m, E(E("h", 42), E(1, 2, 3))), E(42, 1, 2, 3));
    check_answers("(h 42)", mt_eval(m, E("h", 42)), partial("h", E(42)));
    check_answers("a half-applied builtin maps", mt_eval(m, E("map-atom", E(1, 2, 3), E("+", 1))), E(2, 3, 4));

    refused(m, "(+ 1 2 3) is refused", E("+", 1, 2, 3), "function_input_arities(+,[2])", "found `3'");
    refused(m, "through reduce too", E("reduce", E("+", 1, 2, 3)), "function_input_arities(+,[2])", "found `3'");
    refused(m, "(empty 1 2) too", E("empty", 1, 2), "function_input_arities(empty,[0])", "found `2'");
    check_answers("a head naming nothing stays", mt_eval(m, E("nosuchfn", 1, 2, 3)), E("nosuchfn", 1, 2, 3));
    check_answers("a gap between arities is a partial", mt_eval(m, E("overloaded-curry", 1, 2)),
                  partial("overloaded-curry", E(1, 2)));
    return done(m);
}
