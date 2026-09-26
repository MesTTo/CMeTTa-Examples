/* Purpose: a result type decides whether the result is evaluated. f's
 *   %Undefined% result runs its body's sum, g's Atom result keeps it as
 *   written, and h holds its Atom argument too, so the sum it builds keeps
 *   (+ 1 1) inside. C computes what is evaluated with its own arithmetic and
 *   keeps what is held as the term it built.
 * Guarantees: all three claims of the original hold
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const struct { const char *name, *parameter, *result; } arrows[] = {
        { "f", "Number", "%Undefined%" }, { "g", "Number", "Atom" }, { "h", "Atom", "Atom" },
    };
    for (size_t i = 0; i < 3; i++) {
        require("declare the arrow", mt_add(m, E(":", arrows[i].name, E("->", arrows[i].parameter, arrows[i].result))));
        require("(= (name $x) (+ $x 42))", mt_add(m, E("=", E(arrows[i].name, V("x")), E("+", V("x"), 42))));
    }
    assert(answers_are(mt_eval(m, E("f", E("+", 1, 1))), E(N(1 + 1 + 42))) && "an evaluated result");
    assert(answers_are(mt_eval(m, E("g", E("+", 1, 1))), E(E("+", 1 + 1, 42))) && "an Atom result keeps the sum");
    assert(answers_are(mt_eval(m, E("h", E("+", 1, 1))), E(E("+", E("+", 1, 1), 42))) && "and an Atom argument keeps its own");
    mt_close(m);
    return 0;
}
