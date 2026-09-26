/* Purpose: the language's Booleans cross into Python and back. C holds each
 *   answer against its own: Python spells a Boolean True or False, sorting
 *   puts false before true as C's integers do, a list's length is its count,
 *   a Boolean is an instance of a Boolean's type, a number's truth is
 *   whether it is nonzero, true's bit length is one's, and only the Boolean
 *   atoms convert, so abc is upper-cased as text.
 * Guarantees: all nine claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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

static const char *python_spelling(bool b) { return b ? "True" : "False"; }

static int by_truth(const void *a, const void *b) { return (int)*(const bool *)a - (int)*(const bool *)b; }

/* The number of bits a nonnegative integer needs, as int.bit_length counts. */
static int64_t bit_length(uint64_t x)
{
    int64_t n = 0;
    for (; x; x >>= 1) n++;
    return n;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (int b = 1; b >= 0; b--) assert(answers_are(mt_eval(m, E("repr", E("py-call", E("str", B(b))))), E(T(python_spelling(b)))) && "str of a Boolean");
    bool unsorted[] = { true, false };
    qsort(unsorted, 2, sizeof *unsorted, by_truth);
    assert(answers_are(mt_eval(m, E("py-call", E("sorted", E(B(true), B(false))))), E(E(B(unsorted[0]), B(unsorted[1])))) && "sorted, false first");
    const bool three[] = { true, false, true };
    assert(answers_are(mt_eval(m, E("py-call", E("len", E(B(three[0]), B(three[1]), B(three[2]))))), E(N(sizeof three / sizeof *three))) && "a list's length");
    assert(answers_are(mt_eval(m, E("py-call", E("isinstance", B(true), E("py-call", E("type", B(false)))))), E(B(true))) && "a Boolean's type");
    for (int64_t x = 1; x >= 0; x--) assert(answers_are(mt_eval(m, E("py-call", E("bool", x))), E(B(x != 0))) && "a number's truth");
    assert(answers_are(mt_eval(m, E("py-call", E(".bit_length", B(true)))), E(N(bit_length(true)))) && "a method on a Boolean");
    const char word[] = "abc";
    char upper[sizeof word];
    for (size_t i = 0; i < sizeof word; i++) upper[i] = (char)toupper((unsigned char)word[i]);
    assert(answers_are(mt_eval(m, E("repr", E("py-call", E(".upper", word)))), E(T(upper))) && "other symbols stay text");
    mt_close(m);
    return 0;
}
