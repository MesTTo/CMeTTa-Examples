/* Purpose: the fragment every gap ask lands in, one side carrying no gap.
 *   Matching enumerates the splits shortest first, which C finds by scanning
 *   for the separator; a run may be empty; a run is an ordinary expression
 *   C reads with mt_len and mt_at; a repeated gap compares its runs as the
 *   engine compares, so 1 and 1.0 agree; anonymous gaps are distinct; a
 *   nested gap matches inside a child; and a gap pattern joins a space.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "segments.h"

int main(void)
{
    metta *m = open_engine();
    mt_atom *row = E("a", "b", "SEP", "c", "SEP", "d"), *splits[4];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(row); i++)
        if (strcmp(mt_name(mt_at(row, i)), "SEP") == 0)
            splits[n++] = E("pair", run(row, 0, i), run(row, i + 1, mt_len(row)));
    check_list_("shortest first, one answer per split",
                mt_all(mt_eval(m, E("let", E(seg("pre"), "SEP", seg("post")), mt_keep(row), E("pair", V("pre"), V("post"))))),
                n, splits);

    check_answers("zero children is a run", mt_eval(m, E("let", E("row", seg("r")), E("row"), V("r"))), mt_unit());
    check_answers("so is every child", mt_eval(m, E("let", E("row", seg("r")), E("row", "a", "b", "c"), V("r"))), E("a", "b", "c"));

    mt_atom *taken = mt_one(mt_eval(m, E("let", E("row", seg("r")), E("row", "a", "b"), V("r"))));
    check_answers("a run is an Expression", mt_eval(m, E("get-metatype", E("let", E("row", seg("r")), E("row", "a", "b"), V("r")))),
                  "Expression");
    check_int("of two children, which C counts", (int64_t)mt_len(taken), 2);
    check_atom("and indexes", mt_keep(mt_at(taken, 0)), S("a"));
    mt_drop(taken);

    check_answers("a repeated gap repeats its run",
                  mt_eval(m, E("let", E("f", seg("x"), "g", seg("x")), E("f", "a", "b", "g", "a", "b"), V("x"))), E("a", "b"));
    check_none("or nothing answers", mt_eval(m, E("let", E("f", seg("x"), "g", seg("x")), E("f", "a", "b", "g", "c"), V("x"))));
    check_answers("compared as the engine compares, 1 and 1.0 alike",
                  mt_eval(m, E("let", E("f", seg("x"), "g", seg("x")), E("f", 1, "g", 1.0), "took")), "took");
    check_answers("anonymous gaps are distinct", mt_eval(m, E("let", E("f", GAP(), "g", GAP()), E("f", "a", "g", "b", "c"), "done")),
                  "done");
    check_answers("a gap inside a child", mt_eval(m, E("let", E("f", E("g", GAP()), "b"), E("f", E("g", 1, 2), "b"), "nested")),
                  "nested");
    check_none("a settled child refutes every split", mt_eval(m, E("let", E("A", GAP(), "D"), E("A", "b", "c", "E"), "never")));

    require("(edge a b)", mt_add(m, E("edge", "a", "b")));
    require("(edge b c d)", mt_add(m, E("edge", "b", "c", "d")));
    require("(tag b hot)", mt_add(m, E("tag", "b", "hot")));
    check_answers("a gap pattern against a space", mt_eval(m, E("match", "&self", E("edge", "a", GAP(), V("last")), V("last"))), "b");
    check_answers("joined with a conjunct",
                  mt_eval(m, E("match", "&self", E(",", E("edge", GAP(), V("mid")), E("tag", V("mid"), V("heat"))),
                               E(V("mid"), V("heat")))), E("b", "hot"));
    mt_drop(row);
    return done(m);
}
