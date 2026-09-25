/* Purpose: memoization is per arity. add has a two-place and a three-place
 *   equation, each a body lowering.h's operators compile to a C function
 *   and build as the equation, and only the two-place one is memoized. Every
 *   call must answer what C computes, the cached arity on its miss and its
 *   hit alike, and the other arity untouched by the cache.
 * Guarantees: all five claims of the original hold, and only the two-place
 *   calls reach the cache [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "tabling.h"

#define ADD2(ADD, x, y) ADD(x, y)
#define ADD3(ADD, x, y, z) ADD(ADD(x, y), z)

static int64_t add2(int64_t x, int64_t y) { return ADD2(C_ADD, x, y); }
static int64_t add3(int64_t x, int64_t y, int64_t z) { return ADD3(C_ADD, x, y, z); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("add/2", mt_add(m, E("=", E("add", V("x"), V("y")), ADD2(T_ADD, V("x"), V("y")))));
    require("add/3", mt_add(m, E("=", E("add", V("x"), V("y"), V("z")), ADD3(T_ADD, V("x"), V("y"), V("z")))));
    require("memoize add/2", mt_one_truth(mt_eval(m, E("memoize", "add", 2))));

    check_answers("(add 3 4) misses", mt_eval(m, E("add", 3, 4)), add2(3, 4));
    check_answers("(add 3 4) hits", mt_eval(m, E("add", 3, 4)), add2(3, 4));
    check_answers("(add 1 2 3) is not cached", mt_eval(m, E("add", 1, 2, 3)), add3(1, 2, 3));
    check_answers("(add 5 6) misses", mt_eval(m, E("add", 5, 6)), add2(5, 6));
    check_answers("(add 5 6) hits", mt_eval(m, E("add", 5, 6)), add2(5, 6));
    check_answers("two misses and two hits, all two-place", mt_eval(m, E("get-memoize-stats")),
                  memo_counters(2, 2));
    return done(m);
}
