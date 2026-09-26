/* Purpose: a query is a snapshot. A C loop walks the cursor over a cycle of
 *   links and reverses each link it is given, writing to the space the
 *   cursor reads; the cursor still answers all three rows it found, so the
 *   whole cycle is reversed and (link C E) is left alone. Then visit, a C
 *   function the engine calls from match's template, removes the other item
 *   of &snapshot each time, and both rows are still answered.
 * Guarantees: three reversals, the four links the original prints, visit
 *   answers alpha and beta, and &snapshot ends empty
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* visit x: remove the OTHER item from &snapshot, then answer x. */
static mt_status visit(mt_call *call, void *user)
{
    mt_space *snapshot = user;
    const char *item = mt_name(mt_arg(call, 0));
    if (!item) return mt_fail(call, "visit wants alpha or beta");
    const char *other = strcmp(item, "alpha") == 0 ? "beta" : "alpha";
    if (!mt_del(snapshot, E("item", other))) return mt_fail(call, "the other item was already gone");
    return mt_answer(call, mt_keep(mt_arg(call, 0)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const char *const links[][2] = { {"A", "B"}, {"B", "C"}, {"C", "A"}, {"C", "E"} };
    for (size_t i = 0; i < 4; i++) require("a link", mt_add(m, E("link", links[i][0], links[i][1])));

    /* Reverse each link of every cycle while the cursor is still open. */
    size_t reversed = 0;
    mt_rows (row, mt_query(m, E(",", E("link", V("x"), V("y")), E("link", V("y"), V("z")),
                               E("link", V("z"), V("x"))), NULL)) {
        const mt_atom *x = mt_bound(row, "x"), *y = mt_bound(row, "y");
        require("remove the link", mt_del(m, E("link", mt_keep(x), mt_keep(y))));
        require("add it reversed", mt_add(m, E("link", mt_keep(y), mt_keep(x))));
        reversed++;
    }
    assert((int64_t)reversed == 3 && "all three rows of the cycle were answered");
    assert(answers_are(mt_eval(m, E("match", "&self", E("link", V("x"), V("y")), E(V("x"), V("y")))), E(E("C", "E"), E("B", "A"), E("C", "B"), E("A", "C")))
           && "so the cycle is reversed and (link C E) left alone");

    /* Two rows whose templates each remove the other one. */
    mt_space *snapshot = mt_space_open(m, "&snapshot");
    require("open &snapshot", snapshot != NULL);
    require("(item alpha)", mt_add(snapshot, E("item", "alpha")));
    require("(item beta)", mt_add(snapshot, E("item", "beta")));
    require("publish visit", mt_def(m, (mt_op){ .name = "visit", .arity = 1, .effect = MT_EFFECT_CLASS_WRITES_STATE,
                                                .fn = visit, .user = snapshot }));
    assert(answers_are(mt_eval(m, E("match", mt_spaceref("&snapshot"), E("item", V("x")), E("visit", V("x")))), E("alpha", "beta"))
           && "both rows are answered though each template removes the other");
    assert((int64_t)mt_count(snapshot) == 0 && "and both removals happened");
    mt_space_close(snapshot);
    mt_close(m);
    return 0;
}
