/* Purpose: a match inside a match. Every chain of two friendships is
 *   rewritten into one (transitive a b c) fact: an outer cursor over
 *   (friend $a $b), and for each row a fresh inner cursor over (friend b $c)
 *   that sees the removals earlier rows made. hide, the original's way to
 *   silence a `!` form, is an equation whose body answers nothing.
 * Guarantees: the two chains become (transitive sim som sam) and
 *   (transitive tim tom tam), sorted in the engine's order
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (hide $x) (empty))", mt_add(m, E("=", E("hide", V("x")), E("empty"))));
    assert(!mt_first(mt_eval(m, E("hide", "anything"))) && mt_ok() && "hide answers nothing");

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
    assert(list_is(chains, E(E("transitive", "sim", "som", "sam"), E("transitive", "tim", "tom", "tam")))
           && "every chain became one fact");
    mt_close(m);
    return 0;
}
