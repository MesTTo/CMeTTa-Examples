/* Purpose: keeping the first answer only. match-single cuts after its
 *   match, so of two foos one bar is stored; C does the original's let
 *   itself, taking the one answer and storing (bar answer). From C the same
 *   commitment is mt_first, which keeps the first answer of a lazy cursor.
 * Guarantees: the original's claim holds, and mt_first commits as cut does
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

static const int64_t FOO[] = { 1, 2 };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (size_t i = 0; i < sizeof FOO / sizeof *FOO; i++)
        require("(foo n)", mt_add(m, E("foo", FOO[i])));
    require("match-single", mt_add(m, E("=", E("match-single", V("space"), V("pat"), V("ret")),
                                       E("let*", E(E(V("x"), E("match", V("space"), V("pat"), V("ret"))), E(V("temp"), E("cut"))), V("x")))));

    mt_atom *first = mt_one(mt_eval(m, E("match-single", "&self", E("foo", V("n")), V("n"))));
    require("store (bar first)", first && mt_add(m, E("bar", first)));
    assert(answers_are(mt_match(m, E("bar", V("n"))), E(E("bar", 1))) && "one bar, from the first foo");
    assert(atom_is(mt_first(mt_match(m, E("foo", V("n")))), E("foo", 1)) && "mt_first commits the same way");
    mt_close(m);
    return 0;
}
