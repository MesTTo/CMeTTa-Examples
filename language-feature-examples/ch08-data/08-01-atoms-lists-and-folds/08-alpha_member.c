/* Purpose: membership up to unification. Despite its name,
 *   is-alpha-member asks whether some element UNIFIES with the needle,
 *   keeping no binding, except that a needle which is a bare variable
 *   matches only a variable [source: engine/metta/input_guards.pl,
 *   member_alpha/2; commit=33219ffa03a890a068e177d1503fa98978750cca]. In C
 *   that is a loop over the list's children with mt_unify under the same
 *   guard, and each of the original's twenty-two needle and haystack pairs
 *   is a row whose expected answer is C's loop. The closing print is printf
 *   of the pattern and the verdict.
 * Guarantees: all twenty-two claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static bool unifies(const mt_atom *a, const mt_atom *b)
{
    mt_bindings *theta = mt_unify(a, b);
    mt_bindings_free(theta);
    return theta != NULL;
}

static bool alpha_member(const mt_atom *needle, const mt_atom *haystack)
{
    for (size_t i = 0; i < mt_len(haystack); i++) {
        const mt_atom *element = mt_at(haystack, i);
        if (mt_kind_of(needle) == MT_VARIABLE ? mt_kind_of(element) == MT_VARIABLE : unifies(needle, element))
            return true;
    }
    return false;
}

int main(void)
{
    metta *m = open_engine();
    const struct { const char *claim; mt_atom *needle, *haystack; } rows[] = {
        { "nothing is in the empty list", S("x"), mt_unit() },
        { "a variable is not a symbol", V("x"), E("a", "b", "c") },
        { "a symbol that is there", S("a"), E("a", "b", "c") },
        { "one that is not", S("d"), E("a", "b", "c") },
        { "renamed variables", E("f", V("x")), E(E("f", V("y")), E("g", V("z"))) },
        { "a repeated candidate", E("f", V("x")), E(E("f", V("y")), E("f", V("y"))) },
        { "nested", E("f", E("g", V("x")), V("y")), E(E("f", E("g", V("a")), V("b")), E("h", V("c"), V("d"))) },
        { "a repeated variable must repeat", E("f", E("g", V("x")), V("x")),
          E(E("f", E("g", V("a")), V("b")), E("f", E("g", V("c")), V("c"))) },
        { "arities differ", E("f", V("x")), E(E("f", V("x"), V("y")), E("g", V("z"))) },
        { "a number that is there", N(42), E(1, 2, 42, 3) },
        { "one that is not", N(99), E(1, 2, 42, 3) },
        { "a nested list", E(1, V("x")), E(E(1, 2), E(3, 4)) },
        { "one that differs", E(1, V("x")), E(E(2, 3), E(4, 5)) },
        { "more than one occurrence", S("a"), E("a", "b", "a", "c") },
        { "two variables renamed", E("f", V("x"), V("y")), E(E("f", V("a"), V("b")), E("f", V("c"), V("d"))) },
        { "a single element", S("a"), E("a") },
        { "that is not the needle", S("b"), E("a") },
        { "variables all round", V("x"), E(V("y"), V("z"), V("w")) },
        { "deeply nested", E("a", E("b", E("c", V("x")))), E(E("a", E("b", E("c", V("d")))), E("e", V("f"))) },
        { "different functors", E("f", V("x")), E(E("g", V("y")), E("h", V("z"))) },
        { "the empty expression is a member", mt_unit(), E(mt_unit(), "a", "b") },
        { "or is not", mt_unit(), E("a", "b", "c") },
    };
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++) {
        bool held = alpha_member(rows[i].needle, rows[i].haystack);
        check_answers(rows[i].claim, mt_eval(m, E("is-alpha-member", rows[i].needle, rows[i].haystack)), B(held));
    }

    mt_atom *pattern = E("hi", "name", "boss"), *fresh = V("new");
    printf("pattern:- %s\n", mt_show(pattern));
    printf("is member:- %s in pattern:- %s\n",
           mt_show(mt_one(mt_eval(m, E("is-alpha-member", mt_keep(fresh), mt_keep(pattern))))), mt_show(pattern));
    mt_drop(fresh);
    mt_drop(pattern);
    return done(m);
}
