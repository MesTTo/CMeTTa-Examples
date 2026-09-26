/* Purpose: counting without listing, and a filtered drain. lib_spaces'
 *   match-count folds a match into a number, which C checks against
 *   mt_count(); migrateAtoms, despite its name, re-adds each matched atom to
 *   its source and removes every copy, so the destination stays empty and a
 *   second run moves nothing.
 * Guarantees: every claim of the original holds
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

static int64_t match_count(metta *m, mt_atom *pattern)
{
    return mt_one_int(mt_eval(m, E("match-count", mt_spaceref("&ledger"), pattern)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_spaces",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));
    mt_space *ledger = mt_space_open(m, "&ledger");
    mt_space *archive = mt_space_open(m, "&archive");
    require("open the two spaces", ledger && archive);
    require("(n 1)", mt_add(ledger, E("n", 1)));
    require("(n 2)", mt_add(ledger, E("n", 2)));
    require("(m 9)", mt_add(ledger, E("m", 9)));

    assert(match_count(m, E("n", V("x"))) == 2 && "two atoms match (n $x)");
    assert(match_count(m, E("m", V("x"))) == 1 && "one matches (m $x)");
    assert(match_count(m, E("zzz", V("x"))) == 0 && "a pattern nothing matches counts 0");
    assert(match_count(m, E(V("head"), V("tail"))) == (int64_t)mt_count(ledger)
           && "($head $tail) counts every atom");

    /* One pair per matched atom: the answers of the two writes it makes. */
    mt_atom *migrate = E("migrateAtoms", mt_spaceref("&ledger"), mt_spaceref("&archive"), E("n", V("x")));
    assert(answers_are(mt_eval(m, mt_keep(migrate)), E(E(B(true), B(true)), E(B(true), B(true))))
           && "two atoms are migrated");
    assert(answers_are(mt_atoms(ledger), E(E("m", 9))) && "and leave the source");
    assert(match_count(m, E("n", V("x"))) == 0 && "so none match any more");
    assert((int64_t)mt_count(archive) == 0 && "and the destination was never written");

    /* A filtered drain: a second run finds nothing to move. */
    assert(!mt_first(mt_eval(m, migrate)) && mt_ok() && "a second run moves nothing");
    assert(answers_are(mt_atoms(ledger), E(E("m", 9))) && "and the rest stays");
    mt_space_close(ledger);
    mt_space_close(archive);
    mt_close(m);
    return 0;
}
