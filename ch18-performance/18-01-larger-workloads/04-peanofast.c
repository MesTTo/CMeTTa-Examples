/* Purpose: Peano numerals as C builds them. expandK is a C function that
 *   wraps one more S around the numeral before it for each of its K atoms,
 *   so each numeral shares the one below it rather than copying it, and it
 *   stores all K through one mt_add_all batch where the original adds one a
 *   step. demo-peano stays the original's equation, expandK from Z, built as
 *   an atom. The original's claim counts the (num $1) atoms; C counts
 *   them by walking the match's cursor, and must find the K it built.
 * Guarantees: the original's claim holds, and the demo answers done as the
 *   original's does [tested 2026-09-27T00:35:58+10:00:
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

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

/* (num e), (num (S e)), ... one for each of the count steps, in one batch.
   The original counts down to 0, which a negative count never reaches: its
   recursion runs out of stack, and this refuses by name instead. */
static mt_status expand_k(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 1)) != MT_INT) return MT_FAIL;
    const int64_t count = mt_int(mt_arg(call, 1));
    if (count < 0) return mt_fail(call, "expandK counts down to 0, which a negative count never reaches");
    mt_list batch = { mt_alloc((size_t)count * sizeof *batch.items), (size_t)count };
    if (count > 0 && !batch.items) return mt_error_set(MT_NOMEM, "expandK has no room for its batch");
    mt_atom *numeral = mt_keep(mt_arg(call, 0));
    for (int64_t i = 0; i < count; i++) {
        batch.items[i] = E("num", mt_keep(numeral));
        numeral = E("S", numeral);
    }
    mt_drop(numeral);
    return mt_add_all(mt_of(call), batch) ? mt_answer(call, S("done")) : mt_error();
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("expandK", mt_def(m, (mt_op){ .name = "expandK", .arity = 2, .effect = MT_EFFECT_CLASS_WRITES_STATE,
                                          .fn = expand_k }));
    require("demo-peano", mt_add(m, E("=", E("demo-peano", V("K")), E("expandK", "Z", V("K")))));

    const int64_t count = 2500;
    assert(answers_are(mt_eval(m, E("demo-peano", count)), E(S("done"))) && "the demo builds its numerals");
    int64_t numerals = 0;
    mt_each (numeral, mt_match(m, E("num", V("n")))) numerals++;
    assert(numerals == count && "every numeral is one (num $1) atom");
    mt_close(m);
    return 0;
}
