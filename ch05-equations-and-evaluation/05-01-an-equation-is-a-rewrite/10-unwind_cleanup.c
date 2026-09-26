/* Purpose: cleanup as an equation. on-unwind runs its handler once when the
 *   wrapped computation fails or is cut, and never when it succeeds; the
 *   handler is an equation that records the outcome as a fact, and C reads
 *   the facts to see which exits ran it.
 * Guarantees: success leaves no note, failure notes (fail), and a caller's
 *   once notes (!) [tested 2026-09-27T00:35:58+10:00:
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    /* (= (unwind-note $home $outcome) (add-atom $home (unwound $outcome))) */
    require("define the handler", mt_add(m, E("=", E("unwind-note", V("home"), V("outcome")),
                                              E("add-atom", V("home"), E("unwound", V("outcome"))))));
    mt_atom *note = E("unwind-note", "&self");
    mt_atom *unwound = E("unwound", V("outcome"));

    assert(mt_one_int(mt_eval(m, E("on-unwind", 7, mt_keep(note)))) == 7
           && "a deterministic success skips cleanup");
    assert(!mt_first(mt_match(m, mt_keep(unwound))) && mt_ok() && "so nothing was noted");

    assert(!mt_first(mt_eval(m, E("on-unwind", E("superpose", mt_unit()), mt_keep(note)))) && mt_ok() && "a failure answers nothing");
    assert(answers_are(mt_match(m, mt_keep(unwound)), E(E("unwound", E("fail")))) && "and notes (fail)");
    require("clear the note", mt_del(m, E("unwound", E("fail"))));

    assert(mt_one_int(mt_eval(m, E("once", E("on-unwind", E("superpose", E(1, 2)), note)))) == 1
           && "a caller's once cuts after the first answer");
    assert(answers_are(mt_match(m, unwound), E(E("unwound", E("!")))) && "and notes the cut");
    mt_close(m);
    return 0;
}
