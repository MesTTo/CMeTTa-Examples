/* Purpose: a space with a model, which C names through the language's own
 *   door: (new-space &name model) built as a term, the engine answering the
 *   space it made, and a handle opened on it.
 * Assumes: the includer defines MT_SHORTHAND and includes common.h first.
 * Guarantees: the handle names the space the engine made, or the program
 *   ends saying the engine refused it [tested: make twins; commit=WORKTREE].
 */
#ifndef CH19_SPACES_H
#define CH19_SPACES_H

/* TAKES model; the handle is the caller's to close. */
static inline mt_space *new_space(metta *m, const char *name, mt_atom *model)
{
    mt_atom *made = mt_first(mt_eval(m, E("new-space", mt_spaceref(name), model)));
    require("the engine makes the space", made && strcmp(mt_name(made), name) == 0);
    mt_drop(made);
    mt_space *space = mt_space_open(m, name);
    require("a handle on it", space != NULL);
    return space;
}
#endif
