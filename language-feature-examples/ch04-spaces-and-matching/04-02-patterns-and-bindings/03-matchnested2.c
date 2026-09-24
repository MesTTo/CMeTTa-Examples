/* Purpose: the join, not the nesting. One conjunctive query,
 *   (, (friend $a $b) (friend $b $c)), finds every chain at once through
 *   mt_query(); the rows are read by name with mt_bound() into a C array of
 *   structs before any of them is rewritten.
 * Guarantees: the two chains become (transitive sim som sam) and
 *   (transitive tim tom tam) [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct chain { mt_atom *a, *b, *c; } chain;

int main(void)
{
    metta *m = open_engine();
    require("(= (hide $x) (empty))", mt_add(m, E("=", E("hide", V("x")), E("empty"))));
    check_none("hide answers nothing", mt_eval(m, E("hide", "anything")));
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
    check_list("the join found both chains", chains,
               E("transitive", "sim", "som", "sam"), E("transitive", "tim", "tom", "tam"));
    return done(m);
}
