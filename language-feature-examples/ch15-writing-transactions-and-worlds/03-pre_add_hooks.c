/* Purpose: a pre-add hook, arbitrary code at a space's write door. The judge
 *   is a C function published as guard, its four rules a C table, and it
 *   claims &pool's write door through the engine's own declare-pre-add!. C
 *   keeps a model of what &pool should hold, the offered atom for an accept,
 *   the transformed one for a transform and nothing for a drop, and after
 *   each write the space holds exactly the model. A refusal and an atom no
 *   rule covers each raise the Error C builds from the space, the atom and
 *   the rule; through mt_add the refusal reaches the C caller as a failed
 *   write carrying the rule's own words. A second claimant is refused with
 *   both judges named, and releasing the claim makes the door direct again.
 * Guarantees: all seven claims of the original hold, with its six
 *   unasserted forms checked as well [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "judges.h"

static const rule guard_rules[] = {
    { .head = "secret", .verdict = REFUSE, .words = "no secrets in this pool" },
    { .head = "raw", .verdict = TRANSFORM, .into = "cooked" },
    { .head = "dup", .verdict = DROP },
    { .head = "plain", .verdict = ACCEPT },
};
static const rule accept_anything[] = { { .verdict = ACCEPT } };

int main(void)
{
    metta *m = open_engine();
    const judge guard = JUDGE("guard", guard_rules), other_guard = JUDGE("other-guard", accept_anything);
    publish(m, &guard);
    publish(m, &other_guard);
    mt_space *pool = mt_space_open(m, "&pool");
    require("open &pool", pool != NULL);
    model held = { 0 };

    check_answers("guard claims the write door", claim(m, PRE_ADD, pool, &guard), mt_unit());
    write_all(pool, &held, &guard, NULL, E("plain", 1), E("raw", 7), E("dup", 3));
    raise_all(m, pool, &held, &guard, PRE_ADD, E("secret", 1), E("uncovered", 9));
    check_refused_words(pool, &guard, E("secret", 1));
    check_holds("and nothing refused landed", pool, &held);
    check_conflict(m, PRE_ADD, pool, &guard, &other_guard);
    check_answers("releasing the claim", release(m, PRE_ADD, pool), mt_unit());
    write_all(pool, &held, NULL, NULL, E("uncovered", 10));

    model_free(&held);
    mt_space_close(pool);
    return done(m);
}
