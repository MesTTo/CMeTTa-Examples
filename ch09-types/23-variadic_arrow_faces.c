/* Purpose: a segment parameter is checked element by element. vsum's
 *   (:seg Number) admits any count of numbers, whose sum C takes itself, and
 *   refuses a Bool at whichever position it stands, the position C finds by
 *   scanning the arguments. An (:seg Atom) run is held as written, and a
 *   fixed prefix before it is evaluated. A fixed arrow still refuses an
 *   extra argument, and C builds that error from fixed2's own arrow: its
 *   parameter count and the count the call supplied.
 * Guarantees: all nine claims of the original hold
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: vsum (-> (:seg Number) Number))", mt_add(m, E(":", "vsum", E("->", E(":seg", "Number"), "Number"))));
    require("(= (vsum (:seg $ns)) (foldl-atom $ns 0 +))", mt_add(m, E("=", E("vsum", E(":seg", V("ns"))), E("foldl-atom", V("ns"), 0, "+"))));
    mt_atom *sums[4], *want[4];
    for (int64_t n = 0; n < 4; n++) {
        mt_atom *call[4] = { S("vsum") };
        for (int64_t i = 0; i < n; i++) call[1 + i] = N(i + 1);
        sums[n] = mt_exprv((size_t)n + 1, call);
        want[n] = vsum(sums[n]);
    }
    assert(answers_are(mt_eval(m, mt_exprv(4, sums)), E(mt_exprv(4, want))) && "a sum at every arity");

    require("(: vflag Bool)", mt_add(m, E(":", "vflag", "Bool")));
    mt_atom *collapses[3], *errors[3];
    for (size_t at = 0; at < 3; at++) {
        mt_atom *call[4] = { S("vsum") };
        for (size_t i = 0; i < 3; i++) call[1 + i] = i == at ? S("vflag") : N((int64_t)i + 1);
        mt_atom *c = mt_exprv(4, call);
        errors[at] = E(vsum(c));
        collapses[at] = E("collapse", c);
    }
    assert(answers_are(mt_eval(m, mt_exprv(3, collapses)), E(mt_exprv(3, errors))) && "a Bool is refused at its own position");

    require("(: vheld (-> (:seg Atom) Atom))", mt_add(m, E(":", "vheld", E("->", E(":seg", "Atom"), "Atom"))));
    require("(= (vheld (:seg $es)) $es)", mt_add(m, E("=", E("vheld", E(":seg", V("es"))), V("es"))));
    assert(answers_are(mt_eval(m, E("vheld")), E(mt_unit())) && "an empty held run");
    mt_atom *held = E(E("+", 1, 1), E("+", 2, 2));
    assert(answers_are(mt_eval(m, E("vheld", mt_keep(mt_at(held, 0)), mt_keep(mt_at(held, 1)))), E(mt_keep(held))) && "a held run stays as written");
    assert(answers_are(mt_eval(m, E("get-type", "vheld")), E(E("->", E(":seg", "Atom"), "Atom"))) && "vheld's arrow");

    require("(: mixed (-> Number (:seg Atom) Atom))", mt_add(m, E(":", "mixed", E("->", "Number", E(":seg", "Atom"), "Atom"))));
    require("(= (mixed $n (:seg $xs)) (kept $n $xs))", mt_add(m, E("=", E("mixed", V("n"), E(":seg", V("xs"))), E("kept", V("n"), V("xs")))));
    mt_atom *tail = E(E("+", 2, 2), E("+", 3, 3));
    assert(answers_are(mt_eval(m, E("mixed", E("+", 1, 1), mt_keep(mt_at(tail, 0)), mt_keep(mt_at(tail, 1)))), E(E("kept", 1 + 1, mt_keep(tail))))
           && "the prefix evaluates and the tail is held");
    assert(answers_are(mt_eval(m, E("mixed", 2)), E(E("kept", 2, mt_unit()))) && "an empty tail");

    mt_atom *arrow = E("->", "Number", "Number", "Number");
    require("(: fixed2 (-> Number Number Number))", mt_add(m, E(":", "fixed2", mt_keep(arrow))));
    require("(= (fixed2 $a $b) (+ $a $b))", mt_add(m, E("=", E("fixed2", V("a"), V("b")), E("+", V("a"), V("b")))));
    assert(answers_are(mt_eval(m, E("fixed2", 1, 2)), E(N(1 + 2))) && "a fixed arrow");
    mt_atom *over = E("fixed2", 1, 2, 3);
    int64_t parameters = (int64_t)mt_len(arrow) - 2, supplied = (int64_t)mt_len(over) - 1;
    mt_atom *error = mt_one(mt_eval(m, E("catch", mt_keep(over))));
    assert(atom_is(error, E("Error", E("domain_error", E("function_input_arities", "fixed2", E(parameters)), supplied), "none")) && "refuses an extra argument");
    mt_drop(held), mt_drop(tail), mt_drop(arrow), mt_drop(over);
    mt_close(m);
    return 0;
}
