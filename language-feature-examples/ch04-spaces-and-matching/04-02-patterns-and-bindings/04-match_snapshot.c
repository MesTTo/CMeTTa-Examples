/* Purpose: a query is a snapshot. A C loop walks the cursor over a cycle of
 *   links and reverses each link it is given, writing to the space the
 *   cursor reads; the cursor still answers all three rows it found, so the
 *   whole cycle is reversed and (link C E) is left alone. Then visit, a C
 *   function the engine calls from match's template, removes the other item
 *   of &snapshot each time, and both rows are still answered.
 * Guarantees: three reversals, the four links the original prints, visit
 *   answers alpha and beta, and &snapshot ends empty [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
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
    check_int("all three rows of the cycle were answered", (int64_t)reversed, 3);
    check_answers("so the cycle is reversed and (link C E) left alone",
                  mt_eval(m, E("match", "&self", E("link", V("x"), V("y")), E(V("x"), V("y")))),
                  E("C", "E"), E("B", "A"), E("C", "B"), E("A", "C"));

    /* Two rows whose templates each remove the other one. */
    mt_space *snapshot = mt_space_open(m, "&snapshot");
    require("open &snapshot", snapshot != NULL);
    require("(item alpha)", mt_add(snapshot, E("item", "alpha")));
    require("(item beta)", mt_add(snapshot, E("item", "beta")));
    require("publish visit", mt_def(m, (mt_op){ .name = "visit", .arity = 1, .effect = MT_WRITES,
                                                .fn = visit, .user = snapshot }));
    check_answers("both rows are answered though each template removes the other",
                  mt_eval(m, E("match", mt_spaceref("&snapshot"), E("item", V("x")), E("visit", V("x")))),
                  "alpha", "beta");
    check_int("and both removals happened", (int64_t)mt_count(snapshot), 0);
    mt_space_close(snapshot);
    return done(m);
}
