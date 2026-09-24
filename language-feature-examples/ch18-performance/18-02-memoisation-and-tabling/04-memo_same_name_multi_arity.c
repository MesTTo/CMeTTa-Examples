/* Purpose: one name at two arities is two functions to lib_memo. mix/1 and
 *   mix/2 are bodies lowering.h's operators compile to C functions and lower
 *   to the equations; memoizing mix/1 leaves mix/2 unmemoized until it is
 *   memoized too, and is-memoized answers each arity with a boolean. Every
 *   call answers what C computes.
 * Guarantees: all nine claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define MIX1(ADD, x) ADD(x, 1)
#define MIX2(ADD, x, y) ADD(x, y)

static int64_t mix1(int64_t x) { return MIX1(C_ADD, x); }
static int64_t mix2(int64_t x, int64_t y) { return MIX2(C_ADD, x, y); }

static void memoized(metta *m, int64_t arity, bool is)
{
    check_answers(is ? "that arity is memoized" : "that arity is not", mt_eval(m, E("is-memoized", "mix", arity)),
                  B(is));
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    require("mix/1", mt_lower(m, (mix $x), MIX1(M_ADD, $x)));
    require("mix/2", mt_lower(m, (mix $x $y), MIX2(M_ADD, $x, $y)));
    require("memoize mix/1", mt_one_truth(mt_eval(m, E("memoize", "mix", 1))));
    memoized(m, 1, true);
    memoized(m, 2, false);

    for (int i = 0; i < 2; i++) check_answers("(mix 5)", mt_eval(m, E("mix", 5)), mix1(5));
    for (int i = 0; i < 2; i++) check_answers("(mix 3 4)", mt_eval(m, E("mix", 3, 4)), mix2(3, 4));

    require("memoize mix/2", mt_one_truth(mt_eval(m, E("memoize", "mix", 2))));
    memoized(m, 2, true);
    for (int i = 0; i < 2; i++) check_answers("(mix 8 9)", mt_eval(m, E("mix", 8, 9)), mix2(8, 9));
    return done(m);
}
