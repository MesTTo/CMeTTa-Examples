/* Purpose: a translator rule that declares its direction, and a
 *   bidirectional rule whose inverse the engine derives. celsius is
 *   left-to-right, as a rule is by default, and KELVIN is its body written
 *   once: over lowering.h's atom builders it is the rule's expansion, and
 *   over its C operators it is the kelvin C expects. unpack is one
 *   declaration read both ways, so which way a call goes is decided by the
 *   form's cost, and C decides it with costs.h, the engine's node count and
 *   orientation rule in C: each call answers the other side of the rule when
 *   that costs strictly less, and itself otherwise, through the written door
 *   and the eval and reduce doors alike. Withdrawing the rule withdraws the
 *   inverse, so the large twin form is left as written.
 * Guarantees: all eight claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "costs.h"

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
    metta *m = open_engine();
    require("(: celsius (-> Atom %Undefined%))", mt_add(m, E(":", "celsius", E("->", "Atom", "%Undefined%"))));
    require("celsius's equation",
            mt_add(m, E("=", E("celsius", E("degrees", V("c"))), E("noeval", E("kelvin", KELVIN(T_ADD, V("c")))))));
    require("a forward rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", "celsius", options("forward")))));
    const int64_t degrees = 27;
    check_answers("a forward rule fires as written", mt_eval(m, E("celsius", E("degrees", degrees))),
                  E("kelvin", KELVIN(C_ADD, degrees)));

    require("(: unpack (-> Atom %Undefined%))", mt_add(m, E(":", "unpack", E("->", "Atom", "%Undefined%"))));
    require("unpack's one equation", mt_add(m, E("=", unpacked(V("x")), E("noeval", twinned(V("x"))))));
    require("a bidirectional rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", "unpack", options("bidirectional")))));

    mt_atom *one = N(1), *abc = E("a", "b", "c");
    check_answers("four nodes against three goes forwards", mt_eval(m, unpacked(mt_keep(one))), from_unpacked(mt_keep(one)));
    check_answers("seven against six goes back", mt_eval(m, twinned(mt_keep(abc))), from_twinned(mt_keep(abc)));
    check_answers("a call at its cheapest is left alone", mt_eval(m, twinned(mt_keep(one))), from_twinned(mt_keep(one)));
    check_answers("from either side", mt_eval(m, unpacked(mt_keep(abc))), from_unpacked(mt_keep(abc)));
    check_answers("the eval door blocks the same up-rewrite", mt_eval(m, E("eval", twinned(mt_keep(one)))),
                  from_twinned(mt_keep(one)));
    check_answers("and so does reduce", mt_eval(m, E("reduce", twinned(mt_keep(one)))), from_twinned(mt_keep(one)));

    require("withdraw the rule", mt_one_truth(mt_eval(m, E("remove-translator-rule!", "unpack"))));
    check_answers("the inverse went with it", mt_eval(m, twinned(mt_keep(abc))), twinned(mt_keep(abc)));
    mt_drop(one), mt_drop(abc);
    return done(m);
}
