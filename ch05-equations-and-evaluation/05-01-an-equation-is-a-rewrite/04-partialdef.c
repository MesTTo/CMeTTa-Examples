/* Purpose: a definition that answers a partial application, and composition
 *   over partials. (mp) answers (+), so (mp 1 1) adds; (.. f g x) applies f
 *   to (g x), so (plus1times2 1) is 2 times (1 + 1). The same composition
 *   takes C function values made with mt_function(): C functions are atoms
 *   the equation applies like any other.
 * Guarantees: (mp 1 1) is 2, (plus1times2 1) is 4, and composing the C
 *   values times2 and add1 gives 4 too [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

/* (f x) for a C function value: user carries which one. */
static mt_status affine(mt_call *call, void *user)
{
    const int64_t *scale_add = user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "wants an integer");
    return mt_answer(call, N(scale_add[0] * x + scale_add[1]));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(= (mp) (+))", mt_add(m, E("=", E("mp"), E("+"))));
    assert(mt_one_int(mt_eval(m, E("mp", 1, 1))) == 2 && "(mp 1 1) adds");

    /* (= (.. $f1 $f2 $arg) ($f1 ($f2 $arg))) and (= (plus1times2) (.. (* 2) (+ 1))) */
    require("define ..", mt_add(m, E("=", E("..", V("f1"), V("f2"), V("arg")),
                                     E(V("f1"), E(V("f2"), V("arg"))))));
    require("define plus1times2", mt_add(m, E("=", E("plus1times2"), E("..", E("*", 2), E("+", 1)))));
    assert(mt_one_int(mt_eval(m, E("plus1times2", 1))) == 4 && "(plus1times2 1) is 4");

    static const int64_t times2[2] = { 2, 0 }, add1[2] = { 1, 1 };
    mt_atom *twice = mt_function(affine, (void *)times2, NULL);
    mt_atom *next = mt_function(affine, (void *)add1, NULL);
    assert(mt_one_int(mt_eval(m, E("..", twice, next, 1))) == 4
           && "the same composition over two C function values");
    mt_close(m);
    return 0;
}
