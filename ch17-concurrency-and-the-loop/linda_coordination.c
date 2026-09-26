/* Purpose: Linda over a space. A C subscription watches &mailbox, a job is
 *   published, peek-atom reads it without taking it and take-atom consumes
 *   it once, the three tuple-space operations lib_thread provides.
 * Guarantees: the watch sees the one commit, peek leaves the tuple, take
 *   removes it [tested 2026-09-27T00:35:58+10:00:
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

/* The same question as a truth value, for a condition with more in it:
   borrows the atom the program holds and takes the expectation. */
static inline bool alpha_equal(const mt_atom *got, mt_atom *want)
{
    bool equal = got && want && mt_alpha_eq(got, want);
    mt_drop(want);
    return equal;
}

static mt_status published(void *user, bool added, const mt_atom *job)
{
    if (added && alpha_equal(job, E("Job", 7))) ++*(unsigned *)user;
    return MT_OK;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    unsigned seen = 0;
    require("import lib_thread",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_thread")))));
    mt_space *mailbox = mt_space_open(m, "&mailbox");
    require("open &mailbox", mailbox != NULL);
    require("watch for jobs", mt_subscribe(m, "jobs", (mt_subscription){
        .space = "&mailbox", .pattern = E("Job", V("id")), .notify = published, .user = &seen }));

    require("publish a job", mt_add(mailbox, E("Job", 7)));
    assert(seen == 1 && "the watch saw the commit");

    assert(answers_are(mt_eval(m, E("peek-atom", mt_spaceref("&mailbox"), E("Job", V("id")), 1.0)), E(E("Job", 7)))
           && "peek reads the job");
    assert((int64_t)mt_count(mailbox) == 1 && "and leaves it there");
    assert(answers_are(mt_eval(m, E("take-atom", mt_spaceref("&mailbox"), E("Job", V("id")), 1.0)), E(E("Job", 7)))
           && "take consumes the job");
    assert((int64_t)mt_count(mailbox) == 0 && "and the mailbox is empty");

    require("stop watching", mt_unsubscribe(m, "jobs"));
    require("drop the mailbox", mt_space_drop(mailbox));
    mt_space_close(mailbox);
    mt_close(m);
    return 0;
}
