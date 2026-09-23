/* Purpose: a match inside a match. Every chain of two friendships is
 *   rewritten into one (transitive a b c) fact: an outer cursor over
 *   (friend $a $b), and for each row a fresh inner cursor over (friend b $c)
 *   that sees the removals earlier rows made. hide, the original's way to
 *   silence a `!` form, is an equation whose body answers nothing.
 * Guarantees: the two chains become (transitive sim som sam) and
 *   (transitive tim tom tam), sorted in the engine's order [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(= (hide $x) (empty))", mt_add(m, E("=", E("hide", V("x")), E("empty"))));
    check_none("hide answers nothing", mt_eval(m, E("hide", "anything")));

    static const char *const friends[][2] = { {"tim", "tom"}, {"tom", "tam"}, {"sim", "som"}, {"som", "sam"} };
    for (size_t i = 0; i < 4; i++) require("a friendship", mt_add(m, E("friend", friends[i][0], friends[i][1])));

    /* The outer rows are collected first, as match does before any template runs. */
    mt_list outer = mt_all(mt_match(m, E("friend", V("a"), V("b"))));
    for (size_t i = 0; i < outer.len; i++) {
        const mt_atom *a = mt_at(outer.items[i], 1), *b = mt_at(outer.items[i], 2);
        mt_list inner = mt_all(mt_match(m, E("friend", mt_keep(b), V("c"))));
        for (size_t j = 0; j < inner.len; j++) {
            const mt_atom *c = mt_at(inner.items[j], 2);
            require("add the chain", mt_add(m, E("transitive", mt_keep(a), mt_keep(b), mt_keep(c))));
            require("remove its first link", mt_del(m, E("friend", mt_keep(a), mt_keep(b))));
            require("and its second", mt_del(m, E("friend", mt_keep(b), mt_keep(c))));
        }
        mt_list_free(inner);
    }
    mt_list_free(outer);

    mt_list chains = mt_all(mt_match(m, E("transitive", V("a"), V("b"), V("c"))));
    qsort(chains.items, chains.len, sizeof *chains.items, mt_order);
    check_list("every chain became one fact", chains,
               E("transitive", "sim", "som", "sam"), E("transitive", "tim", "tom", "tam"));
    return done(m);
}
