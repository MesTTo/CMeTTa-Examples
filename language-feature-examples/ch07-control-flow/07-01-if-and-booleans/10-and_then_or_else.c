/* Purpose: short-circuiting. and-then and or-else are special forms, so the
 *   branch they skip never runs; note and note2 are C functions that record
 *   their tag in a C array each time they run, so what ran is C's own
 *   record, and it holds the taken branches only. and runs its second
 *   argument even after a False, and-then does not; and and-then, unlike
 *   and, cannot be solved backwards.
 * Guarantees: all nine claims of the original hold, with the six runs'
 *   answers checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct { mt_atom *tag[8]; size_t n; } record;
static record ran, ran2;

/* (note tag): record the tag, answer True. The argument is borrowed for the
   call, so the record keeps its own reference. */
static mt_status note(mt_call *call, void *user)
{
    record *r = user;
    if (r->n == sizeof r->tag / sizeof *r->tag) return mt_fail(call, "the record is full");
    r->tag[r->n++] = mt_keep(mt_arg(call, 0));
    return mt_answer(call, B(true));
}

/* Whether the record holds exactly these tags in order; it is emptied. */
static bool recorded(record *r, size_t n, const char *const *want)
{
    bool same = r->n == n;
    for (size_t i = 0; i < r->n; i++) {
        same = same && strcmp(mt_name(r->tag[i]), want[i]) == 0;
        mt_drop(r->tag[i]);
    }
    r->n = 0;
    return same;
}

int main(void)
{
    metta *m = open_engine();
    require("publish note", mt_def(m, (mt_op){ .name = "note", .arity = 1, .effect = MT_WRITES,
                                             .fn = note, .user = &ran }));
    require("publish note2", mt_def(m, (mt_op){ .name = "note2", .arity = 1, .effect = MT_WRITES,
                                              .fn = note, .user = &ran2 }));

    check_answers("(and-then True yes)", mt_eval(m, E("and-then", B(true), "yes")), "yes");
    check_answers("(and-then False yes)", mt_eval(m, E("and-then", B(false), "yes")), B(false));
    check_answers("(or-else True no)", mt_eval(m, E("or-else", B(true), "no")), B(true));
    check_answers("(or-else False fallback)", mt_eval(m, E("or-else", B(false), "fallback")), "fallback");
    check_answers("they take expressions", mt_eval(m, E("and-then", E(">", 2, 1), E(">", 3, 2))), B(true));
    check_answers("on either side", mt_eval(m, E("or-else", E(">", 1, 2), E(">", 3, 2))), B(true));

    check_answers("and-then False skips", mt_eval(m, E("and-then", B(false), E("note", "skipped-by-and-then"))), B(false));
    check_answers("or-else True skips", mt_eval(m, E("or-else", B(true), E("note", "skipped-by-or-else"))), B(true));
    check_answers("and-then True runs", mt_eval(m, E("and-then", B(true), E("note", "taken-by-and-then"))), B(true));
    check_answers("or-else False runs", mt_eval(m, E("or-else", B(false), E("note", "taken-by-or-else"))), B(true));
    check("only the taken branches ran",
          recorded(&ran, 2, (const char *const[]){ "taken-by-and-then", "taken-by-or-else" }));

    /* A cursor is lazy, so each form runs as its answer is read. */
    check_answers("and is False", mt_eval(m, E("and", B(false), E("note2", "and-runs-it"))), B(false));
    check_answers("and-then is False", mt_eval(m, E("and-then", B(false), E("note2", "and-then-skips-it"))), B(false));
    check("and runs its second argument; and-then does not",
          recorded(&ran2, 1, (const char *const[]){ "and-runs-it" }));

    check_none("and-then cannot be solved backwards",
               mt_eval(m, E("if", E("and-then", E("or-else", V("p"), B(true)), V("q")), E(V("p"), V("q")))));
    return done(m);
}
