/* Purpose: once, the compiler's own form, swapped for the MeTTa equation
 *   lib_derived writes it as, and back. Before the import, after it and
 *   after the rule is withdrawn, (once ...) over C's array answers exactly
 *   the array's first item, and over (empty) nothing. It is still the first
 *   answer of a generator with side effects: noisy records each item it
 *   produces in &seen, a space C opens, and after once over two noisy items
 *   &seen holds exactly the first one's record, so the rest of the generator
 *   never ran.
 * Guarantees: all seven claims of the original hold
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

static const int64_t items[] = { 1, 2, 3 };
#define ITEMS (sizeof items / sizeof *items)

static mt_atom *superposed(void)
{
    mt_atom *list[ITEMS];
    for (size_t i = 0; i < ITEMS; i++) list[i] = N(items[i]);
    return E("superpose", mt_exprv(ITEMS, list));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(mt_one_int(mt_eval(m, E("once", superposed()))) == items[0] && "the compiler's once answers the first item");

    require("import lib_derived", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_derived")))));
    assert(mt_one_int(mt_eval(m, E("once", superposed()))) == items[0] && "the equation's once answers the same");
    assert(answers_are(mt_eval(m, E("once", superposed())), E(items[0])) && "and only it");
    assert(!mt_first(mt_eval(m, E("once", E("empty")))) && mt_ok() && "over nothing it answers nothing");

    mt_space *seen = mt_space_open(m, "&seen");
    require("open &seen", seen != NULL);
    require("noisy", mt_add(m, E("=", E("noisy", V("x")),
                                 E("let", V("_"), E("add-atom", mt_spaceref("&seen"), E("saw", V("x"))), V("x")))));
    static const char *const produced[] = { "a", "b" };
    assert(answers_are(mt_eval(m, E("once", E("superpose", E(E("noisy", produced[0]), E("noisy", produced[1]))))), E(produced[0]))
           && "once takes the generator's first answer");
    assert(answers_are(mt_atoms(seen), E(E("saw", produced[0]))) && "and the rest never ran");
    mt_space_close(seen);

    require("withdraw the rule", mt_one_truth(mt_eval(m, E("remove-translator-rule!", "once"))));
    assert(mt_one_int(mt_eval(m, E("once", superposed()))) == items[0] && "the compiler's own once is back");
    mt_close(m);
    return 0;
}
