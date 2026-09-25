/* Purpose: memoization belongs to a space. &self and &metric each define
 *   shipping-cost at their own rate, 2 and 9, one body COST expanded two
 *   ways through lowering.h: with C's operators it is what C expects, and
 *   with the atom builders the equation's atom, which mt_add installs in each
 *   space and mt_del removes from &self when its rate changes to 3. Memoizing in one space leaves the other's function
 *   alone until it is memoized too; the change invalidates only &self's cache.
 *   evalc is mt_eval with the space's handle as its target.
 * Guarantees: all sixteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define COST(MUL, w, rate) MUL(w, rate)

static int64_t cost(int64_t w, int64_t rate) { return COST(C_MUL, w, rate); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_memo", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_memo")))));
    mt_space *metric = mt_space_open(m, "&metric");
    require("open &metric", metric != NULL);
    require("&metric's rate", mt_add(metric, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 9))));
    require("&self's rate", mt_add(m, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 2))));

    const int64_t w = 3;
    check_answers("&self's function", mt_eval(m, E("shipping-cost", w)), cost(w, 2));
    check_answers("&metric's", mt_eval(metric, E("shipping-cost", w)), cost(w, 9));
    check_answers("neither memoized here", mt_eval(m, E("is-memoized", "shipping-cost")), B(false));
    check_answers("nor there", mt_eval(metric, E("is-memoized", "shipping-cost")), B(false));

    require("memoize &self's", mt_one_truth(mt_eval(m, E("memoize", "shipping-cost"))));
    check_answers("&self's is memoized", mt_eval(m, E("is-memoized", "shipping-cost")), B(true));
    check_answers("&metric's is not", mt_eval(metric, E("is-memoized", "shipping-cost")), B(false));
    for (int i = 0; i < 2; i++) check_answers("&self's, missed then hit", mt_eval(m, E("shipping-cost", w)), cost(w, 2));
    for (int i = 0; i < 2; i++) check_answers("&metric's, uncached", mt_eval(metric, E("shipping-cost", w)), cost(w, 9));

    require("memoize &metric's", mt_one_truth(mt_eval(metric, E("memoize", "shipping-cost"))));
    check_answers("a second cache", mt_eval(metric, E("is-memoized", "shipping-cost")), B(true));
    for (int i = 0; i < 2; i++) check_answers("&metric's, missed then hit", mt_eval(metric, E("shipping-cost", w)), cost(w, 9));
    check_answers("&self's cache still its own", mt_eval(m, E("shipping-cost", w)), cost(w, 2));

    require("remove &self's rate", mt_del(m, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 2))));
    require("its new rate", mt_add(m, E("=", E("shipping-cost", V("w")), COST(T_MUL, V("w"), 3))));
    check_answers("the change invalidates &self's cache", mt_eval(m, E("shipping-cost", w)), cost(w, 3));
    check_answers("and leaves &metric's standing", mt_eval(metric, E("shipping-cost", w)), cost(w, 9));
    mt_space_close(metric);
    return done(m);
}
