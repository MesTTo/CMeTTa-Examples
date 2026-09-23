/* Purpose: growing a space 300 times. demo-peano seeds (num Z) and expands
 *   it, each round adding the successor of every numeral not yet stored;
 *   C then measures each answered numeral's depth by walking its S layers
 *   with mt_at and checks the depths are exactly 0 to 300, each once.
 * Guarantees: the original's claim holds, as a count and as the set of
 *   depths [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

enum { ROUNDS = 300 };

/* How many S wrap Z, or -1 for anything that is not a numeral. */
static int depth(const mt_atom *numeral)
{
    int d = 0;
    while (mt_kind_of(numeral) == MT_EXPR && mt_len(numeral) == 2 && strcmp(mt_name(mt_at(numeral, 0)), "S") == 0) {
        numeral = mt_at(numeral, 1);
        d++;
    }
    return mt_kind_of(numeral) == MT_SYMBOL && strcmp(mt_name(numeral), "Z") == 0 ? d : -1;
}

int main(void)
{
    metta *m = open_engine();
    require("add-atom-no-duplicate", mt_lower(m, (add-atom-no-duplicate $Space $Atom),
                                              (if (== () (collapse (once (match $Space $Atom $Atom))))
                                                  (add-atom $Space $Atom) (empty))));
    require("expand-once", mt_lower(m, (expand-once),
                                    (case (match &self (num $t) $t) (($x (add-atom-no-duplicate &self (num (S $x))))))));
    /* Raw, because `done (let ...)` is a call of common.h's done() macro to
       the preprocessor, which mt_lower would expand into the equation. */
    require("expandK", mt_lower_raw(m, (expandK $n), (if (== $n 0) done (let $temp1 (expand-once) (expandK (- $n 1))))));
    require("demo-peano", mt_lower(m, (demo-peano $K),
                                   (let* (($s (add-atom &self (num Z))) ($g (expandK $K))) (match &self (num $1) $1))));

    bool seen[ROUNDS + 1] = { false };
    int64_t numerals = 0, distinct = 0;
    mt_each (numeral, mt_eval(m, E("demo-peano", ROUNDS))) {
        int d = depth(numeral);
        numerals++;
        if (d >= 0 && d <= ROUNDS && !seen[d]) seen[d] = true, distinct++;
    }
    check_int("one numeral per depth, 0 to 300", numerals == distinct ? numerals : -1, ROUNDS + 1);
    return done(m);
}
