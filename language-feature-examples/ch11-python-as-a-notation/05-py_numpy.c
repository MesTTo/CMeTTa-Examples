/* Purpose: numpy reached from C through the engine's Python. C holds each
 *   numpy callable in a variable, where the original binds a reader token,
 *   and each answer against its own: an absolute value against llabs, a sum
 *   that stays numpy's against C's +, the scalar's class against the name of
 *   C's int64_t, which is the type numpy's int64 is, and each arange against
 *   a C loop from start toward stop by step. The two class names C has no
 *   type for, numpy's ndarray and Python's int, are the ones numpy answers.
 * Guarantees: all nine claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <limits.h>

/* numpy.arange's answers: start, then by step while below stop. */
static mt_atom *arange(int64_t start, int64_t stop, int64_t step)
{
    size_t count = stop > start ? (size_t)((stop - start + step - 1) / step) : 0, n = 0;
    mt_atom **items = malloc((count + 1) * sizeof *items);
    require("room for the range", items != NULL);
    for (int64_t x = start; x < stop; x += step) items[n++] = N(x);
    mt_atom *out = mt_exprv(n, items);
    free(items);
    return out;
}

static mt_atom *class_name(mt_atom *object) { return E("py-dot", E("py-dot", object, "__class__"), "__name__"); }

int main(void)
{
    metta *m = open_engine();
    mt_atom *np_abs = mt_one(mt_eval(m, E("py-atom", "numpy.absolute"))), *np_array = mt_one(mt_eval(m, E("py-atom", "numpy.array"))),
            *np_arange = mt_one(mt_eval(m, E("py-atom", "numpy.arange"))), *np_random = mt_one(mt_eval(m, E("py-atom", "numpy.random")));
    require("numpy's callables", np_abs && np_array && np_arange && np_random);
    char int64_name[16];
    snprintf(int64_name, sizeof int64_name, "int%zu", sizeof(int64_t) * CHAR_BIT);

    check_answers("a numpy scalar's class", mt_eval(m, class_name(E(mt_keep(np_abs), -5))), T(int64_name));
    check_answers("its value", mt_eval(m, E(E("py-dot", E(mt_keep(np_abs), -5), "item"))), N(llabs(-5)));
    check_answers("a sum through Python's +", mt_eval(m, E(E("py-dot", E("+", E(mt_keep(np_abs), -5), 10), "item"))), N(llabs(-5) + 10));
    check_answers("that stays numpy's", mt_eval(m, class_name(E("+", E(mt_keep(np_abs), -5), 10))), T(int64_name));
    check_answers("an array from a Python expression", mt_eval(m, class_name(E(mt_keep(np_array), E("py-atom", T("[1, 2, 3]"))))), T("ndarray"));

    const struct { const char *claim; mt_atom *arg; int64_t start, stop, step; } ranges[] = {
        { "arange by stop", N(4), 0, 4, 1 },
        { "keywords skip the defaults", E("Kwargs", E("step", 2), E("stop", 8)), 0, 8, 2 },
        { "start, stop and step", E("Kwargs", E("start", 2), E("stop", 10), E("step", 3)), 2, 10, 3 },
    };
    for (size_t i = 0; i < 3; i++)
        check_answers(ranges[i].claim, mt_eval(m, E("collapse", E("py-iter", E(E("py-dot", E(mt_keep(np_arange), ranges[i].arg), "tolist"))))),
                      arange(ranges[i].start, ranges[i].stop, ranges[i].step));
    check_answers("a submodule held and reached into", mt_eval(m, class_name(E(E("py-dot", mt_keep(np_random), "randint"), 25))), T("int"));
    mt_drop(np_abs), mt_drop(np_array), mt_drop(np_arange), mt_drop(np_random);
    return done(m);
}
