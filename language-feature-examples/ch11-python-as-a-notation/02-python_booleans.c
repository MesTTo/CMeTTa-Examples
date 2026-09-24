/* Purpose: the language's Booleans cross into Python and back. C holds each
 *   answer against its own: Python spells a Boolean True or False, sorting
 *   puts false before true as C's integers do, a list's length is its count,
 *   a Boolean is an instance of a Boolean's type, a number's truth is
 *   whether it is nonzero, true's bit length is one's, and only the Boolean
 *   atoms convert, so abc is upper-cased as text.
 * Guarantees: all nine claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>

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
    metta *m = open_engine();
    for (int b = 1; b >= 0; b--) check_answers("str of a Boolean", mt_eval(m, E("repr", E("py-call", E("str", B(b))))), T(python_spelling(b)));
    bool unsorted[] = { true, false };
    qsort(unsorted, 2, sizeof *unsorted, by_truth);
    check_answers("sorted, false first", mt_eval(m, E("py-call", E("sorted", E(B(true), B(false))))), E(B(unsorted[0]), B(unsorted[1])));
    const bool three[] = { true, false, true };
    check_answers("a list's length", mt_eval(m, E("py-call", E("len", E(B(three[0]), B(three[1]), B(three[2]))))), N(sizeof three / sizeof *three));
    check_answers("a Boolean's type", mt_eval(m, E("py-call", E("isinstance", B(true), E("py-call", E("type", B(false)))))), B(true));
    for (int64_t x = 1; x >= 0; x--) check_answers("a number's truth", mt_eval(m, E("py-call", E("bool", x))), B(x != 0));
    check_answers("a method on a Boolean", mt_eval(m, E("py-call", E(".bit_length", B(true)))), N(bit_length(true)));
    const char word[] = "abc";
    char upper[sizeof word];
    for (size_t i = 0; i < sizeof word; i++) upper[i] = (char)toupper((unsigned char)word[i]);
    check_answers("other symbols stay text", mt_eval(m, E("repr", E("py-call", E(".upper", word)))), T(upper));
    return done(m);
}
