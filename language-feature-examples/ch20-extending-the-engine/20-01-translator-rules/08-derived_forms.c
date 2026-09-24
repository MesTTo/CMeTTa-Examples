/* Purpose: once, the compiler's own form, swapped for the MeTTa equation
 *   lib_derived writes it as, and back. Before the import, after it and
 *   after the rule is withdrawn, (once ...) over C's array answers exactly
 *   the array's first item, and over (empty) nothing. It is still the first
 *   answer of a generator with side effects: noisy records each item it
 *   produces in &seen, a space C opens, and after once over two noisy items
 *   &seen holds exactly the first one's record, so the rest of the generator
 *   never ran.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    check_int("the compiler's once answers the first item", mt_one_int(mt_eval(m, E("once", superposed()))), items[0]);

    require("import lib_derived", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_derived")))));
    check_int("the equation's once answers the same", mt_one_int(mt_eval(m, E("once", superposed()))), items[0]);
    check_answers("and only it", mt_eval(m, E("once", superposed())), items[0]);
    check_none("over nothing it answers nothing", mt_eval(m, E("once", E("empty"))));

    mt_space *seen = mt_space_open(m, "&seen");
    require("open &seen", seen != NULL);
    require("noisy", mt_add(m, E("=", E("noisy", V("x")),
                                 E("let", V("_"), E("add-atom", mt_spaceref("&seen"), E("saw", V("x"))), V("x")))));
    static const char *const produced[] = { "a", "b" };
    check_answers("once takes the generator's first answer",
                  mt_eval(m, E("once", E("superpose", E(E("noisy", produced[0]), E("noisy", produced[1]))))), produced[0]);
    check_answers("and the rest never ran", mt_atoms(seen), E("saw", produced[0]));
    mt_space_close(seen);

    require("withdraw the rule", mt_one_truth(mt_eval(m, E("remove-translator-rule!", "once"))));
    check_int("the compiler's own once is back", mt_one_int(mt_eval(m, E("once", superposed()))), items[0]);
    return done(m);
}
