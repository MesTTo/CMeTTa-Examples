/* Purpose: the verdicts a program's judges answer the engine with, spelled
 *   here and nowhere else in the corpus. (Accept) lets the offered atom in,
 *   (Accept <atom>) lets <atom> in instead, (Refuse <words>) raises with the
 *   words, and (Drop) writes nothing, which is the write-hook protocol
 *   chapter 15's judges speak. The refusal is also the outcome a typing rule
 *   gives in chapter 9 and a translator rule's decline in chapter 20: the
 *   engine reads the one shape in all three places. The heads are capitalized
 *   because a verdict is data, and a lowercase (drop) is a call wherever a
 *   library defines drop, as lib_functional does [source:
 *   engine/metta/space_hooks.pl, metta_hook_apply/6 and
 *   metta_hook_post_apply/4; engine/type_rules.pl,
 *   valid_typing_rule_outcome/1; engine/translator/lowering.pl line 1250;
 *   docs/journal/2026-09-24-verdicts-are-constructors.md;
 *   commit=5bae989dfe8735448d575856e72f735bb2043e24].
 */
#ifndef EXAMPLES_VERDICTS_H
#define EXAMPLES_VERDICTS_H
#include <cmetta.h>

/* TAKES `as`; NULL accepts the atom as offered. */
static inline mt_atom *accepting(mt_atom *as) { return as ? mt_expr("Accept", as) : mt_expr("Accept"); }
/* TAKES the words. */
static inline mt_atom *refusing(mt_atom *words) { return mt_expr("Refuse", words); }
static inline mt_atom *dropping(void) { return mt_expr("Drop"); }
#endif
