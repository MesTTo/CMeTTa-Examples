/* Purpose: the same deduction asked of a relation rather than a rule. The
 *   consistency conditions and the deduction are pln.h's bodies, added as
 *   the equations they are, and what each node is believed to be is C's
 *   table. The sentences are a second table, each row an equation that
 *   answers True once, and the derived sentence is the recursive equation
 *   that chains two of them. C finds the chain the relation finds, the first
 *   sentence out of the source and the first from there to the target in
 *   the table's order, and computes its truth with pln.h's deduction.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "nars_truth.h"
#include "pln.h"

static const struct {
    const char *node;
    truth tv;
} nodes[] = { { "a", { 0.4, 0.9 } }, { "b", { 0.4, 0.9 } }, { "c", { 0.4, 0.9 } } };

static const struct {
    const char *from, *to;
    truth tv;
} sentences[] = { { "a", "b", { 0.9, 0.9 } }, { "b", "c", { 0.9, 0.9 } } };
enum { SENTENCES = sizeof sentences / sizeof *sentences };

static truth belief(const char *node)
{
    for (size_t i = 0; i < sizeof nodes / sizeof *nodes; i++)
        if (strcmp(nodes[i].node, node) == 0) return nodes[i].tv;
    require("a node C believes something of", false);
    return (truth){ 0, 0 };
}

/* The truth of the first two-step chain from FROM to TO, as once finds it. */
static mt_atom *derived(const char *from, const char *to)
{
    for (size_t i = 0; i < SENTENCES; i++) {
        if (strcmp(sentences[i].from, from) != 0) continue;
        for (size_t j = 0; j < SENTENCES; j++)
            if (strcmp(sentences[j].from, sentences[i].to) == 0 && strcmp(sentences[j].to, to) == 0)
                return deduction_answer(belief(from), belief(sentences[i].to), belief(to), sentences[i].tv, sentences[j].tv);
    }
    require("a chain C can find", false);
    return NULL;
}

int main(void)
{
    metta *m = open_engine();
    install_deduction(m);
    for (size_t i = 0; i < sizeof nodes / sizeof *nodes; i++)
        require("a node's truth", mt_add(m, E("=", E("STV", nodes[i].node), stv(nodes[i].tv))));
    for (size_t i = 0; i < SENTENCES; i++)
        require("a sentence", mt_add(m, E("=", E("sentence", E("Inheritance", sentences[i].from, sentences[i].to), stv(sentences[i].tv)),
                                          E("once", B(true)))));
    mt_atom *a = V("A"), *b = V("B"), *c = V("C");
    require("the derived sentence",
            mt_add(m, E("=", E("sentence", E("Inheritance", mt_keep(a), mt_keep(c)), V("TV")),
                        E("once", E("and", E("and", E("sentence", E("Inheritance", mt_keep(a), mt_keep(b)), V("T1")),
                                             E("sentence", E("Inheritance", mt_keep(b), mt_keep(c)), V("T2"))),
                                    E("=", V("TV"),
                                      E("Truth_Deduction", E("STV", mt_keep(a)), E("STV", mt_keep(b)), E("STV", mt_keep(c)), V("T1"),
                                        V("T2"))))))));
    mt_drop(a), mt_drop(b), mt_drop(c);

    check_answers("the derived truth of a to c",
                  mt_eval(m, E("let", V("derivation"), E("sentence", E("Inheritance", "a", "c"), V("TV")), V("TV"))), derived("a", "c"));
    return done(m);
}
