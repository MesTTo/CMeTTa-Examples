/* Purpose: Linda over a space. A C subscription watches &mailbox, a job is
 *   published, peek-atom reads it without taking it and take-atom consumes
 *   it once, the three tuple-space operations lib_thread provides.
 * Guarantees: the watch sees the one commit, peek leaves the tuple, take
 *   removes it [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status published(void *user, bool added, const mt_atom *job)
{
    if (added && alpha_equal(job, E("Job", 7))) ++*(unsigned *)user;
    return MT_OK;
}

int main(void)
{
    metta *m = open_engine();
    unsigned seen = 0;
    require("import lib_thread",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_thread")))));
    mt_space *mailbox = mt_space_open(m, "&mailbox");
    require("open &mailbox", mailbox != NULL);
    require("watch for jobs", mt_subscribe(m, "jobs", (mt_subscription){
        .space = "&mailbox", .pattern = E("Job", V("id")), .notify = published, .user = &seen }));

    require("publish a job", mt_add(mailbox, E("Job", 7)));
    check_int("the watch saw the commit", seen, 1);

    check_answers("peek reads the job",
                  mt_eval(m, E("peek-atom", mt_spaceref("&mailbox"), E("Job", V("id")), 1.0)),
                  E("Job", 7));
    check_int("and leaves it there", (int64_t)mt_count(mailbox), 1);
    check_answers("take consumes the job",
                  mt_eval(m, E("take-atom", mt_spaceref("&mailbox"), E("Job", V("id")), 1.0)),
                  E("Job", 7));
    check_int("and the mailbox is empty", (int64_t)mt_count(mailbox), 0);

    require("stop watching", mt_unsubscribe(m, "jobs"));
    require("drop the mailbox", mt_space_drop(mailbox));
    mt_space_close(mailbox);
    return done(m);
}
