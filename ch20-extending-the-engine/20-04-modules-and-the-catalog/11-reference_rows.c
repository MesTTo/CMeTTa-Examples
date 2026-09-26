/* Purpose: a live reference to another space's definitions. &reference-home
 *   holds a public module-answer calling an internal module-helper, and
 *   &self its own module-helper; a (from &reference-home) row makes the
 *   home's public heads callable here while their bodies keep calling the
 *   home's helper. HELPER is one body over the C_ and T_ operators, built
 *   into each space with that space's offset and compiled for C, so each
 *   call answers what C computes with the offset of the space whose
 *   definition runs. The home's data and equations stay home, the
 *   declaration and the properties travel, two equal home equations stay two
 *   answers until one is subtracted, and removing the row withdraws what it
 *   brought.
 * Guarantees: all thirteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* Whether a list holds exactly the children of want, in order, each equal
   up to renaming variables; takes both, and shows them when they differ. */
static inline bool list_is(mt_list got, mt_atom *want)
{
    bool holds = mt_ok() && want && got.len == mt_len(want);
    for (size_t i = 0; holds && i < got.len; i++)
        holds = mt_alpha_eq(got.items[i], mt_at(want, i));
    if (!holds) {
        fprintf(stderr, "  got");
        for (size_t i = 0; i < got.len; i++) fprintf(stderr, " %s", mt_show(got.items[i]));
        fprintf(stderr, "\n  want %s\n", want ? mt_show(want) : "nothing");
    }
    mt_list_free(got);
    mt_drop(want);
    return holds;
}

/* Whether a query answers exactly the children of want; takes both. */
static inline bool answers_are(mt_answers *answers, mt_atom *want)
{
    return list_is(mt_all(answers), want);
}

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)

#define HELPER(ADD, x, offset) ADD(x, offset)
enum { HOME_OFFSET = 1, SELF_OFFSET = 100, LATER = 9 };

static mt_atom *arrow(void) { return E("->", "Number", "Number"); }

/* What (get-property name) says about one property, read through case. */
static mt_answers *property(metta *m, const char *name, mt_atom *pattern, mt_atom *answer)
{
    return mt_eval(m, E("case", E("get-property", name), E(E(pattern, answer))));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *home = mt_space_open(m, "&reference-home");
    require("open &reference-home", home != NULL);
    require("the helper is internal", mt_add(home, E("internal", "module-helper")));
    require("its type", mt_add(home, E(":", "module-helper", arrow())));
    require("the home's helper", mt_add(home, E("=", E("module-helper", V("x")), HELPER(T_ADD, V("x"), HOME_OFFSET))));
    require("the answer's type", mt_add(home, E(":", "module-answer", arrow())));
    require("the answer calls the helper", mt_add(home, E("=", E("module-answer", V("x")), E("module-helper", V("x")))));
    require("its doc", mt_add(home, E("@doc", "module-answer", E("@desc", T("Add one at home")))));
    require("home data", mt_add(home, E("home-data", "kept")));

    require("this space's helper is internal too", mt_add(m, E("internal", "module-helper")));
    require("its type", mt_add(m, E(":", "module-helper", arrow())));
    require("this space's helper", mt_add(m, E("=", E("module-helper", V("x")), HELPER(T_ADD, V("x"), SELF_OFFSET))));
    mt_atom *reference = E("from", mt_spaceref("&reference-home"));
    require("the reference row", mt_add(m, mt_keep(reference)));

    const int64_t x = 2;
    assert(mt_one_int(mt_eval(m, E("module-answer", x))) == HELPER(C_ADD, x, HOME_OFFSET)
           && "the home's answer calls the home's helper");
    assert(mt_one_int(mt_eval(m, E("module-helper", x))) == HELPER(C_ADD, x, SELF_OFFSET) && "this space's helper is its own");
    assert(!mt_first(mt_match(m, E("home-data", V("x")))) && mt_ok() && "the home's data stays home");
    assert(!mt_first(mt_match(m, E("=", E("module-answer", V("x")), V("body")))) && mt_ok() && "and so do its equations");
    assert(answers_are(mt_match(m, E(":", "module-answer", V("type"))), E(E(":", "module-answer", arrow()))) && "the declaration travels");

    assert(answers_are(property(m, "module-answer", E("visibility", V("v")), V("v")), E("public")) && "module-answer is public");
    assert(answers_are(property(m, "module-helper", E("visibility", V("v")), V("v")), E("internal")) && "module-helper is internal");
    assert(answers_are(property(m, "module-answer", E("origin", V("home"), V("file"), V("line")), E(V("home"), V("line"))), E(E(mt_spaceref("&reference-home"), -1)))
           && "the origin is the home, with no equation line");
    assert(mt_one_int(mt_eval(m, E("evalc", E("module-helper", x), mt_spaceref("&reference-home")))) == HELPER(C_ADD, x, HOME_OFFSET)
           && "evalc calls the helper in its home");

    mt_atom *later = E("=", E("module-later"), LATER);
    for (int i = 0; i < 2; i++) require("module-later at home", mt_add(home, mt_keep(later)));
    assert(answers_are(mt_eval(m, E("module-later")), E(LATER, LATER)) && "two equal equations are two answers");
    require("subtract one", mt_one_truth(mt_eval(m, E("subtract-atom", mt_spaceref("&reference-home"), mt_keep(later)))));
    assert(answers_are(mt_eval(m, E("module-later")), E(LATER)) && "one is left");
    mt_drop(later);

    require("remove the reference", mt_del(m, reference));
    assert(answers_are(mt_eval(m, E("module-answer", x)), E(E("module-answer", x))) && "its definitions went with it");
    assert(!mt_first(mt_match(m, E(":", "module-answer", V("type")))) && mt_ok() && "and its copied declaration");
    mt_space_close(home);
    mt_close(m);
    return 0;
}
