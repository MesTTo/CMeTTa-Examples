/* Purpose: too few arguments, and too many. A call short of arguments is a
 *   partial application, which C holds as a handle and reads through
 *   mt_show, the engine's own rendering; applied to the rest it answers.
 *   A call with too many is refused, which C sees as MT_ERROR on the cursor
 *   with the engine's words naming the arities the function has and the
 *   count it was given; a head that names nothing stays as written.
 * Guarantees: all twelve claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* The engine's rendering of the call's one answer when it is a handle, the
   partial application the call leaves, or "" when it is not. */
static const char *partial(metta *m, mt_atom *call)
{
    mt_atom *held = mt_one(mt_eval(m, call));
    const char *shown = held && mt_kind_of(held) == MT_HANDLE ? mt_show(held) : "";
    mt_drop(held);
    return shown;
}

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

    check_text("(f 1) is a partial", partial(m, E("f", 1)), "(partial f (1))");
    check_answers("applied to the rest it answers", mt_eval(m, E(E("f", 1), 2)), 3);
    check_text("(g 1 2) too", partial(m, E("g", 1, 2)), "(partial g (1 2))");
    check_answers("((h 42) (1 2 3))", mt_eval(m, E(E("h", 42), E(1, 2, 3))), E(42, 1, 2, 3));
    check_text("(h 42)", partial(m, E("h", 42)), "(partial h (42))");
    check_answers("a half-applied builtin maps", mt_eval(m, E("map-atom", E(1, 2, 3), E("+", 1))), E(2, 3, 4));

    refused(m, "(+ 1 2 3) is refused", E("+", 1, 2, 3), "function_input_arities(+,[2])", "found `3'");
    refused(m, "through reduce too", E("reduce", E("+", 1, 2, 3)), "function_input_arities(+,[2])", "found `3'");
    refused(m, "(empty 1 2) too", E("empty", 1, 2), "function_input_arities(empty,[0])", "found `2'");
    check_answers("a head naming nothing stays", mt_eval(m, E("nosuchfn", 1, 2, 3)), E("nosuchfn", 1, 2, 3));
    check_text("a gap between arities is a partial", partial(m, E("overloaded-curry", 1, 2)),
               "(partial overloaded-curry (1 2))");
    return done(m);
}
