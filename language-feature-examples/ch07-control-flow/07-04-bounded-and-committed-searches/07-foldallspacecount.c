/* Purpose: counting by folding ones. countitem answers 1 per foo fact, and
 *   folding those ones with merge, a C function, counts the facts; C counts
 *   them itself by walking the match cursor.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status merge(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(mt_int(mt_arg(call, 0)) + mt_int(mt_arg(call, 1))));
}

int main(void)
{
    metta *m = open_engine();
    for (int64_t n = 1; n <= 3; n++) require("(foo n)", mt_add(m, E("foo", n)));
    require("countitem", mt_lower(m, (countitem), (let $x (match &self (foo $1) (foo $1)) 1)));
    require("publish merge", mt_def(m, (mt_op){ .name = "merge", .arity = 2, .effect = MT_PURE, .fn = merge }));
    require("spacecount", mt_lower(m, (spacecount $x), (foldall merge (countitem) 0)));

    int64_t facts = 0;
    mt_each (fact, mt_match(m, E("foo", V("n")))) {
        (void)fact;
        facts++;
    }
    check_answers("folding ones counts the facts", mt_eval(m, E("foldall", "merge", E("countitem"), 0)), facts);
    return done(m);
}
