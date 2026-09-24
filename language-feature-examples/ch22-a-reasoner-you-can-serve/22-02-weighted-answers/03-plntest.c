/* Purpose: one PLN deduction step, in a program that defines its own
 *   formula. The consistency conditions and the deduction are pln.h's
 *   bodies, added as the equations they are; what each node is believed to
 *   be is C's table, each row a (= (STV node) (stv s c)) equation, and the
 *   link types a deduction may chain are a second table, each row a guard.
 *   The rule is the term it is. C computes what the rule must answer from
 *   its own tables: the premises chain where the first's target is the
 *   second's source, their link type is one C lists, and the conclusion
 *   carries the truth pln.h's deduction gives from the three node truths and
 *   the two premises.
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

static const char *const syllogistic[] = { "Inheritance", "Implication" };

typedef struct link {
    const char *type, *from, *to;
    truth tv;
} link;

static const link premises[] = { { "Inheritance", "a", "b", { 0.9, 0.9 } }, { "Inheritance", "b", "c", { 0.8, 0.9 } } };

static truth belief(const char *node)
{
    for (size_t i = 0; i < sizeof nodes / sizeof *nodes; i++)
        if (strcmp(nodes[i].node, node) == 0) return nodes[i].tv;
    require("a node C believes something of", false);
    return (truth){ 0, 0 };
}

static bool chains(const char *type)
{
    for (size_t i = 0; i < sizeof syllogistic / sizeof *syllogistic; i++)
        if (strcmp(syllogistic[i], type) == 0) return true;
    return false;
}

/* ((type from to) (stv s c)) */
static mt_atom *premise(const link *l) { return E(E(l->type, l->from, l->to), stv(l->tv)); }

/* The conclusion the rule must answer for two premises, or NULL where they
   do not chain. */
static mt_atom *deduced(const link *first, const link *second)
{
    if (strcmp(first->type, second->type) != 0 || strcmp(first->to, second->from) != 0 || !chains(first->type)) return NULL;
    return E(E(first->type, first->from, second->to), deduction_answer(belief(first->from), belief(first->to), belief(second->to),
                                                                        first->tv, second->tv));
}

int main(void)
{
    metta *m = open_engine();
    install_deduction(m);
    for (size_t i = 0; i < sizeof syllogistic / sizeof *syllogistic; i++)
        require("a guard", mt_add(m, E("=", E("SyllogisticRuleGuard", syllogistic[i]), B(true))));
    for (size_t i = 0; i < sizeof nodes / sizeof *nodes; i++)
        require("a node's truth", mt_add(m, E("=", E("STV", nodes[i].node), stv(nodes[i].tv))));
    mt_atom *type = V("LinkType"), *a = V("A"), *b = V("B"), *c = V("C");
    require("the deduction rule",
            mt_add(m, E("=", E("|-", E(E(mt_keep(type), mt_keep(a), mt_keep(b)), V("T1")), E(E(mt_keep(type), mt_keep(b), mt_keep(c)), V("T2"))),
                        T_IF(E("SyllogisticRuleGuard", mt_keep(type)),
                             E(E(mt_keep(type), mt_keep(a), mt_keep(c)),
                               E("Truth_Deduction", E("STV", mt_keep(a)), E("STV", mt_keep(b)), E("STV", mt_keep(c)), V("T1"), V("T2"))),
                             E("empty")))));
    mt_drop(type), mt_drop(a), mt_drop(b), mt_drop(c);

    mt_atom *conclusion = deduced(&premises[0], &premises[1]);
    require("the premises chain", conclusion != NULL);
    check_answers("deduction through b", mt_eval(m, E("|-", premise(&premises[0]), premise(&premises[1]))), conclusion);
    return done(m);
}
