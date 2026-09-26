/* Purpose: a pattern child that stands for a run of children. `...` is an
 *   anonymous gap and (:seg $x) a named one, spelled GAP() and seg("x") in
 *   C. A gap reads every arity a head has; a named gap answers the run it
 *   took, which C takes as a slice view of the same children; two gaps
 *   around a separator answer once per separator, which a C loop counts;
 *   a repeated gap must repeat its run; unify reads a gap on either side;
 *   outside the proved finite fragments an ask refuses; and a marker a
 *   variable carries is data.
 * Guarantees: all twelve claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_fixtures/segments.h"

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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *seven = E("Order", 7, "x", "y"), *eight = E("Order", 8);
    require("(Order 7 x y)", mt_add(m, mt_keep(seven)));
    require("(Order 8)", mt_add(m, mt_keep(eight)));
    require("(Order 9 z)", mt_add(m, E("Order", 9, "z")));
    require("(Note 1)", mt_add(m, E("Note", 1)));

    assert(answers_are(mt_eval(m, E("match", "&self", E("Order", GAP()), "matched")), E("matched", "matched", "matched"))
           && "a gap reads every arity");
    assert(answers_are(mt_eval(m, E("match", "&self", E("Order", 8, seg("rest")), V("rest"))), E(run(eight, 2, mt_len(eight))))
           && "a named gap answers the empty run");
    assert(answers_are(mt_eval(m, E("match", "&self", E("Order", 7, seg("rest")), V("rest"))), E(run(seven, 2, mt_len(seven))))
           && "and a longer one");
    assert(answers_are(mt_eval(m, E("match", "&self", E("Note", GAP()), "matched")), E("matched")) && "the gap does not widen the head");

    mt_atom *row = E("a", "b", "SEP", "c", "SEP", "d");
    mt_atom **splits = malloc(mt_len(row) * sizeof *splits);
    require("room for an answer per separator", splits != NULL);
    size_t n = 0;
    for (size_t i = 1; i + 1 < mt_len(row); i++)
        if (strcmp(mt_name(mt_at(row, i)), "SEP") == 0)
            splits[n++] = E(mt_keep(mt_at(row, 0)), mt_keep(mt_at(row, mt_len(row) - 1)));
    assert(list_is(mt_all(mt_eval(m, E("let", E(V("pre"), GAP(), "SEP", GAP(), V("post")), row, E(V("pre"), V("post"))))), mt_exprv(n, splits))
           && "one answer per separator");
    free(splits);

    mt_atom *twice = E("f", "a", "b", "mid", "a", "b");
    assert(answers_are(mt_eval(m, E("let", E("f", seg("run"), "mid", seg("run")), mt_keep(twice), V("run"))), E(run(twice, 1, 3)))
           && "a repeated gap repeats its run");
    assert(!mt_first(mt_eval(m, E("let", E("f", seg("run"), "mid", seg("run")), E("f", "a", "b", "mid", "c"), V("run")))) && mt_ok()
           && "or answers nothing");
    assert(answers_are(mt_eval(m, E("case", mt_keep(seven), E(E(E("Order", V("id"), seg("rest")), E(V("id"), V("rest"))),
                                                              E(V("_"), "nope")))), E(E(7, run(seven, 2, mt_len(seven)))))
           && "a case arm reads a run");
    assert(answers_are(mt_eval(m, E("unify", E("f", "a", seg("u")), E("f", "a", "b", seg("v")), V("u"), "none")), E(E("b", seg("v"))))
           && "unify reads a gap on either side");
    assert(answers_are(mt_eval(m, E("get-metatype", E("catch", E("unify", E("f", seg("x"), "a"),
                                                                  E("f", "a", seg("x")), "yes", "no")))), E("Expression"))
           && "the fence refuses");

    require("(Marked ... tail)", mt_add(m, E("Marked", GAP(), "tail")));
    assert(answers_are(mt_eval(m, E("match", "&self", E("Marked", V("slot"), "tail"), V("slot"))), E(GAP()))
           && "a marker a variable carries is data");
    mt_drop(seven);
    mt_drop(eight);
    mt_drop(twice);
    mt_close(m);
    return 0;
}
