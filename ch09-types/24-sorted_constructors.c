/* Purpose: a declared constructor is sorted data. SortedPoint's arrow has no
 *   equation, so an application is a point whose type is the arrow's last
 *   type, and methods destructure it. C takes the same fields: x is the
 *   first, the norm is its own sqrt, and a String where a Number is declared
 *   is refused at the position C finds. The three loops add a field twenty
 *   times whichever way it is reached, sixty by C's multiplication.
 * Guarantees: all seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 24-sorted_constructors.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

/* (name n sum): n == 0 answers sum, else adds x and loops, x read by `read`. */
static mt_atom *loop(const char *name, mt_atom *read)
{
    return E("=", E(name, V("n"), V("sum")),
             E("if", E("==", V("n"), 0), V("sum"), E("let", V("x"), read, E(name, E("-", V("n"), 1), E("+", V("sum"), V("x"))))));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *arrow = E("->", "Number", "Number", "SortedPoint");
    require("(: SortedPoint (-> Number Number SortedPoint))", mt_add(m, E(":", "SortedPoint", mt_keep(arrow))));
    require("sorted-x", mt_add(m, E("=", E("sorted-x", E("SortedPoint", V("x"), V("y"))), V("x"))));
    require("plain-x", mt_add(m, E("=", E("plain-x", E("PlainPoint", V("x"), V("y"))), V("x"))));
    require("make-sorted-point", mt_add(m, E("=", E("make-sorted-point", V("x"), V("y")), E("SortedPoint", V("x"), V("y")))));
    require("sorted-norm", mt_add(m, E("=", E("sorted-norm", E("SortedPoint", V("x"), V("y"))),
                                       E("sqrt-math", E("+", E("*", V("x"), V("x")), E("*", V("y"), V("y")))))));
    const int64_t x = 3, y = 4;
    assert(answers_are(mt_eval(m, E("get-type", E("SortedPoint", x, y))), E(mt_keep(mt_at(arrow, mt_len(arrow) - 1)))) && "a point's type is its arrow's last");
    assert(answers_are(mt_eval(m, E("sorted-x", E("SortedPoint", x, y))), E(N(x))) && "a field");
    assert(answers_are(mt_eval(m, E("sorted-norm", E("SortedPoint", x, y))), E(mt_real(sqrt((double)(x * x + y * y))))) && "a method over the fields");
    mt_atom *bad = E("SortedPoint", T("bad"), y);
    assert(answers_are(mt_eval(m, E("make-sorted-point", T("bad"), y)), E(E("Error", mt_keep(bad), E("BadArgType", 1, mt_keep(mt_at(arrow, 1)), "String"))))
           && "a text where a Number goes");
    mt_drop(bad);

    require("constructor-control", mt_add(m, loop("constructor-control", N(3))));
    require("constructor-sorted", mt_add(m, loop("constructor-sorted", E("sorted-x", E("SortedPoint", x, y)))));
    require("constructor-plain", mt_add(m, loop("constructor-plain", E("plain-x", E("PlainPoint", x, y)))));
    const char *loops[] = { "constructor-control", "constructor-sorted", "constructor-plain" };
    for (size_t i = 0; i < 3; i++) assert(answers_are(mt_eval(m, E(loops[i], 20, 0)), E(N(20 * x))) && loops[i]);
    mt_drop(arrow);
    mt_close(m);
    return 0;
}
