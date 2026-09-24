/* Purpose: Peano numerals as C builds them. expandK is a C function that
 *   wraps one more S around the numeral before it for each of its K atoms,
 *   so each numeral shares the one below it rather than copying it, and it
 *   stores all K through one mt_add_all batch where the original adds one a
 *   step. demo-peano stays the original's equation, expandK from Z, lowered
 *   from C tokens. The original's claim counts the (num $1) atoms; C counts
 *   them by walking the match's cursor, and must find the K it built.
 * Guarantees: the original's claim holds, and the demo answers done as the
 *   original's does [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    require("expandK", mt_def(m, (mt_op){ .name = "expandK", .arity = 2, .effect = MT_EFFECT_CLASS_WRITES_STATE,
                                          .fn = expand_k }));
    require("demo-peano", mt_lower(m, (demo-peano $K), (expandK Z $K)));

    const int64_t count = 2500;
    check_answers("the demo builds its numerals", mt_eval(m, E("demo-peano", count)), S("done"));
    int64_t numerals = 0;
    mt_each (numeral, mt_match(m, E("num", V("n")))) numerals++;
    check_int("every numeral is one (num $1) atom", numerals, count);
    return done(m);
}
