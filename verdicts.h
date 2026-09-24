/* Purpose: the verdicts a program's judges answer the engine with, spelled
 *   here and nowhere else in the corpus. (accept) lets the offered atom in,
 *   (accept <atom>) lets <atom> in instead, (refuse <words>) raises with the
 *   words, and (drop) writes nothing, which is the write-hook protocol
 *   chapter 15's judges speak. The refusal is also the outcome a typing rule
 *   gives in chapter 9 and a translator rule's decline in chapter 20: the
 *   engine reads the one shape in all three places [source:
 *   engine/metta/space_hooks.pl, metta_hook_apply/6 and
 *   metta_hook_post_apply/4; engine/type_rules.pl,
 *   valid_typing_rule_outcome/1; engine/translator/lowering.pl line 1250;
 *   commit=214188f1d5b5018a0061ea1bc72b104e69137b8f].
 */
#ifndef EXAMPLES_VERDICTS_H
#define EXAMPLES_VERDICTS_H
#include <cmetta.h>

/* TAKES `as`; NULL accepts the atom as offered. */
static inline mt_atom *accepting(mt_atom *as) { return as ? mt_expr("accept", as) : mt_expr("accept"); }
/* TAKES the words. */
static inline mt_atom *refusing(mt_atom *words) { return mt_expr("refuse", words); }
static inline mt_atom *dropping(void) { return mt_expr("drop"); }
#endif
