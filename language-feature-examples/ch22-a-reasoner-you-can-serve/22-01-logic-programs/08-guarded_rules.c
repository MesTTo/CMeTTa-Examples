/* Purpose: a tagged rule with a side condition over its premise tags. The
 *   scored facts are C's table and the threshold is ABOVE_HALF, one body
 *   over lowering.h's operators, built as above-half's equation for the
 *   rule's guard and compiled for C; C admits exactly the facts its own threshold admits,
 *   each answering its score times the rule's tag. Under bool the guard
 *   reads the same score. Under the product of prob and polynomial each
 *   answer carries its one derivation's monomial, whose two variables carry
 *   the fact's tag and the rule's, which C sums.
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define ABOVE_HALF(GT, s) GT(s, 0.5)
enum { RULE_TAG = 1 };

static const struct { const char *who; double score; } scores[] = { { "a", 0.6 }, { "b", 0.3 } };
#define SCORES (sizeof scores / sizeof *scores)

static mt_answers *under(metta *m, mt_atom *carrier, mt_atom *pattern)
{
    return mt_eval(m, E("match-under", mt_spaceref("&self"), carrier, pattern));
}

int main(void)
{
    metta *m = open_engine();
    for (size_t i = 0; i < SCORES; i++) require("a scored fact", mt_add(m, E("fact", scores[i].score, E("score", scores[i].who))));
    require("above-half", mt_add(m, E("=", E("above-half", V("s")), ABOVE_HALF(T_GT, V("s")))));
    require("the guarded rule", mt_add(m, E("rule", RULE_TAG, E("trusted", V("x")), E("premises", E("score", V("x"))),
                                            E("where", "above-half"))));
    mt_atom *want[SCORES];
    size_t admitted = 0;
    for (size_t i = 0; i < SCORES; i++)
        if (ABOVE_HALF(C_GT, scores[i].score)) want[admitted++] = E(E("trusted", scores[i].who), scores[i].score * RULE_TAG);
    require("C admits one", admitted == 1);
    check_answers_("the guard keeps what C's threshold keeps", under(m, S("prob"), E("trusted", V("x"))), admitted, want);
    int64_t rows = 0;
    mt_each (row, under(m, S("prob"), E("trusted", V("x")))) rows++;
    check_int("and nothing else", rows, (int64_t)admitted);
    check_answers("bool reads the same score", under(m, S("bool"), E("trusted", "a")), E(E("trusted", "a"), scores[0].score));

    mt_atom *answer = mt_first(under(m, E("product", "prob", "polynomial"), E("trusted", "a")));
    mt_atom *pattern = E(E("trusted", "a"), E("pair", V("p"), E("poly", E(1, E("var", V("k1"), V("w1")), E("var", V("k2"), V("w2"))))));
    mt_bindings *monomial = answer ? mt_unify(answer, pattern) : NULL;
    require("one monomial of two variables", monomial != NULL);
    check_real("its variables carry the fact's tag and the rule's",
               mt_float(mt_binding(monomial, "w1")) + mt_float(mt_binding(monomial, "w2")), scores[0].score + RULE_TAG);
    mt_bindings_free(monomial);
    mt_drop(answer), mt_drop(pattern);
    return done(m);
}
