/* Purpose: the verdicts of the engine's write-hook protocol, spelled here and
 *   nowhere else in chapter 15's twins: (accept) lets the offered atom in,
 *   (accept <atom>) lets <atom> in instead, (refuse <words>) raises with the
 *   words, and (drop) writes nothing [source: engine/metta/space_hooks.pl,
 *   metta_hook_apply/6 and metta_hook_post_apply/4;
 *   commit=11bb3d9780d9714f2284eaae7d218b12476122ae].
 */
#ifndef CH15_VERDICTS_H
#define CH15_VERDICTS_H
#include "common.h"

/* TAKES `as`; NULL accepts the atom as offered. */
static inline mt_atom *accepting(mt_atom *as) { return as ? E("accept", as) : E("accept"); }
/* TAKES the words. */
static inline mt_atom *refusing(mt_atom *words) { return E("refuse", words); }
static inline mt_atom *dropping(void) { return E("drop"); }
#endif
