/* Purpose: which heads run. f is a C function; g answers its arguments
 *   under a head nothing defines, so they stay data; h applies whatever
 *   function it is handed; notjustdata answers the name f, which then
 *   applies; and a data expression with a call inside it has the call
 *   reduced. f is reached by name from every one of them.
 * Guarantees: the five answers of the original, evaluated in one tuple
 *   [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status twice(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "f doubles an integer");
    return mt_answer(call, N(x * 2));
}

int main(void)
{
    metta *m = open_engine();
    require("publish f", mt_def(m, (mt_op){ .name = "f", .arity = 1, .effect = MT_PURE, .fn = twice }));
    require("(= (g $f $x) (justdata $f $x))",
            mt_add(m, E("=", E("g", V("f"), V("x")), E("justdata", V("f"), V("x")))));
    require("(= (h $f $x) ($f $x))", mt_add(m, E("=", E("h", V("f"), V("x")), E(V("f"), V("x")))));
    require("(= (notjustdata $x) f)", mt_add(m, E("=", E("notjustdata", V("x")), "f")));
    require("(= (datawithnondatacomponent) ((lol (f 42))))",
            mt_add(m, E("=", E("datawithnondatacomponent"), E(E("lol", E("f", 42))))));

    check_answers("each head does what its equation says",
                  mt_eval(m, E(E("f", 21), E("g", "f", 2), E("h", "f", 2),
                               E(E("notjustdata", 42), 21), E("datawithnondatacomponent"))),
                  E(42, E("justdata", "f", 2), 4, 42, E(E("lol", 84))));
    return done(m);
}
