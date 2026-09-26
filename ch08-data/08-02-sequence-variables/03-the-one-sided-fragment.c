/* Purpose: the fragment every gap ask lands in, one side carrying no gap.
 *   Matching enumerates the splits shortest first, which C finds by scanning
 *   for the separator; a run may be empty; a run is an ordinary expression
 *   C reads with mt_len and mt_at; a repeated gap compares its runs as the
 *   engine compares, so 1 and 1.0 agree; anonymous gaps are distinct; a
 *   nested gap matches inside a child; and a gap pattern joins a space.
 * Guarantees: all fifteen claims of the original hold
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *row = E("a", "b", "SEP", "c", "SEP", "d"), *splits[4];
    size_t n = 0;
    for (size_t i = 0; i < mt_len(row); i++)
        if (strcmp(mt_name(mt_at(row, i)), "SEP") == 0)
            splits[n++] = E("pair", run(row, 0, i), run(row, i + 1, mt_len(row)));
    assert(list_is(mt_all(mt_eval(m, E("let", E(seg("pre"), "SEP", seg("post")), mt_keep(row), E("pair", V("pre"), V("post"))))), mt_exprv(n, splits))
           && "shortest first, one answer per split");

    assert(answers_are(mt_eval(m, E("let", E("row", seg("r")), E("row"), V("r"))), E(mt_unit())) && "zero children is a run");
    assert(answers_are(mt_eval(m, E("let", E("row", seg("r")), E("row", "a", "b", "c"), V("r"))), E(E("a", "b", "c"))) && "so is every child");

    mt_atom *taken = mt_one(mt_eval(m, E("let", E("row", seg("r")), E("row", "a", "b"), V("r"))));
    assert(answers_are(mt_eval(m, E("get-metatype", E("let", E("row", seg("r")), E("row", "a", "b"), V("r")))), E("Expression"))
           && "a run is an Expression");
    assert((int64_t)mt_len(taken) == 2 && "of two children, which C counts");
    assert(atom_is(mt_keep(mt_at(taken, 0)), S("a")) && "and indexes");
    mt_drop(taken);

    assert(answers_are(mt_eval(m, E("let", E("f", seg("x"), "g", seg("x")), E("f", "a", "b", "g", "a", "b"), V("x"))), E(E("a", "b")))
           && "a repeated gap repeats its run");
    assert(!mt_first(mt_eval(m, E("let", E("f", seg("x"), "g", seg("x")), E("f", "a", "b", "g", "c"), V("x")))) && mt_ok() && "or nothing answers");
    assert(answers_are(mt_eval(m, E("let", E("f", seg("x"), "g", seg("x")), E("f", 1, "g", 1.0), "took")), E("took"))
           && "compared as the engine compares, 1 and 1.0 alike");
    assert(answers_are(mt_eval(m, E("let", E("f", GAP(), "g", GAP()), E("f", "a", "g", "b", "c"), "done")), E("done"))
           && "anonymous gaps are distinct");
    assert(answers_are(mt_eval(m, E("let", E("f", E("g", GAP()), "b"), E("f", E("g", 1, 2), "b"), "nested")), E("nested"))
           && "a gap inside a child");
    assert(!mt_first(mt_eval(m, E("let", E("A", GAP(), "D"), E("A", "b", "c", "E"), "never"))) && mt_ok() && "a settled child refutes every split");

    require("(edge a b)", mt_add(m, E("edge", "a", "b")));
    require("(edge b c d)", mt_add(m, E("edge", "b", "c", "d")));
    require("(tag b hot)", mt_add(m, E("tag", "b", "hot")));
    assert(answers_are(mt_eval(m, E("match", "&self", E("edge", "a", GAP(), V("last")), V("last"))), E("b")) && "a gap pattern against a space");
    assert(answers_are(mt_eval(m, E("match", "&self", E(",", E("edge", GAP(), V("mid")), E("tag", V("mid"), V("heat"))),
                                    E(V("mid"), V("heat")))), E(E("b", "hot")))
           && "joined with a conjunct");
    mt_drop(row);
    mt_close(m);
    return 0;
}
