/* Purpose: short-circuiting. and-then and or-else are special forms, so the
 *   branch they skip never runs; note and note2 are C functions that record
 *   their tag in a C array each time they run, so what ran is C's own
 *   record, and it holds the taken branches only. and runs its second
 *   argument even after a False, and-then does not; and and-then, unlike
 *   and, cannot be solved backwards.
 * Guarantees: all nine claims of the original hold, with the six runs'
 *   answers checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish note", mt_def(m, (mt_op){ .name = "note", .arity = 1, .effect = MT_EFFECT_CLASS_WRITES_STATE,
                                             .fn = note, .user = &ran }));
    require("publish note2", mt_def(m, (mt_op){ .name = "note2", .arity = 1, .effect = MT_EFFECT_CLASS_WRITES_STATE,
                                              .fn = note, .user = &ran2 }));

    assert(answers_are(mt_eval(m, E("and-then", B(true), "yes")), E("yes")) && "(and-then True yes)");
    assert(answers_are(mt_eval(m, E("and-then", B(false), "yes")), E(B(false))) && "(and-then False yes)");
    assert(answers_are(mt_eval(m, E("or-else", B(true), "no")), E(B(true))) && "(or-else True no)");
    assert(answers_are(mt_eval(m, E("or-else", B(false), "fallback")), E("fallback")) && "(or-else False fallback)");
    assert(answers_are(mt_eval(m, E("and-then", E(">", 2, 1), E(">", 3, 2))), E(B(true))) && "they take expressions");
    assert(answers_are(mt_eval(m, E("or-else", E(">", 1, 2), E(">", 3, 2))), E(B(true))) && "on either side");

    assert(answers_are(mt_eval(m, E("and-then", B(false), E("note", "skipped-by-and-then"))), E(B(false))) && "and-then False skips");
    assert(answers_are(mt_eval(m, E("or-else", B(true), E("note", "skipped-by-or-else"))), E(B(true))) && "or-else True skips");
    assert(answers_are(mt_eval(m, E("and-then", B(true), E("note", "taken-by-and-then"))), E(B(true))) && "and-then True runs");
    assert(answers_are(mt_eval(m, E("or-else", B(false), E("note", "taken-by-or-else"))), E(B(true))) && "or-else False runs");
    assert(recorded(&ran, 2, (const char *const[]){ "taken-by-and-then", "taken-by-or-else" })
           && "only the taken branches ran");

    /* A cursor is lazy, so each form runs as its answer is read. */
    assert(answers_are(mt_eval(m, E("and", B(false), E("note2", "and-runs-it"))), E(B(false))) && "and is False");
    assert(answers_are(mt_eval(m, E("and-then", B(false), E("note2", "and-then-skips-it"))), E(B(false))) && "and-then is False");
    assert(recorded(&ran2, 1, (const char *const[]){ "and-runs-it" })
           && "and runs its second argument; and-then does not");

    assert(!mt_first(mt_eval(m, E("if", E("and-then", E("or-else", V("p"), B(true)), V("q")), E(V("p"), V("q"))))) && mt_ok()
           && "and-then cannot be solved backwards");
    mt_close(m);
    return 0;
}
