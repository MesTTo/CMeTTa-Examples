/* Purpose: the C spelling of a sequence variable, shared by this section's
 *   twins: seg("x") is the named gap (:seg $x), GAP() the anonymous `...`,
 *   and a run is a view over a slice of an expression's children, which is
 *   what a gap answers.
 * Assumes: the includer defines MT_SHORTHAND before its first include.
 */
#ifndef SEGMENTS_H
#define SEGMENTS_H
#include <cmetta.h>

static inline mt_atom *seg(const char *name) { return E(":seg", V(name)); }
static inline mt_atom *GAP(void) { return S("..."); }

static inline void drop_parent(void *parent) { mt_drop(parent); }

/* Children [from, to) of `e` as an expression, sharing e's own children. */
static inline mt_atom *run(const mt_atom *e, size_t from, size_t to)
{
    return mt_expr_ref(to - from, mt_children(e) + from, mt_keep(e), drop_parent);
}
#endif
