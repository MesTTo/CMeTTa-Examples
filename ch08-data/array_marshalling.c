/* Purpose: a C array becomes an expression of any length. mt_exprv() takes
 *   the children and copies the vector, so the array stays the caller's, and
 *   the expression crosses the engine and back element for element.
 * Owns resources: the child vector is mt_calloc'd and freed after mt_exprv().
 * Guarantees: every element round-trips, in order
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const int64_t values[] = { 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377,
                                      610, 987, 1597, 2584, 4181, 6765, 10946 };
    const size_t count = sizeof values / sizeof values[0];   /* more than mt_expr's 16 */

    mt_atom **children = mt_calloc(count, sizeof *children);
    require("allocate the child vector", children != NULL);
    for (size_t i = 0; i < count; i++) children[i] = N(values[i]);
    mt_atom *sequence = mt_exprv(count, children);
    mt_free(children);     /* mt_exprv took the children and copied the vector */

    assert((int64_t)mt_len(sequence) == (int64_t)count && "every element is a child");
    mt_list back = mt_all(mt_eval(m, E("superpose", mt_keep(sequence))));
    bool same = back.len == count;
    for (size_t i = 0; same && i < count; i++) same = mt_int(back.items[i]) == values[i];
    assert(same && mt_ok() && "the engine answers each element, in order");
    mt_list_free(back);
    mt_drop(sequence);
    mt_close(m);
    return 0;
}
