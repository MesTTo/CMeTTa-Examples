/* Purpose: a C array becomes an expression of any length. mt_exprv() takes
 *   the children and copies the vector, so the array stays the caller's, and
 *   the expression crosses the engine and back element for element.
 * Owns resources: the child vector is mt_calloc'd and freed after mt_exprv().
 * Guarantees: every element round-trips, in order [tested: make check;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    static const int64_t values[] = { 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377,
                                      610, 987, 1597, 2584, 4181, 6765, 10946 };
    const size_t count = sizeof values / sizeof values[0];   /* more than mt_expr's 16 */

    mt_atom **children = mt_calloc(count, sizeof *children);
    require("allocate the child vector", children != NULL);
    for (size_t i = 0; i < count; i++) children[i] = N(values[i]);
    mt_atom *sequence = mt_exprv(count, children);
    mt_free(children);     /* mt_exprv took the children and copied the vector */

    check_int("every element is a child", (int64_t)mt_len(sequence), (int64_t)count);
    mt_list back = mt_all(mt_eval(m, E("superpose", mt_keep(sequence))));
    bool same = back.len == count;
    for (size_t i = 0; same && i < count; i++) same = mt_int(back.items[i]) == values[i];
    check("the engine answers each element, in order", same && mt_ok());
    mt_list_free(back);
    mt_drop(sequence);
    return done(m);
}
