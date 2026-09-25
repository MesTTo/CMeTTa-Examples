/* Purpose: what the cache counted. SQUARE is one body lowering.h's operators
 *   compile to the C function square() and build as sq's equation; sq is
 *   memoized and asked three times on one key, each answer what C computes,
 *   and the library's counters say one miss and two hits, over one stored
 *   entry holding one answer.
 * Guarantees: all three claims of the original hold, and the counters agree
 *   [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "tabling.h"

#define SQUARE(MUL, x) MUL(x, x)

static int64_t square(int64_t x) { return SQUARE(C_MUL, x); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("sq", mt_add(m, E("=", E("sq", V("x")), SQUARE(T_MUL, V("x")))));
    require("memoize sq", mt_one_truth(mt_eval(m, E("memoize", "sq"))));

    for (int i = 0; i < 3; i++) check_answers("(sq 9)", mt_eval(m, E("sq", 9)), square(9));
    check_answers("one miss, then two hits", mt_eval(m, E("get-memoize-stats")),
                  memo_counters(2, 1));
    check_answers("over one entry with one answer", mt_eval(m, E("get-memoize-stats", "sq")),
                  memo_store(1, 1));
    return done(m);
}
