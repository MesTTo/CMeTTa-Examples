/* Purpose: lib_patrick's as-pattern, comprehension and iterate, each beside
 *   the C that computes the same. mirror names a list and destructures it at
 *   once and answers its reverse followed by its tail, which C builds from
 *   the array of names; for keeps the elements past 3, a C filter loop; and
 *   iterate folds 0..9 into 1, a C accumulation.
 * Guarantees: all three claims of the original hold
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

static const char *const NAME[] = { "h", "a", "n", "n", "e", "s" };
enum { LETTERS = sizeof NAME / sizeof *NAME };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    require("mirror", mt_add(m, E("=", E("mirror", V("A")),
                                  E("let", V("A"), E("@", V("L"), E("cons", V("head"), V("tail"))),
                                    E("append", E("reverse", V("L")), V("tail"))))));

    mt_atom *letters[LETTERS], *mirrored[2 * LETTERS - 1];
    for (size_t i = 0; i < LETTERS; i++) letters[i] = mt_sym(NAME[i]);
    for (size_t i = 0; i < LETTERS; i++) mirrored[i] = mt_sym(NAME[LETTERS - 1 - i]);
    for (size_t i = 1; i < LETTERS; i++) mirrored[LETTERS - 1 + i] = mt_sym(NAME[i]);
    assert(answers_are(mt_eval(m, E("mirror", mt_exprv(LETTERS, letters))), E(mt_exprv(2 * LETTERS - 1, mirrored)))
           && "the reverse, then the tail");

    mt_atom *kept[6];
    size_t n = 0;
    for (int64_t x = 1; x <= 6; x++)
        if (x > 3) kept[n++] = N(x);
    assert(list_is(mt_all(mt_eval(m, E("for", V("x"), E(1, 2, 3, 4, 5, 6), E("if", E(">", V("x"), 3), V("x"))))), mt_exprv(n, kept))
           && "for keeps what the if lets through");

    int64_t x = 1;
    for (int64_t i = 0; i < 10; i++) x += i;
    assert(answers_are(mt_eval(m, E("iterate", 0, 10, 1, E("|->", E(V("i"), V("x")), E("+", V("x"), V("i"))))), E(x)) && "iterate folds 0..9");
    mt_close(m);
    return 0;
}
