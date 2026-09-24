/* Purpose: a foreign space that holds rules as well as facts. The store is
 *   chapter 19's C array, c_store.h, behind cmetta's provider door with the
 *   rules promise, where the original's store is thirteen lines of Prolog
 *   declaring the same capability. The engine compiles each equation added
 *   there, so a rule in the C store is the same compiled clause a native one
 *   is, and it answers in the space that holds it, asked through (metta goal
 *   type space). Each rule's body is written once over lowering.h's
 *   operators, lowered into the store and compiled for C, and each answer is
 *   what C computes: fdouble's double, fpick's answers as a set, sorted by
 *   mt_order against C's own, fplain's bare symbol, fnest's arithmetic
 *   evaluated inside out, and ffact's factorial with if taking only its
 *   branch. The space is still a data source, and the one edge C stored is
 *   the one match finds.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "../../ch19-spaces-backed-by-anything/19-02-a-space-in-c/c_store.h"

#define FDOUBLE(MUL, x) MUL(2, x)
#define FNEST(ADD, MUL) ADD(1, MUL(2, 3))
#define FFACT(IF, GT, MUL, SUB, SELF, x) IF(GT(x, 0), MUL(x, SELF(SUB(x, 1))), 1)
#define M_FFACT(x) (ffact x)

static int64_t ffact(int64_t x) { return FFACT(C_IF, C_GT, C_MUL, C_SUB, ffact, x); }

static c_store rules = C_STORE_INIT;
static const char *const picks[] = { "one", "two" };
#define PICKS (sizeof picks / sizeof *picks)

/* (metta goal %Undefined% &rule_demo): evaluation in the space that holds
   the rule. TAKES goal. */
static mt_answers *in_space(metta *m, mt_atom *goal)
{
    return mt_eval(m, E("metta", goal, "%Undefined%", mt_spaceref("&rule_demo")));
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_import", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_import")))));
    require("the store holds rules", mt_provider_open(m, "&rule_demo", c_store_provider(&rules, true)));
    mt_space *demo = mt_space_open(m, "&rule_demo");
    require("a handle on &rule_demo", demo != NULL);

    require("fdouble", mt_lower(demo, (fdouble $x), FDOUBLE(M_MUL, $x)));
    const int64_t doubled = 21;
    check_int("a rule in the foreign space evaluates", mt_one_int(in_space(m, E("fdouble", doubled))),
              FDOUBLE(C_MUL, doubled));

    for (size_t i = 0; i < PICKS; i++) require("an fpick", mt_add(demo, E("=", E("fpick"), picks[i])));
    mt_list got = mt_all(in_space(m, E("fpick")));
    qsort(got.items, got.len, sizeof *got.items, mt_order);
    mt_atom *want[PICKS];
    for (size_t i = 0; i < PICKS; i++) want[i] = S(picks[i]);
    qsort(want, PICKS, sizeof *want, mt_order);
    check_list_("several equations are an answer set", got, PICKS, want);

    require("fplain", mt_add(demo, E("=", E("fplain"), "settled")));
    check_answers("a body that is not a call is the answer", in_space(m, E("fplain")), "settled");

    require("fnest", mt_lower(demo, (fnest), FNEST(M_ADD, M_MUL)));
    check_int("a nested body is evaluated inside out", mt_one_int(in_space(m, E("fnest"))), FNEST(C_ADD, C_MUL));

    require("ffact", mt_lower(demo, (ffact $x), FFACT(M_IF, M_GT, M_MUL, M_SUB, M_FFACT, $x)));
    const int64_t n = 5;
    check_int("a rule recurses, if taking only its branch", mt_one_int(in_space(m, E("ffact", n))), ffact(n));

    require("a fact beside the rules", mt_add(demo, E("edge", "a", "b")));
    check_answers("and the space is still a data source", mt_match(demo, E("edge", V("x"), V("y"))), E("edge", "a", "b"));

    mt_space_close(demo);
    require("withdraw the store", mt_provider_close(m, "&rule_demo"));
    return done(m);
}
