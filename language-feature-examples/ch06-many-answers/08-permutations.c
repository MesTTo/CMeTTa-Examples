/* Purpose: 9! by joining inequalities. C writes the 56 facts (i != j) for
 *   every ordered pair of distinct positions and the nine E rows, each
 *   moving the hole ___ one slot along, with loops; the query is a
 *   conjunction C generates in the same triangular order, each new position
 *   differing from every earlier one; and C counts the joined answers as it
 *   walks the lazy cursor.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

enum { POSITIONS = 8 };

static mt_atom *slot(int k)
{
    char name[16];
    snprintf(name, sizeof name, "_%d", k);
    return V(name);
}

int main(void)
{
    metta *m = open_engine();

    for (int i = 1; i <= POSITIONS; i++)
        for (int j = 1; j <= POSITIONS; j++)
            if (i != j) require("(i != j)", mt_add(m, E(i, "!=", j)));

    /* (E $_1 ... $_8 hole (... ___ ...)): the hole at 1-based position `at`. */
    for (int at = 1; at <= POSITIONS + 1; at++) {
        mt_atom *row[POSITIONS + 1], *fact[POSITIONS + 3];
        for (int k = 1, r = 0; r <= POSITIONS; r++) row[r] = r == at - 1 ? mt_sym("___") : slot(k++);
        fact[0] = mt_sym("E");
        for (int k = 1; k <= POSITIONS; k++) fact[k] = slot(k);
        fact[POSITIONS + 1] = N(at);
        fact[POSITIONS + 2] = mt_exprv(POSITIONS + 1, row);
        require("(E ...)", mt_add(m, mt_exprv(POSITIONS + 3, fact)));
    }

    /* (, ($_1 != $_2) ($_2 != $_3) ($_3 != $_1) ($_3 != $_4) ... (E ... $x $state)) */
    mt_atom *conjuncts[1 + POSITIONS * (POSITIONS - 1) / 2 + 1], *e[POSITIONS + 3];
    size_t n = 0;
    conjuncts[n++] = mt_sym(",");
    for (int k = 2; k <= POSITIONS; k++) {
        conjuncts[n++] = E(slot(k - 1), "!=", slot(k));
        for (int j = k - 2; j >= 1; j--) conjuncts[n++] = E(slot(k), "!=", slot(j));
    }
    e[0] = mt_sym("E");
    for (int k = 1; k <= POSITIONS; k++) e[k] = slot(k);
    e[POSITIONS + 1] = V("x");
    e[POSITIONS + 2] = V("state");
    conjuncts[n++] = mt_exprv(POSITIONS + 3, e);

    size_t count = 0;
    mt_each (answer, mt_eval(m, E("match", "&self", mt_exprv(n, conjuncts), E("state1", V("state"))))) {
        (void)answer;
        count++;
    }
    check_int("every permutation of nine", (int64_t)count, 362880);
    return done(m);
}
