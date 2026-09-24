/* Purpose: a translator rule that inspects its match and declines in its
 *   own words. strength's first equation refuses a dose above C's limit,
 *   the refusal spelled by verdicts.h as every judge's is, and a refusal is
 *   a decline, so the call carries on to the second equation, which states
 *   the dose in grams. C holds each dose to its own rule: under the limit
 *   milligrams, over it grams, the division being C's. The words are the
 *   rule's own and the engine publishes them into &metta, where C reads them
 *   through the catalog's lookup.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "verdicts.h"

enum { LIMIT = 1000 };
static const char *const too_strong = "a dose above 1000 is not a milligram strength";

static mt_atom *strength(mt_atom *dose) { return E("strength", E("dose", dose), E("unit", "mg")); }

/* What C's rule says a dose is. */
static mt_atom *stated(int64_t dose) { return C_GT(dose, LIMIT) ? E("grams", dose / LIMIT) : E("mg", dose); }

int main(void)
{
    metta *m = open_engine();
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
        check_answers("each dose as C's rule states it", mt_eval(m, strength(N(doses[i]))), stated(doses[i]));
    check_answers("the refusal's words are the rule's own",
                  mt_match(mt_catalog(m), E("translator-rule-refusal", "strength", V("why"))),
                  E("translator-rule-refusal", "strength", T(too_strong)));
    return done(m);
}
