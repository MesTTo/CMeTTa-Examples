/* Purpose: a translator rule that declares its direction, and a
 *   bidirectional rule whose inverse the engine derives. celsius is
 *   left-to-right, as a rule is by default, and KELVIN is its body written
 *   once: over the T_ atom builders it is the rule's expansion, and
 *   over its C operators it is the kelvin C expects. unpack is one
 *   declaration read both ways, so which way a call goes is decided by the
 *   form's cost, and C decides it with costs.h, the engine's node count and
 *   orientation rule in C: each call answers the other side of the rule when
 *   that costs strictly less, and itself otherwise, through the written door
 *   and the eval and reduce doors alike. Withdrawing the rule withdraws the
 *   inverse, so the large twin form is left as written.
 * Guarantees: all eight claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)

#define KELVIN(ADD, c) ADD(c, 273)

/* The two sides of unpack's rule for one argument. Each TAKES x. */
static mt_atom *unpacked(mt_atom *x) { return E("unpack", E("wrap", E("box", x))); }
static mt_atom *twinned(mt_atom *x) { return E("twin", mt_keep(x), x); }

/* Where the rule takes a call on one side: the other side, if cheaper. */
static mt_atom *from_unpacked(mt_atom *x) { return oriented(unpacked(mt_keep(x)), twinned(x), NULL, 0); }
static mt_atom *from_twinned(mt_atom *x) { return oriented(twinned(mt_keep(x)), unpacked(x), NULL, 0); }

static mt_atom *options(const char *direction) { return E(E("direction", direction)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: celsius (-> Atom %Undefined%))", mt_add(m, E(":", "celsius", E("->", "Atom", "%Undefined%"))));
    require("celsius's equation",
            mt_add(m, E("=", E("celsius", E("degrees", V("c"))), E("noeval", E("kelvin", KELVIN(T_ADD, V("c")))))));
    require("a forward rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", "celsius", options("forward")))));
    const int64_t degrees = 27;
    assert(answers_are(mt_eval(m, E("celsius", E("degrees", degrees))), E(E("kelvin", KELVIN(C_ADD, degrees))))
           && "a forward rule fires as written");

    require("(: unpack (-> Atom %Undefined%))", mt_add(m, E(":", "unpack", E("->", "Atom", "%Undefined%"))));
    require("unpack's one equation", mt_add(m, E("=", unpacked(V("x")), E("noeval", twinned(V("x"))))));
    require("a bidirectional rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", "unpack", options("bidirectional")))));

    mt_atom *one = N(1), *abc = E("a", "b", "c");
    assert(answers_are(mt_eval(m, unpacked(mt_keep(one))), E(from_unpacked(mt_keep(one)))) && "four nodes against three goes forwards");
    assert(answers_are(mt_eval(m, twinned(mt_keep(abc))), E(from_twinned(mt_keep(abc)))) && "seven against six goes back");
    assert(answers_are(mt_eval(m, twinned(mt_keep(one))), E(from_twinned(mt_keep(one)))) && "a call at its cheapest is left alone");
    assert(answers_are(mt_eval(m, unpacked(mt_keep(abc))), E(from_unpacked(mt_keep(abc)))) && "from either side");
    assert(answers_are(mt_eval(m, E("eval", twinned(mt_keep(one)))), E(from_twinned(mt_keep(one))))
           && "the eval door blocks the same up-rewrite");
    assert(answers_are(mt_eval(m, E("reduce", twinned(mt_keep(one)))), E(from_twinned(mt_keep(one)))) && "and so does reduce");

    require("withdraw the rule", mt_one_truth(mt_eval(m, E("remove-translator-rule!", "unpack"))));
    assert(answers_are(mt_eval(m, twinned(mt_keep(abc))), E(twinned(mt_keep(abc)))) && "the inverse went with it");
    mt_drop(one), mt_drop(abc);
    mt_close(m);
    return 0;
}
