/* Purpose: committing to the first answer. match-single wraps a match in
 *   once, and C does the original's let itself: it takes the one answer the
 *   call gives and stores (bar answer), so of two foos one bar is stored.
 *   From C, mt_first is once: it keeps the first answer of a lazy cursor
 *   and leaves the rest uncomputed.
 * Guarantees: the original's claim holds, and mt_first commits as once
 *   does [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static const int64_t FOO[] = { 1, 2 };

int main(void)
{
    metta *m = open_engine();
    for (size_t i = 0; i < sizeof FOO / sizeof *FOO; i++)
        require("(foo n)", mt_add(m, E("foo", FOO[i])));
    require("match-single", mt_lower(m, (match-single $space $pat $ret), (once (match $space $pat $ret))));

    mt_atom *first = mt_one(mt_eval(m, E("match-single", "&self", E("foo", V("n")), V("n"))));
    require("store (bar first)", first && mt_add(m, E("bar", first)));
    check_answers("one bar, from the first foo", mt_match(m, E("bar", V("n"))), E("bar", 1));
    check_atom("mt_first commits the same way", mt_first(mt_match(m, E("foo", V("n")))), E("foo", 1));
    return done(m);
}
