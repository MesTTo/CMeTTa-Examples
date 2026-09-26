/* Purpose: a gap in an equation head makes a function of variable arity.
 *   The heads are built from C with seg(); the run a head takes is a slice
 *   view C takes of the same call; two gaps answer once per separator, the
 *   positions a C loop finds; a bare $xs keeps the run as one expression
 *   while a written (:seg $xs) splices it; one name may be both a gap and a
 *   term here; and a gap head is additive with an ordinary one.
 * Guarantees: all thirteen claims of the original hold
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
    require("allof", mt_add(m, E("=", E("allof", seg("xs")), E("kept", V("xs")))));
    require("middle", mt_add(m, E("=", E("middle", E("row", "start", seg("mid"), "end")), V("mid"))));
    require("split", mt_add(m, E("=", E("split", E("row", seg("before"), "SEP", seg("after"))), E("pair", V("before"), V("after")))));
    require("project", mt_add(m, E("=", E("project", E("head", seg("xs"), "tail")), E("rebuilt", "before", V("xs"), "after"))));
    require("splice", mt_add(m, E("=", E("splice", E("head", seg("xs"), "tail")), E("rebuilt", "before", seg("xs"), "after"))));
    require("echoes", mt_add(m, E("=", E("echoes", E(seg("xs"), "tag", V("xs"))), "yes")));
    require("kind of a row", mt_add(m, E("=", E("kind", E("row", GAP())), "row-shaped")));
    require("kind of anything", mt_add(m, E("=", E("kind", V("other")), "anything")));

    mt_atom *none = E("allof"), *one = E("allof", "a"), *three = E("allof", "a", "b", "c");
    assert(answers_are(mt_eval(m, mt_keep(none)), E(E("kept", run(none, 1, 1)))) && "no arguments");
    assert(answers_are(mt_eval(m, mt_keep(one)), E(E("kept", run(one, 1, 2)))) && "one");
    assert(answers_are(mt_eval(m, mt_keep(three)), E(E("kept", run(three, 1, 4)))) && "three");

    mt_atom *filled = E("row", "start", "a", "b", "end"), *bare = E("row", "start", "end");
    assert(answers_are(mt_eval(m, E("middle", mt_keep(filled))), E(run(filled, 2, 4))) && "the run between fixed children");
    assert(answers_are(mt_eval(m, E("middle", mt_keep(bare))), E(run(bare, 2, 2))) && "which may be empty");

    mt_atom *splittable = E("row", "a", "SEP", "b", "SEP", "c"), *splits[4];
    size_t n = 0;
    for (size_t i = 1; i < mt_len(splittable); i++)
        if (strcmp(mt_name(mt_at(splittable, i)), "SEP") == 0)
            splits[n++] = E("pair", run(splittable, 1, i), run(splittable, i + 1, mt_len(splittable)));
    assert(list_is(mt_all(mt_eval(m, E("split", mt_keep(splittable)))), mt_exprv(n, splits)) && "one answer per split, shortest first");

    mt_atom *written = E("head", "a", "b", "tail");
    assert(answers_are(mt_eval(m, E("project", mt_keep(written))), E(E("rebuilt", "before", run(written, 1, 3), "after"))) && "$xs is one expression");
    assert(answers_are(mt_eval(m, E("splice", mt_keep(written))), E(E("rebuilt", "before", "a", "b", "after"))) && "(:seg $xs) splices");

    assert(answers_are(mt_eval(m, E("echoes", E("a", "b", "tag", E("a", "b")))), E("yes")) && "one name in both roles");
    assert(answers_are(mt_eval(m, E("echoes", E("tag", mt_unit()))), E("yes")) && "with an empty run");
    assert(!mt_first(mt_eval(m, E("echoes", E("a", "b", "tag", E("a", "c"))))) && mt_ok() && "and a tail that differs");
    assert(answers_are(mt_eval(m, E("kind", E("row", "a", "b"))), E("row-shaped", "anything")) && "a gap head and an ordinary one both answer");
    assert(answers_are(mt_eval(m, E("kind", 7)), E("anything")) && "only the ordinary one fits 7");
    mt_atom *all[] = { none, one, three, filled, bare, splittable, written };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    mt_close(m);
    return 0;
}
