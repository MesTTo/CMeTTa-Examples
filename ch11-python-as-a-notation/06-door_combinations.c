/* Purpose: one definition kind inside another. A define inside a define is
 *   an ordinary call, which C computes as its own twice of twice; a Python
 *   operation inside a define is one crossing, held against C's toupper; and
 *   a body can write an equation, which C builds as the atom the body adds,
 *   so the new name answers that atom's body and a match finds it.
 * Guarantees: all four claims of the original hold, with its one unasserted
 *   form checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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

static int64_t twice(int64_t x) { return x + x; }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("dc-twice", mt_add(m, E("=", E("dc-twice", V("x")), E("+", V("x"), V("x")))));
    require("dc-quad", mt_add(m, E("=", E("dc-quad", V("x")), E("dc-twice", E("dc-twice", V("x"))))));
    assert(answers_are(mt_eval(m, E("dc-quad", 5)), E(N(twice(twice(5))))) && "a define inside a define");
    require("dc-upper", mt_add(m, E("=", E("dc-upper", V("s")), E("py-call", E(".upper", V("s"))))));
    const char word[] = "ab";
    char upper[sizeof word];
    for (size_t i = 0; i < sizeof word; i++) upper[i] = (char)toupper((unsigned char)word[i]);
    assert(answers_are(mt_eval(m, E("dc-upper", T(word))), E(S(upper))) && "a Python operation inside a define");
    mt_atom *nine = E("=", E("dc-nine"), 9);
    require("dc-install", mt_add(m, E("=", E("dc-install"), E("add-atom", "&self", mt_keep(nine)))));
    assert(answers_are(mt_eval(m, E("dc-install")), E(B(true))) && "the body adds an equation");
    assert(answers_are(mt_eval(m, E("dc-nine")), E(mt_keep(mt_at(nine, 2)))) && "and the new name answers");
    assert(answers_are(mt_eval(m, E("match", "&self", E("=", E("dc-nine"), V("body")), V("body"))), E(mt_keep(mt_at(nine, 2)))) && "an equation is an atom to match");
    mt_drop(nine);
    mt_close(m);
    return 0;
}
