/* Purpose: one family program asked in every direction under five algebras.
 *   ancestor is two equations built as terms, and mt_eval_under() answers
 *   each result paired with its coefficient in the chosen carrier, so the
 *   same derivations are counted, costed, traced, ranked and weighed.
 * Guarantees: every direction keeps its answer count and every coefficient
 *   is its carrier's identity for these one-derivation answers
 *   [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(Parent Tom Bob)", mt_add(m, E("Parent", "Tom", "Bob")));
    require("(Parent Bob Ann)", mt_add(m, E("Parent", "Bob", "Ann")));
    /* (= (ancestor $x $y) (match &self (Parent $x $y) True))
       (= (ancestor $x $y) (match &self (Parent $x $z) (ancestor $z $y))) */
    require("ancestor, directly", mt_add(m, E("=", E("ancestor", V("x"), V("y")),
        E("match", "&self", E("Parent", V("x"), V("y")), B(true)))));
    require("ancestor, through a parent", mt_add(m, E("=", E("ancestor", V("x"), V("y")),
        E("match", "&self", E("Parent", V("x"), V("z")), E("ancestor", V("z"), V("y"))))));

    struct { const char *carrier; mt_atom *identity; } algebras[] = {
        { "counting", N(1) }, { "tropical", N(0) }, { "prov", S("one") },
        { "ranked", N(1) },   { "prob", N(1) },
    };
    struct { mt_atom *goal; size_t answers; } directions[] = {
        { E("ancestor", "Tom", "Ann"), 1 },     /* is it so? */
        { E("ancestor", V("x"), "Ann"), 2 },    /* who are Ann's ancestors? */
        { E("ancestor", "Tom", V("y")), 2 },    /* whose ancestor is Tom? */
        { E("ancestor", V("x"), V("y")), 3 },   /* every pair */
    };
    for (size_t a = 0; a < 5; a++) {
        for (size_t d = 0; d < 4; d++) {
            mt_list rows = mt_all(mt_eval_under(m, S(algebras[a].carrier), mt_keep(directions[d].goal)));
            check("each direction keeps its answer count", mt_ok() && rows.len == directions[d].answers);
            for (size_t i = 0; i < rows.len; i++)
                check("each answer is True with the carrier's identity",
                      mt_len(rows.items[i]) == 2 &&
                      alpha_equal(mt_at(rows.items[i], 0), B(true)) &&
                      mt_alpha_eq(mt_at(rows.items[i], 1), algebras[a].identity));
            mt_list_free(rows);
        }
    }
    for (size_t a = 0; a < 5; a++) mt_drop(algebras[a].identity);
    for (size_t d = 0; d < 4; d++) mt_drop(directions[d].goal);
    return done(m);
}
