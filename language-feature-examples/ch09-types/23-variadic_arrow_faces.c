/* Purpose: a segment parameter is checked element by element. vsum's
 *   (:seg Number) admits any count of numbers, whose sum C takes itself, and
 *   refuses a Bool at whichever position it stands, the position C finds by
 *   scanning the arguments. An (:seg Atom) run is held as written, and a
 *   fixed prefix before it is evaluated. A fixed arrow still refuses an
 *   extra argument, and C builds that error from fixed2's own arrow: its
 *   parameter count and the count the call supplied.
 * Guarantees: all nine claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* vsum's answer for integers and at most one Bool: the sum, or the error at
   the Bool's position. */
static mt_atom *vsum(const mt_atom *call)
{
    int64_t total = 0;
    for (size_t i = 1; i < mt_len(call); i++) {
        if (mt_kind_of(mt_at(call, i)) != MT_INT) return E("Error", mt_keep(call), E("BadArgType", (int64_t)i, "Number", "Bool"));
        total += mt_int(mt_at(call, i));
    }
    return N(total);
}

int main(void)
{
    metta *m = open_engine();
    require("(: vsum (-> (:seg Number) Number))", mt_add(m, E(":", "vsum", E("->", E(":seg", "Number"), "Number"))));
    require("(= (vsum (:seg $ns)) (foldl-atom $ns 0 +))", mt_add(m, E("=", E("vsum", E(":seg", V("ns"))), E("foldl-atom", V("ns"), 0, "+"))));
    mt_atom *sums[4], *want[4];
    for (int64_t n = 0; n < 4; n++) {
        mt_atom *call[4] = { S("vsum") };
        for (int64_t i = 0; i < n; i++) call[1 + i] = N(i + 1);
        sums[n] = mt_exprv((size_t)n + 1, call);
        want[n] = vsum(sums[n]);
    }
    check_answers("a sum at every arity", mt_eval(m, mt_exprv(4, sums)), mt_exprv(4, want));

    require("(: vflag Bool)", mt_add(m, E(":", "vflag", "Bool")));
    mt_atom *collapses[3], *errors[3];
    for (size_t at = 0; at < 3; at++) {
        mt_atom *call[4] = { S("vsum") };
        for (size_t i = 0; i < 3; i++) call[1 + i] = i == at ? S("vflag") : N((int64_t)i + 1);
        mt_atom *c = mt_exprv(4, call);
        errors[at] = E(vsum(c));
        collapses[at] = E("collapse", c);
    }
    check_answers("a Bool is refused at its own position", mt_eval(m, mt_exprv(3, collapses)), mt_exprv(3, errors));

    require("(: vheld (-> (:seg Atom) Atom))", mt_add(m, E(":", "vheld", E("->", E(":seg", "Atom"), "Atom"))));
    require("(= (vheld (:seg $es)) $es)", mt_add(m, E("=", E("vheld", E(":seg", V("es"))), V("es"))));
    check_answers("an empty held run", mt_eval(m, E("vheld")), mt_unit());
    mt_atom *held = E(E("+", 1, 1), E("+", 2, 2));
    check_answers("a held run stays as written", mt_eval(m, E("vheld", mt_keep(mt_at(held, 0)), mt_keep(mt_at(held, 1)))), mt_keep(held));
    check_answers("vheld's arrow", mt_eval(m, E("get-type", "vheld")), E("->", E(":seg", "Atom"), "Atom"));

    require("(: mixed (-> Number (:seg Atom) Atom))", mt_add(m, E(":", "mixed", E("->", "Number", E(":seg", "Atom"), "Atom"))));
    require("(= (mixed $n (:seg $xs)) (kept $n $xs))", mt_add(m, E("=", E("mixed", V("n"), E(":seg", V("xs"))), E("kept", V("n"), V("xs")))));
    mt_atom *tail = E(E("+", 2, 2), E("+", 3, 3));
    check_answers("the prefix evaluates and the tail is held", mt_eval(m, E("mixed", E("+", 1, 1), mt_keep(mt_at(tail, 0)), mt_keep(mt_at(tail, 1)))),
                  E("kept", 1 + 1, mt_keep(tail)));
    check_answers("an empty tail", mt_eval(m, E("mixed", 2)), E("kept", 2, mt_unit()));

    mt_atom *arrow = E("->", "Number", "Number", "Number");
    require("(: fixed2 (-> Number Number Number))", mt_add(m, E(":", "fixed2", mt_keep(arrow))));
    require("(= (fixed2 $a $b) (+ $a $b))", mt_add(m, E("=", E("fixed2", V("a"), V("b")), E("+", V("a"), V("b")))));
    check_answers("a fixed arrow", mt_eval(m, E("fixed2", 1, 2)), N(1 + 2));
    mt_atom *over = E("fixed2", 1, 2, 3);
    int64_t parameters = (int64_t)mt_len(arrow) - 2, supplied = (int64_t)mt_len(over) - 1;
    mt_atom *error = mt_one(mt_eval(m, E("catch", mt_keep(over))));
    check_atom("refuses an extra argument", error, E("Error", E("domain_error", E("function_input_arities", "fixed2", E(parameters)), supplied), "none"));
    mt_drop(held), mt_drop(tail), mt_drop(arrow), mt_drop(over);
    return done(m);
}
