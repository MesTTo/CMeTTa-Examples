/* Purpose: lets and superpositions stacked four ways, lowered from C tokens,
 *   then collapsed together. program1 collapses 12 and its argument plus 4,
 *   program2 fans out a collapsed list, program3 branches into (42 43), and
 *   program4 gathers the three calls: the three fan-outs of program2 each
 *   carry the other two answers. C builds the expected rows in a loop.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("program1", mt_lower(m, (program1 $Y), (let $X $Y (collapse (superpose (12 (+ $X 4)))))));
    require("program2", mt_lower(m, (program2 $Y),
                                 (let $list (let $L (1 2 3) (collapse (superpose $L))) (superpose $list))));
    require("program3", mt_lower(m, (program3 $x),
                                 (if (== $x 2) (let $z (superpose ((if (< $x 10) (superpose ((42 43))) 43))) $z)
                                     (let $z 4 $z))));
    require("program4", mt_lower(m, (program4), (collapse ((program1 42) (program2 42) (program3 2)))));

    mt_atom *rows[3];
    for (int64_t n = 1; n <= 3; n++) rows[n - 1] = E(E(12, 42 + 4), n, E(42, 43));
    check_answers("(program4)", mt_eval(m, E("program4")), mt_exprv(3, rows));
    return done(m);
}
