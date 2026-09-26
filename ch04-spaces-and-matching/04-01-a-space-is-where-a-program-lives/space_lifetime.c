/* Purpose: a C handle is not the space. Closing the handle leaves the named
 *   space and its facts where they were; mt_space_drop() ends the engine
 *   space itself, so the name opens fresh and empty afterwards.
 * Guarantees: facts survive a closed handle and not a drop
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *space = mt_space_open(m, "&durable-name");
    require("open", space != NULL);
    require("store a fact", mt_add(space, E("item", 42)));
    mt_space_close(space);

    space = mt_space_open(m, "&durable-name");
    require("reopen", space != NULL);
    assert(answers_are(mt_atoms(space), E(E("item", 42))) && "closing the handle kept the fact");
    require("drop the engine space", mt_space_drop(space));
    mt_space_close(space);

    space = mt_space_open(m, "&durable-name");
    require("open the name again", space != NULL);
    assert((int64_t)mt_count(space) == 0 && "dropping it removed the facts");
    require("drop the empty space", mt_space_drop(space));
    mt_space_close(space);
    mt_close(m);
    return 0;
}
