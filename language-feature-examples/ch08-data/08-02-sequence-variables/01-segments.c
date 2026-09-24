/* Purpose: a pattern child that stands for a run of children. `...` is an
 *   anonymous gap and (:seg $x) a named one, spelled GAP() and seg("x") in
 *   C. A gap reads every arity a head has; a named gap answers the run it
 *   took, which C takes as a slice view of the same children; two gaps
 *   around a separator answer once per separator, which a C loop counts;
 *   a repeated gap must repeat its run; unify reads a gap on either side;
 *   outside the proved finite fragments an ask refuses; and a marker a
 *   variable carries is data.
 * Guarantees: all twelve claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "segments.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *seven = E("Order", 7, "x", "y"), *eight = E("Order", 8);
    require("(Order 7 x y)", mt_add(m, mt_keep(seven)));
    require("(Order 8)", mt_add(m, mt_keep(eight)));
    require("(Order 9 z)", mt_add(m, E("Order", 9, "z")));
    require("(Note 1)", mt_add(m, E("Note", 1)));

    check_answers("a gap reads every arity", mt_eval(m, E("match", "&self", E("Order", GAP()), "matched")),
                  "matched", "matched", "matched");
    check_answers("a named gap answers the empty run", mt_eval(m, E("match", "&self", E("Order", 8, seg("rest")), V("rest"))),
                  run(eight, 2, mt_len(eight)));
    check_answers("and a longer one", mt_eval(m, E("match", "&self", E("Order", 7, seg("rest")), V("rest"))),
                  run(seven, 2, mt_len(seven)));
    check_answers("the gap does not widen the head", mt_eval(m, E("match", "&self", E("Note", GAP()), "matched")), "matched");

    mt_atom *row = E("a", "b", "SEP", "c", "SEP", "d");
    mt_atom **splits = malloc(mt_len(row) * sizeof *splits);
    require("room for an answer per separator", splits != NULL);
    size_t n = 0;
    for (size_t i = 1; i + 1 < mt_len(row); i++)
        if (strcmp(mt_name(mt_at(row, i)), "SEP") == 0)
            splits[n++] = E(mt_keep(mt_at(row, 0)), mt_keep(mt_at(row, mt_len(row) - 1)));
    check_list_("one answer per separator",
                mt_all(mt_eval(m, E("let", E(V("pre"), GAP(), "SEP", GAP(), V("post")), row, E(V("pre"), V("post"))))), n, splits);
    free(splits);

    mt_atom *twice = E("f", "a", "b", "mid", "a", "b");
    check_answers("a repeated gap repeats its run",
                  mt_eval(m, E("let", E("f", seg("run"), "mid", seg("run")), mt_keep(twice), V("run"))), run(twice, 1, 3));
    check_none("or answers nothing",
               mt_eval(m, E("let", E("f", seg("run"), "mid", seg("run")), E("f", "a", "b", "mid", "c"), V("run"))));
    check_answers("a case arm reads a run",
                  mt_eval(m, E("case", mt_keep(seven), E(E(E("Order", V("id"), seg("rest")), E(V("id"), V("rest"))),
                                                         E(V("_"), "nope")))),
                  E(7, run(seven, 2, mt_len(seven))));
    check_answers("unify reads a gap on either side",
                  mt_eval(m, E("unify", E("f", "a", seg("u")), E("f", "a", "b", seg("v")), V("u"), "none")), E("b", seg("v")));
    check_answers("the fence refuses", mt_eval(m, E("get-metatype", E("catch", E("unify", E("f", seg("x"), "a"),
                                                                                  E("f", "a", seg("x")), "yes", "no")))),
                  "Expression");

    require("(Marked ... tail)", mt_add(m, E("Marked", GAP(), "tail")));
    check_answers("a marker a variable carries is data", mt_eval(m, E("match", "&self", E("Marked", V("slot"), "tail"), V("slot"))),
                  GAP());
    mt_drop(seven);
    mt_drop(eight);
    mt_drop(twice);
    return done(m);
}
