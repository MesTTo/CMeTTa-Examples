/* Purpose: a PLN query answered by lib_pln's bounded search, over a
 *   knowledge base whose nodes carry truths of their own, held to the proof
 *   the answer's stamp names. The node truths and the four sentences are C's
 *   tables, added as the STV equations and the kb equation they are. The
 *   stamp (1 2 3 4) is both ways from A to D: each two-step chain through a
 *   middle node is a deduction from the three node truths and the two
 *   links, and the answer revises the chains together, all with pln.h. Its
 *   deduction is the one 03 defines for itself, which lib_pln's is wherever
 *   a strength is a number: lib_pln divides through /safe, whose divisor
 *   1 - Qs the formula reaches only when Qs is at most 0.9999, so it is
 *   positive whenever it is divided by. The search needs a deeper stack than
 *   the evaluator's default, which the original states for itself.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "nars_truth.h"
#include "pln.h"
#include "derive.h"

static const struct {
    const char *node;
    truth tv;
} nodes[] = { { "A", { 0.5, 0.9 } }, { "B", { 0.25, 0.9 } }, { "C", { 0.25, 0.9 } }, { "D", { 0.5, 0.9 } } };

static const struct {
    int64_t id;
    const char *from, *to;
    truth tv;
} kb[] = { { 1, "A", "B", { 0.25, 0.9 } }, { 2, "A", "C", { 0.25, 0.9 } }, { 3, "B", "D", { 0.5, 0.9 } }, { 4, "C", "D", { 0.5, 0.9 } } };
#define KB (sizeof kb / sizeof *kb)

static truth belief(const char *node)
{
    for (size_t i = 0; i < sizeof nodes / sizeof *nodes; i++)
        if (strcmp(nodes[i].node, node) == 0) return nodes[i].tv;
    require("a node C believes something of", false);
    return (truth){ 0, 0 };
}

/* ((stv f c) (id ...)): every chain from FROM to TO through one middle
   node deduced, the chains revised together in table order, each step's
   stamp its premises' merged as derive.h's loop merges them. */
static mt_atom *answer(const char *from, const char *to)
{
    mt_atom *stamp = NULL;
    truth revised = { 0, 0 };
    for (size_t i = 0; i < KB; i++)
        for (size_t j = 0; j < KB; j++) {
            if (strcmp(kb[i].from, from) != 0 || strcmp(kb[i].to, kb[j].from) != 0 || strcmp(kb[j].to, to) != 0) continue;
            mt_atom *deduced = deduction_answer(belief(from), belief(kb[i].to), belief(to), kb[i].tv, kb[j].tv),
                    *first = E(kb[i].id), *second = E(kb[j].id), *chain = stamps_merged(first, second);
            truth deduction = stv_truth(deduced);
            mt_drop(deduced), mt_drop(first), mt_drop(second);
            if (stamp) {
                require("the chains revise", pln_revision(revised, deduction, &revised));
                mt_atom *both = stamps_merged(stamp, chain);
                mt_drop(stamp), mt_drop(chain);
                stamp = both;
            } else {
                revised = deduction, stamp = chain;
            }
        }
    require("a chain from A to D", stamp != NULL);
    return E(stv(revised), stamp);
}

int main(void)
{
    metta *m = open_engine();
    require("lib_pln", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln")))));
    for (size_t i = 0; i < sizeof nodes / sizeof *nodes; i++)
        require("a node's truth", mt_add(m, E("=", E("STV", nodes[i].node), stv(nodes[i].tv))));
    mt_atom *sentences[KB];
    for (size_t i = 0; i < KB; i++) sentences[i] = E("Sentence", E(E("Inheritance", kb[i].from, kb[i].to), stv(kb[i].tv)), E(kb[i].id));
    require("the knowledge base", mt_add(m, E("=", E("kb"), mt_exprv(KB, sentences))));

    check_answers("A to D, both ways and revised",
                  mt_eval(m, E("with-pragma!", E(E("max-stack-depth", 100000000)), E("PLN.Query", E("kb"), E("Inheritance", "A", "D")))),
                  answer("A", "D"));
    return done(m);
}
