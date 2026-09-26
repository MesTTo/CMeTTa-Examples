/* Purpose: three questions about two atoms, each with a C oracle. implies
 *   is C's !p || q, checked over the whole truth table by two loops, and
 *   may-vote? guards a table of people the same way. =? asks what mt_unify
 *   asks in C, whether two atoms could be made equal, and binds neither
 *   side; if-equal2 asks what mt_eq asks, whether they are equal as they
 *   stand, variables by identity. Each engine answer is set against its C
 *   oracle.
 * Guarantees: all seventeen claims of the original hold
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

static const struct { const char *name; int64_t age; bool registered; } PEOPLE[] = {
    { "ann", 30, true }, { "bo", 12, true },
};

/* mt_unify's answer as a Bool: whether the pair unifies. */
static mt_atom *unifies(mt_atom *a, mt_atom *b)
{
    mt_bindings *theta = mt_unify(a, b);
    mt_atom *answer = B(theta != NULL);
    mt_bindings_free(theta);
    mt_drop(a);
    mt_drop(b);
    return answer;
}

static void unifiable(metta *m, const char *claim, mt_atom *a, mt_atom *b)
{
    mt_atom *want = unifies(mt_keep(a), mt_keep(b));
    assert(answers_are(mt_eval(m, E("=?", a, b)), E(want)) && claim);
}

/* mt_eq's choice between two arms, as if-equal2 chooses. */
static void equal(metta *m, const char *claim, mt_atom *a, mt_atom *b, const char *same, const char *other)
{
    const char *want = mt_eq(a, b) ? same : other;
    assert(answers_are(mt_eval(m, E("if-equal2", a, b, same, other)), E(want)) && claim);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    for (int p = 0; p <= 1; p++)
        for (int q = 0; q <= 1; q++)
            assert(answers_are(mt_eval(m, E("implies", B(p), B(q))), E(B(!p || q))) && "implies is !p || q");

    for (size_t i = 0; i < sizeof PEOPLE / sizeof *PEOPLE; i++) {
        require("(= (age p) n)", mt_add(m, E("=", E("age", PEOPLE[i].name), PEOPLE[i].age)));
        require("(= (registered? p) b)", mt_add(m, E("=", E("registered?", PEOPLE[i].name), B(PEOPLE[i].registered))));
    }
    require("adult?", mt_add(m, E("=", E("adult?", V("p")), E(">", E("age", V("p")), 17))));
    require("may-vote?", mt_add(m, E("=", E("may-vote?", V("p")), E("implies", E("registered?", V("p")), E("adult?", V("p"))))));
    for (size_t i = 0; i < sizeof PEOPLE / sizeof *PEOPLE; i++)
        assert(answers_are(mt_eval(m, E("may-vote?", PEOPLE[i].name)), E(B(!PEOPLE[i].registered || PEOPLE[i].age > 17)))
               && "may-vote? guards with implies");

    unifiable(m, "f of a variable could be f of 1", E("f", V("x")), E("f", 1));
    unifiable(m, "f of 2 could not be f of 1", E("f", 2), E("f", 1));
    unifiable(m, "two variables could be one", V("a"), V("b"));
    unifiable(m, "f is not g", E("f", V("x")), E("g", V("x")));
    assert(answers_are(mt_eval(m, E("and", E("=?", E("f", V("x")), E("f", 1)), E("=?", E("f", V("x")), E("f", 2)))), E(B(true)))
           && "=? binds nothing, so $x could be 1 and 2");

    equal(m, "one variable is itself", E("f", V("x")), E("f", V("x")), "same", "different");
    equal(m, "two variables are two", E("f", V("x")), E("f", V("y")), "renamed", "different");
    equal(m, "a variable is not a number", E("f", V("x")), E("f", 1), "renamed", "different");
    equal(m, "nor a repeated one two", E("f", V("x"), V("x")), E("f", V("y"), V("z")), "renamed", "different");

    unifiable(m, "the same pair unifies", E("f", V("x")), E("f", 1));
    equal(m, "but is not equal", E("f", V("x")), E("f", 1), "renamed", "different");
    mt_close(m);
    return 0;
}
