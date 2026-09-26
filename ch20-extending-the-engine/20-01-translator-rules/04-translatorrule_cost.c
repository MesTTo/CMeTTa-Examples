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
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_fixtures/costs.h"

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: pow2 (-> Atom %Undefined%))", mt_add(m, E(":", "pow2", E("->", "Atom", "%Undefined%"))));
    require("pow2's equation", mt_add(m, E("=", powered(V("x")), E("noeval", squared(V("x"))))));
    require("a costed bidirectional rule",
            mt_one_truth(mt_eval(m, E("add-translator-rule!", "pow2",
                                      E(E("direction", "bidirectional"), E("cost", pow2_cost[0].cost))))));

    mt_atom *three = N(3), *wide = E("a", "b", "c", "d", "e", "f", "g", "h", "i", "j");
    assert(answers_are(mt_eval(m, powered(mt_keep(three))), E(oriented(powered(mt_keep(three)), squared(mt_keep(three)), pow2_cost, POW2_COSTS)))
           && "the squaring is expanded");
    assert(answers_are(mt_eval(m, squared(mt_keep(wide))), E(oriented(squared(mt_keep(wide)), powered(mt_keep(wide)), pow2_cost, POW2_COSTS)))
           && "and a wide product collapses");
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
            assert(answers_are(mt_eval(m, E("unit-of", asked[i])), E(E("in", unit))) && "the conjuncts join on the shared variable");
        else
            assert(!mt_first(mt_eval(m, E("unit-of", asked[i]))) && mt_ok() && "a conjunct that does not match is a miss");
    }

    int64_t bodies = 0;
    mt_rows (row, mt_match(m, E("=", E("unit-of", V("q")), V("body")))) {
        const mt_atom *body = mt_bound(row, "body");
        assert(mt_kind_of(body) == MT_EXPR && mt_len(body) > 0 && mt_kind_of(mt_at(body, 0)) == MT_SYMBOL &&
                   strcmp(mt_name(mt_at(body, 0)), "match") == 0
               && "the compiled body is a match chain");
        bodies++;
    }
    assert(bodies == 1 && "and the rule compiled to one equation");
    mt_close(m);
    return 0;
}
