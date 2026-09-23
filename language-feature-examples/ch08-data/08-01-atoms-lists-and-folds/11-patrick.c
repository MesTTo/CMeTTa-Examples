/* Purpose: lib_patrick's as-pattern, comprehension and iterate, each beside
 *   the C that computes the same. mirror names a list and destructures it at
 *   once and answers its reverse followed by its tail, which C builds from
 *   the array of names; for keeps the elements past 3, a C filter loop; and
 *   iterate folds 0..9 into 1, a C accumulation.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static const char *const NAME[] = { "h", "a", "n", "n", "e", "s" };
enum { LETTERS = sizeof NAME / sizeof *NAME };

int main(void)
{
    metta *m = open_engine();
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    require("mirror", mt_add(m, E("=", E("mirror", V("A")),
                                  E("let", V("A"), E("@", V("L"), E("cons", V("head"), V("tail"))),
                                    E("append", E("reverse", V("L")), V("tail"))))));

    mt_atom *letters[LETTERS], *mirrored[2 * LETTERS - 1];
    for (size_t i = 0; i < LETTERS; i++) letters[i] = mt_sym(NAME[i]);
    for (size_t i = 0; i < LETTERS; i++) mirrored[i] = mt_sym(NAME[LETTERS - 1 - i]);
    for (size_t i = 1; i < LETTERS; i++) mirrored[LETTERS - 1 + i] = mt_sym(NAME[i]);
    check_answers("the reverse, then the tail", mt_eval(m, E("mirror", mt_exprv(LETTERS, letters))),
                  mt_exprv(2 * LETTERS - 1, mirrored));

    mt_atom *kept[6];
    size_t n = 0;
    for (int64_t x = 1; x <= 6; x++)
        if (x > 3) kept[n++] = N(x);
    check_list_("for keeps what the if lets through",
                mt_all(mt_eval(m, E("for", V("x"), E(1, 2, 3, 4, 5, 6), E("if", E(">", V("x"), 3), V("x"))))), n, kept);

    int64_t x = 1;
    for (int64_t i = 0; i < 10; i++) x += i;
    check_answers("iterate folds 0..9", mt_eval(m, E("iterate", 0, 10, 1, E("|->", E(V("i"), V("x")), E("+", V("x"), V("i"))))), x);
    return done(m);
}
