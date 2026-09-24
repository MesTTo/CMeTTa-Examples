/* Purpose: a table stays true to its equations. pick answers one, and tabled
 *   it answers one from its table; a second equation lowered beside the
 *   first makes the table stale, and the engine's change funnel drops it, so
 *   the next call answers one and two; removing the first equation, as an
 *   atom C builds, leaves two. A tabled call answers from SWI's answer trie
 *   rather than in clause order, so the pair is compared as a set: sorted
 *   with qsort and mt_order, which is sort-atom's order.
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* Every answer of a call, in the standard order. TAKES goal. */
static mt_list sorted(metta *m, mt_atom *goal)
{
    mt_list answers = mt_all(mt_eval(m, goal));
    qsort(answers.items, answers.len, sizeof *answers.items, mt_order);
    return answers;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_tabling", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_tabling")))));
    require("(pick $x) is one", mt_lower(m, (pick $x), one));
    require("table pick", mt_one_truth(mt_eval(m, E("tabled", E("pick", V("x"))))));
    for (int i = 0; i < 2; i++) check_answers("one, from the table", mt_eval(m, E("pick", "a")), S("one"));

    require("(pick $x) is two as well", mt_lower(m, (pick $x), two));
    check_list("the change reaches the table", sorted(m, E("pick", "a")), "one", "two");
    require("remove the first", mt_del(m, E("=", E("pick", V("x")), "one")));
    check_answers("and the removal", mt_eval(m, E("pick", "a")), S("two"));
    return done(m);
}
