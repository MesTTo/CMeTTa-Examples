/* Purpose: a gap in an equation head makes a function of variable arity.
 *   The heads are built from C with seg(); the run a head takes is a slice
 *   view C takes of the same call; two gaps answer once per separator, the
 *   positions a C loop finds; a bare $xs keeps the run as one expression
 *   while a written (:seg $xs) splices it; one name may be both a gap and a
 *   term here; and a gap head is additive with an ordinary one.
 * Guarantees: all thirteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "segments.h"

int main(void)
{
    metta *m = open_engine();
    require("allof", mt_add(m, E("=", E("allof", seg("xs")), E("kept", V("xs")))));
    require("middle", mt_add(m, E("=", E("middle", E("row", "start", seg("mid"), "end")), V("mid"))));
    require("split", mt_add(m, E("=", E("split", E("row", seg("before"), "SEP", seg("after"))), E("pair", V("before"), V("after")))));
    require("project", mt_add(m, E("=", E("project", E("head", seg("xs"), "tail")), E("rebuilt", "before", V("xs"), "after"))));
    require("splice", mt_add(m, E("=", E("splice", E("head", seg("xs"), "tail")), E("rebuilt", "before", seg("xs"), "after"))));
    require("echoes", mt_add(m, E("=", E("echoes", E(seg("xs"), "tag", V("xs"))), "yes")));
    require("kind of a row", mt_add(m, E("=", E("kind", E("row", GAP())), "row-shaped")));
    require("kind of anything", mt_add(m, E("=", E("kind", V("other")), "anything")));

    mt_atom *none = E("allof"), *one = E("allof", "a"), *three = E("allof", "a", "b", "c");
    check_answers("no arguments", mt_eval(m, mt_keep(none)), E("kept", run(none, 1, 1)));
    check_answers("one", mt_eval(m, mt_keep(one)), E("kept", run(one, 1, 2)));
    check_answers("three", mt_eval(m, mt_keep(three)), E("kept", run(three, 1, 4)));

    mt_atom *filled = E("row", "start", "a", "b", "end"), *bare = E("row", "start", "end");
    check_answers("the run between fixed children", mt_eval(m, E("middle", mt_keep(filled))), run(filled, 2, 4));
    check_answers("which may be empty", mt_eval(m, E("middle", mt_keep(bare))), run(bare, 2, 2));

    mt_atom *splittable = E("row", "a", "SEP", "b", "SEP", "c"), *splits[4];
    size_t n = 0;
    for (size_t i = 1; i < mt_len(splittable); i++)
        if (strcmp(mt_name(mt_at(splittable, i)), "SEP") == 0)
            splits[n++] = E("pair", run(splittable, 1, i), run(splittable, i + 1, mt_len(splittable)));
    check_list_("one answer per split, shortest first", mt_all(mt_eval(m, E("split", mt_keep(splittable)))), n, splits);

    mt_atom *written = E("head", "a", "b", "tail");
    check_answers("$xs is one expression", mt_eval(m, E("project", mt_keep(written))), E("rebuilt", "before", run(written, 1, 3), "after"));
    check_answers("(:seg $xs) splices", mt_eval(m, E("splice", mt_keep(written))), E("rebuilt", "before", "a", "b", "after"));

    check_answers("one name in both roles", mt_eval(m, E("echoes", E("a", "b", "tag", E("a", "b")))), "yes");
    check_answers("with an empty run", mt_eval(m, E("echoes", E("tag", mt_unit()))), "yes");
    check_none("and a tail that differs", mt_eval(m, E("echoes", E("a", "b", "tag", E("a", "c")))));
    check_answers("a gap head and an ordinary one both answer", mt_eval(m, E("kind", E("row", "a", "b"))), "row-shaped", "anything");
    check_answers("only the ordinary one fits 7", mt_eval(m, E("kind", 7)), "anything");
    mt_atom *all[] = { none, one, three, filled, bare, splittable, written };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    return done(m);
}
