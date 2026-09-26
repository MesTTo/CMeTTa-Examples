/* Purpose: a tagged rule with a side condition over its premise tags. The
 *   scored facts are C's table and the threshold is ABOVE_HALF, one body
 *   over the C_ and T_ operators, built as above-half's equation for the
 *   rule's guard and compiled for C; C admits exactly the facts its own threshold admits,
 *   each answering its score times the rule's tag. Under bool the guard
 *   reads the same score. Under the product of prob and polynomial each
 *   answer carries its one derivation's monomial, whose two variables carry
 *   the fact's tag and the rule's, which C sums.
 * Guarantees: all four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_GT(a, b) ((a) > (b))
#define T_GT(a, b) mt_expr(">", a, b)

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (size_t i = 0; i < SCORES; i++) require("a scored fact", mt_add(m, E("fact", scores[i].score, E("score", scores[i].who))));
    require("above-half", mt_add(m, E("=", E("above-half", V("s")), ABOVE_HALF(T_GT, V("s")))));
    require("the guarded rule", mt_add(m, E("rule", RULE_TAG, E("trusted", V("x")), E("premises", E("score", V("x"))),
                                            E("where", "above-half"))));
    mt_atom *want[SCORES];
    size_t admitted = 0;
    for (size_t i = 0; i < SCORES; i++)
        if (ABOVE_HALF(C_GT, scores[i].score)) want[admitted++] = E(E("trusted", scores[i].who), scores[i].score * RULE_TAG);
    require("C admits one", admitted == 1);
    assert(answers_are(under(m, S("prob"), E("trusted", V("x"))), mt_exprv(admitted, want)) && "the guard keeps what C's threshold keeps");
    int64_t rows = 0;
    mt_each (row, under(m, S("prob"), E("trusted", V("x")))) rows++;
    assert(rows == (int64_t)admitted && "and nothing else");
    assert(answers_are(under(m, S("bool"), E("trusted", "a")), E(E(E("trusted", "a"), scores[0].score))) && "bool reads the same score");

    mt_atom *answer = mt_first(under(m, E("product", "prob", "polynomial"), E("trusted", "a")));
    mt_atom *pattern = E(E("trusted", "a"), E("pair", V("p"), E("poly", E(1, E("var", V("k1"), V("w1")), E("var", V("k2"), V("w2"))))));
    mt_bindings *monomial = answer ? mt_unify(answer, pattern) : NULL;
    require("one monomial of two variables", monomial != NULL);
    assert(mt_float(mt_binding(monomial, "w1")) + mt_float(mt_binding(monomial, "w2")) == scores[0].score + RULE_TAG
           && "its variables carry the fact's tag and the rule's");
    mt_bindings_free(monomial);
    mt_drop(answer), mt_drop(pattern);
    mt_close(m);
    return 0;
}
