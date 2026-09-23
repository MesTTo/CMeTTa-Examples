/* Purpose: Publish underscores and hyphens as distinct operation names.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
static mt_status constant(mt_call *call, void *user)
{ return mt_answer(call, mt_num(*(int *)user)); }
int main(void)
{
    metta *m = open_engine();
    int underscore = 7, hyphen = 9;
    check("publish underscore", mt_def(m, (mt_op){.name="word_count", .arity=0, .effect=MT_PURE, .fn=constant, .user=&underscore}));
    check("publish hyphen", mt_def(m, (mt_op){.name="word-count", .arity=0, .effect=MT_PURE, .fn=constant, .user=&hyphen}));
    check_answers("names remain distinct", mt_run(m, "!(word_count) !(word-count)"), "7 9");
    check("withdraw underscore", mt_undef(m, "word_count"));
    check("withdraw hyphen", mt_undef(m, "word-count"));
    return done(m, "exact_names");
}

