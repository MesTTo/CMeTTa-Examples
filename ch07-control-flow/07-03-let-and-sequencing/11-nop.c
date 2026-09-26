/* Purpose: the step whose answer is not wanted, C's (void). nop evaluates
 *   every argument and answers the unit, so a write inside it lands and one
 *   unit comes back per branch, where empty answers nothing. The writes go
 *   to spaces C opens by name and reads back itself, mt_count being
 *   space-atom-count.
 * Guarantees: all eleven claims of the original hold
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
    mt_space *log = mt_space_open(m, "&log"), *drained = mt_space_open(m, "&drained");
    require("open &log and &drained", log && drained);

    assert(answers_are(mt_eval(m, E("nop")), E(mt_unit())) && "(nop)");
    assert(answers_are(mt_eval(m, E("nop", 1)), E(mt_unit())) && "(nop 1)");
    assert(answers_are(mt_eval(m, E("nop", 1, 2, 3)), E(mt_unit())) && "(nop 1 2 3)");

    assert(answers_are(mt_eval(m, E("nop", E("add-atom", mt_spaceref("&log"), E("seen", 1)),
                                    E("add-atom", mt_spaceref("&log"), E("seen", 2)))), E(mt_unit()))
           && "the writes inside land");
    assert(answers_are(mt_eval(log, E("match", mt_spaceref("&log"), E("seen", V("n")), V("n"))), E(1, 2)) && "and C reads them back");

    assert(list_is(mt_all(mt_eval(m, E("nop", 1))), E(mt_unit())) && "nop answers one unit");
    assert(!mt_first(mt_eval(m, E("empty"))) && mt_ok() && "empty answers nothing");

    assert(answers_are(mt_eval(m, E("nop", E("let", V("x"), E("superpose", E(1, 2, 3)),
                                             E("add-atom", mt_spaceref("&drained"), E("x", V("x")))))), E(mt_unit(), mt_unit(), mt_unit()))
           && "three branches, three units");
    assert((int64_t)mt_count(drained) == 3 && "and all three writes landed");

    assert(answers_are(mt_eval(m, E("==", E("nop", 1), E("add-atom", mt_spaceref("&log"), E("seen", 3)))), E(B(false)))
           && "the unit is not add-atom's answer");
    assert(answers_are(mt_eval(m, E("==", E("nop", 1), E("nop", 2))), E(B(true))) && "but every nop's is the same");
    mt_space_close(log);
    mt_space_close(drained);
    mt_close(m);
    return 0;
}
