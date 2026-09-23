/* Purpose: a C handle is not the space. Closing the handle leaves the named
 *   space and its facts where they were; mt_space_drop() ends the engine
 *   space itself, so the name opens fresh and empty afterwards.
 * Guarantees: facts survive a closed handle and not a drop [tested: make
 *   check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    mt_space *space = mt_space_open(m, "&durable-name");
    require("open", space != NULL);
    require("store a fact", mt_add(space, E("item", 42)));
    mt_space_close(space);

    space = mt_space_open(m, "&durable-name");
    require("reopen", space != NULL);
    check_answers("closing the handle kept the fact", mt_atoms(space), E("item", 42));
    require("drop the engine space", mt_space_drop(space));
    mt_space_close(space);

    space = mt_space_open(m, "&durable-name");
    require("open the name again", space != NULL);
    check_int("dropping it removed the facts", (int64_t)mt_count(space), 0);
    require("drop the empty space", mt_space_drop(space));
    mt_space_close(space);
    return done(m);
}
