/* Purpose: a post-add hook, the pre-add hook read against a LANDED atom. The
 *   judges are C functions published as audit and gate, their rules C tables,
 *   and each verdict now has to undo a write that already happened. C keeps a
 *   model of what &pool should hold and derives it from the verdicts as the
 *   pre-add twin does, since an undone write leaves what a refused one does;
 *   after each write the space holds exactly the model, a refused or
 *   uncovered write included, whose Error C builds from the space, the atom
 *   and the rule. With gate claiming the pre slot as well, audit revises only
 *   what gate let in as offered, and releasing both claims, the second
 *   release of the post slot included, leaves a direct write.
 * Guarantees: all twelve claims of the original hold, with its eleven
 *   unasserted forms checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "judges.h"

static const rule audit_rules[] = {
    { .head = "raw", .verdict = TRANSFORM, .into = "cooked" },
    { .head = "secret", .verdict = REFUSE, .words = "no secrets survive the audit" },
    { .head = "dup", .verdict = DROP },
    { .head = "plain", .verdict = ACCEPT },
};
static const rule accept_anything[] = { { .verdict = ACCEPT } };
static const rule gate_rules[] = {
    { .head = "raw", .verdict = ACCEPT },
    { .head = "banned", .verdict = DROP },
    { .head = "plain", .verdict = ACCEPT },
};

int main(void)
{
    metta *m = open_engine();
    const judge audit = JUDGE("audit", audit_rules), other_audit = JUDGE("other-audit", accept_anything),
                gate = JUDGE("gate", gate_rules);
    publish(m, &audit);
    publish(m, &other_audit);
    publish(m, &gate);
    mt_space *pool = mt_space_open(m, "&pool");
    require("open &pool", pool != NULL);
    model held = { 0 };

    check_answers("audit claims the post slot", claim(m, POST_ADD, pool, &audit), mt_unit());
    write_all(pool, &held, NULL, &audit, E("plain", 1), E("raw", 7), E("dup", 3));
    raise_all(m, pool, &held, &audit, POST_ADD, E("secret", 1), E("uncovered", 9));
    check_refused_words(pool, &audit, E("secret", 1));
    check_holds("and the refused write left nothing behind", pool, &held);
    check_conflict(m, POST_ADD, pool, &audit, &other_audit);

    check_answers("gate claims the pre slot beside it", claim(m, PRE_ADD, pool, &gate), mt_unit());
    write_all(pool, &held, &gate, &audit, E("raw", 8), E("banned", 1));

    check_answers("releasing the post slot", release(m, POST_ADD, pool), mt_unit());
    check_answers("and the pre slot", release(m, PRE_ADD, pool), mt_unit());
    write_all(pool, &held, NULL, NULL, E("uncovered", 10));
    check_answers("a second release is no error", release(m, POST_ADD, pool), mt_unit());

    model_free(&held);
    mt_space_close(pool);
    return done(m);
}
