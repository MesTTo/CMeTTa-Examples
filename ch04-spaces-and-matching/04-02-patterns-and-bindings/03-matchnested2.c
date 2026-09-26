/* Purpose: the join, not the nesting. One conjunctive query,
 *   (, (friend $a $b) (friend $b $c)), finds every chain at once through
 *   mt_query(); the rows are read by name with mt_bound() into a C array of
 *   structs before any of them is rewritten.
 * Guarantees: the two chains become (transitive sim som sam) and
 *   (transitive tim tom tam) [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

typedef struct chain { mt_atom *a, *b, *c; } chain;

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (hide $x) (empty))", mt_add(m, E("=", E("hide", V("x")), E("empty"))));
    assert(!mt_first(mt_eval(m, E("hide", "anything"))) && mt_ok() && "hide answers nothing");
    static const char *const friends[][2] = { {"tim", "tom"}, {"tom", "tam"}, {"sim", "som"}, {"som", "sam"} };
    for (size_t i = 0; i < 4; i++) require("a friendship", mt_add(m, E("friend", friends[i][0], friends[i][1])));

    chain found[4];
    size_t count = 0;
    mt_rows (row, mt_query(m, E(",", E("friend", V("a"), V("b")), E("friend", V("b"), V("c"))), NULL))
        if (count < 4)
            found[count++] = (chain){ mt_keep(mt_bound(row, "a")), mt_keep(mt_bound(row, "b")),
                                      mt_keep(mt_bound(row, "c")) };
    for (size_t i = 0; i < count; i++) {
        require("add the chain", mt_add(m, E("transitive", mt_keep(found[i].a), mt_keep(found[i].b),
                                             mt_keep(found[i].c))));
        require("remove its first link", mt_del(m, E("friend", mt_keep(found[i].a), mt_keep(found[i].b))));
        require("and its second", mt_del(m, E("friend", found[i].b, found[i].c)));
        mt_drop(found[i].a);
    }

    mt_list chains = mt_all(mt_match(m, E("transitive", V("a"), V("b"), V("c"))));
    qsort(chains.items, chains.len, sizeof *chains.items, mt_order);
    assert(list_is(chains, E(E("transitive", "sim", "som", "sam"), E("transitive", "tim", "tom", "tam")))
           && "the join found both chains");
    mt_close(m);
    return 0;
}
