/* Purpose: the target of a write is computed. space is a C function
 *   answering the symbol my_space_name, and add-atom evaluates its space
 *   argument, so the write lands in whatever space the function names; any
 *   symbol is a space name the moment it is written to. is-space asks the
 *   narrower question and wants the & prefix.
 * Guarantees: the atom lands in my_space_name, is-space says False for the
 *   bare name and True for &self [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status space(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, S("my_space_name"));
}

int main(void)
{
    metta *m = open_engine();
    require("publish space", mt_def(m, (mt_op){ .name = "space", .arity = 0,
                                                .effect = MT_PURE, .fn = space }));
    require("write through the computed space",
            mt_one_truth(mt_eval(m, E("add-atom", E("space"), E("my", "test", "atom")))));
    check_answers("the atom is in the space the function named",
                  mt_eval(m, E("match", E("space"), V("a"), V("a"))), E("my", "test", "atom"));
    check("is-space wants the & prefix", !mt_one_truth(mt_eval(m, E("is-space", "my_space_name"))) && mt_ok());
    check("and &self is a space", mt_one_truth(mt_eval(m, E("is-space", "&self"))));
    return done(m);
}
