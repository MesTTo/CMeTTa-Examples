/* Purpose: counting without listing, and a filtered drain. lib_spaces'
 *   match-count folds a match into a number, which C checks against
 *   mt_count(); migrateAtoms, despite its name, re-adds each matched atom to
 *   its source and removes every copy, so the destination stays empty and a
 *   second run moves nothing.
 * Guarantees: every claim of the original holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static int64_t match_count(metta *m, mt_atom *pattern)
{
    return mt_one_int(mt_eval(m, E("match-count", mt_spaceref("&ledger"), pattern)));
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_spaces",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));
    mt_space *ledger = mt_space_open(m, "&ledger");
    mt_space *archive = mt_space_open(m, "&archive");
    require("open the two spaces", ledger && archive);
    require("(n 1)", mt_add(ledger, E("n", 1)));
    require("(n 2)", mt_add(ledger, E("n", 2)));
    require("(m 9)", mt_add(ledger, E("m", 9)));

    check_int("two atoms match (n $x)", match_count(m, E("n", V("x"))), 2);
    check_int("one matches (m $x)", match_count(m, E("m", V("x"))), 1);
    check_int("a pattern nothing matches counts 0", match_count(m, E("zzz", V("x"))), 0);
    check_int("($head $tail) counts every atom", match_count(m, E(V("head"), V("tail"))),
              (int64_t)mt_count(ledger));

    /* One pair per matched atom: the answers of the two writes it makes. */
    mt_atom *migrate = E("migrateAtoms", mt_spaceref("&ledger"), mt_spaceref("&archive"), E("n", V("x")));
    check_answers("two atoms are migrated", mt_eval(m, mt_keep(migrate)),
                  E(B(true), B(true)), E(B(true), B(true)));
    check_answers("and leave the source", mt_atoms(ledger), E("m", 9));
    check_int("so none match any more", match_count(m, E("n", V("x"))), 0);
    check_int("and the destination was never written", (int64_t)mt_count(archive), 0);

    /* A filtered drain: a second run finds nothing to move. */
    check_none("a second run moves nothing", mt_eval(m, migrate));
    check_answers("and the rest stays", mt_atoms(ledger), E("m", 9));
    mt_space_close(ledger);
    mt_space_close(archive);
    return done(m);
}
