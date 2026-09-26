/* Purpose: a translator rule that inspects its match and declines in its
 *   own words. strength's first equation refuses a dose above C's limit,
 *   the refusal spelled as every judge's is, and a refusal is
 *   a decline, so the call carries on to the second equation, which states
 *   the dose in grams. C holds each dose to its own rule: under the limit
 *   milligrams, over it grams, the division being C's. The words are the
 *   rule's own and the engine publishes them into &metta, where C reads them
 *   through the catalog's lookup.
 * Guarantees: all three claims of the original hold
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

/* The verdict that raises with the words; takes them. */
static inline mt_atom *refusing(mt_atom *words) { return mt_expr("Refuse", words); }

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define T_IF(c, t, e) mt_expr("if", c, t, e)
#define C_GT(a, b) ((a) > (b))
#define T_GT(a, b) mt_expr(">", a, b)

enum { LIMIT = 1000 };
static const char *const too_strong = "a dose above 1000 is not a milligram strength";

static mt_atom *strength(mt_atom *dose) { return E("strength", E("dose", dose), E("unit", "mg")); }

/* What C's rule says a dose is. */
static mt_atom *stated(int64_t dose) { return C_GT(dose, LIMIT) ? E("grams", dose / LIMIT) : E("mg", dose); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: strength (-> Atom Atom %Undefined%))",
            mt_add(m, E(":", "strength", E("->", "Atom", "Atom", "%Undefined%"))));
    require("the equation that may refuse",
            mt_add(m, E("=", strength(V("n")),
                        T_IF(T_GT(V("n"), LIMIT), refusing(T(too_strong)), E("noeval", E("mg", V("n")))))));
    require("the one a refusal falls through to",
            mt_add(m, E("=", strength(V("n")), E("noeval", E("grams", E("/", V("n"), LIMIT))))));
    require("strength is a rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", "strength"))));

    static const int64_t doses[] = { 250, 5000 };
    for (size_t i = 0; i < sizeof doses / sizeof *doses; i++)
        assert(answers_are(mt_eval(m, strength(N(doses[i]))), E(stated(doses[i]))) && "each dose as C's rule states it");
    assert(answers_are(mt_match(mt_catalog(m), E("translator-rule-refusal", "strength", V("why"))), E(E("translator-rule-refusal", "strength", T(too_strong))))
           && "the refusal's words are the rule's own");
    mt_close(m);
    return 0;
}
