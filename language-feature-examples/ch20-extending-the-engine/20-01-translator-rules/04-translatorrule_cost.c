/* Purpose: a translator rule that carries a cost, and one whose left side
 *   is a conjunction. pow2 is one bidirectional declaration costing 10 for
 *   its head, and C decides which of the two equivalent forms each call
 *   answers with costs.h, the engine's cost fold and orientation rule, fed
 *   the same declaration: (pow2 3) weighs 11 against (mul 3 3)'s 3, so it
 *   expands, and a multiplication of a ten-item list by itself weighs 21
 *   against (pow2 ...)'s 20, so it collapses. unit-of's left side joins the
 *   call to a space pattern on the variables they share; C keeps the unit
 *   rows as its own table, adds them, and expects (in <unit>) exactly for
 *   the quantities its table holds and no answer for any other. The rule
 *   compiles to the equation an author would write, a match chain, which C
 *   reads back through the space's own lookup.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "costs.h"

static const declared_cost pow2_cost[] = { { "pow2", 10 } };
#define POW2_COSTS (sizeof pow2_cost / sizeof *pow2_cost)

/* The unit each quantity is measured in, as C holds it. */
static const struct { const char *quantity, *unit; } units[] = { { "mass", "kg" }, { "length", "m" } };
#define UNITS (sizeof units / sizeof *units)

static mt_atom *squared(mt_atom *x) { return E("mul", mt_keep(x), x); }
static mt_atom *powered(mt_atom *x) { return E("pow2", x); }

/* The unit C's table gives a quantity, or NULL for one it does not hold. */
static const char *unit_of(const char *quantity)
{
    for (size_t i = 0; i < UNITS; i++)
        if (strcmp(units[i].quantity, quantity) == 0) return units[i].unit;
    return NULL;
}

int main(void)
{
    metta *m = open_engine();
    require("(: pow2 (-> Atom %Undefined%))", mt_add(m, E(":", "pow2", E("->", "Atom", "%Undefined%"))));
    require("pow2's equation", mt_add(m, E("=", powered(V("x")), E("noeval", squared(V("x"))))));
    require("a costed bidirectional rule",
            mt_one_truth(mt_eval(m, E("add-translator-rule!", "pow2",
                                      E(E("direction", "bidirectional"), E("cost", pow2_cost[0].cost))))));

    mt_atom *three = N(3), *wide = E("a", "b", "c", "d", "e", "f", "g", "h", "i", "j");
    check_answers("the squaring is expanded", mt_eval(m, powered(mt_keep(three))),
                  oriented(powered(mt_keep(three)), squared(mt_keep(three)), pow2_cost, POW2_COSTS));
    check_answers("and a wide product collapses", mt_eval(m, squared(mt_keep(wide))),
                  oriented(squared(mt_keep(wide)), powered(mt_keep(wide)), pow2_cost, POW2_COSTS));
    mt_drop(three), mt_drop(wide);

    for (size_t i = 0; i < UNITS; i++) require("a unit row", mt_add(m, E("unit", units[i].quantity, units[i].unit)));
    require("(: unit-of (-> Atom %Undefined%))", mt_add(m, E(":", "unit-of", E("->", "Atom", "%Undefined%"))));
    require("a conjunctive rule",
            mt_one_truth(mt_eval(m, E("add-translator-rule!", "unit-of",
                                      E(E("left", E(E("unit-of", V("q")), E("unit", V("q"), V("u")))),
                                        E("right", E("in", V("u"))))))));
    static const char *const asked[] = { "mass", "length", "time" };
    for (size_t i = 0; i < sizeof asked / sizeof *asked; i++) {
        const char *unit = unit_of(asked[i]);
        if (unit)
            check_answers("the conjuncts join on the shared variable", mt_eval(m, E("unit-of", asked[i])), E("in", unit));
        else
            check_none("a conjunct that does not match is a miss", mt_eval(m, E("unit-of", asked[i])));
    }

    int64_t bodies = 0;
    mt_rows (row, mt_match(m, E("=", E("unit-of", V("q")), V("body")))) {
        const mt_atom *body = mt_bound(row, "body");
        check("the compiled body is a match chain",
              mt_kind_of(body) == MT_EXPR && mt_len(body) > 0 && mt_kind_of(mt_at(body, 0)) == MT_SYMBOL &&
                  strcmp(mt_name(mt_at(body, 0)), "match") == 0);
        bodies++;
    }
    check_int("and the rule compiled to one equation", bodies, 1);
    return done(m);
}
