/* Purpose: sequencing. A run of C statements is progn: write a fact, take it
 *   back, write another, and read what is left. progn keeps its last value
 *   as C's comma operator does, and prog1 keeps its first.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("write (friend sam tom)", mt_add(m, E("friend", "sam", "tom")));
    require("take it back", mt_del(m, E("friend", "sam", "tom")));
    require("write (friend sam tim)", mt_add(m, E("friend", "sam", "tim")));
    check_answers("what is left", mt_eval(m, E("match", "&self", E("friend", "sam", V("who")), V("who"))), "tim");

    check_answers("prog1 keeps the first", mt_eval(m, E("prog1", 1, 2, 3)), 1);
    check_answers("progn keeps the last, as the comma operator does",
                  mt_eval(m, E("progn", 1, 2, 3)), ((void)1, (void)2, 3));
    return done(m);
}
