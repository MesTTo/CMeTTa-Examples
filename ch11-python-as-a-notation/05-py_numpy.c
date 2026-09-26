/* Purpose: numpy reached from C through the engine's Python. C holds each
 *   numpy callable in a variable, where the original binds a reader token,
 *   and each answer against its own: an absolute value against llabs, a sum
 *   that stays numpy's against C's +, the scalar's class against the name of
 *   C's int64_t, which is the type numpy's int64 is, and each arange against
 *   a C loop from start toward stop by step. The two class names C has no
 *   type for, numpy's ndarray and Python's int, are the ones numpy answers.
 * Guarantees: all nine claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *np_abs = mt_one(mt_eval(m, E("py-atom", "numpy.absolute"))), *np_array = mt_one(mt_eval(m, E("py-atom", "numpy.array"))),
            *np_arange = mt_one(mt_eval(m, E("py-atom", "numpy.arange"))), *np_random = mt_one(mt_eval(m, E("py-atom", "numpy.random")));
    require("numpy's callables", np_abs && np_array && np_arange && np_random);
    char int64_name[16];
    snprintf(int64_name, sizeof int64_name, "int%zu", sizeof(int64_t) * CHAR_BIT);

    assert(answers_are(mt_eval(m, class_name(E(mt_keep(np_abs), -5))), E(T(int64_name))) && "a numpy scalar's class");
    assert(answers_are(mt_eval(m, E(E("py-dot", E(mt_keep(np_abs), -5), "item"))), E(N(llabs(-5)))) && "its value");
    assert(answers_are(mt_eval(m, E(E("py-dot", E("+", E(mt_keep(np_abs), -5), 10), "item"))), E(N(llabs(-5) + 10))) && "a sum through Python's +");
    assert(answers_are(mt_eval(m, class_name(E("+", E(mt_keep(np_abs), -5), 10))), E(T(int64_name))) && "that stays numpy's");
    assert(answers_are(mt_eval(m, class_name(E(mt_keep(np_array), E("py-atom", T("[1, 2, 3]"))))), E(T("ndarray"))) && "an array from a Python expression");

    const struct { const char *claim; mt_atom *arg; int64_t start, stop, step; } ranges[] = {
        { "arange by stop", N(4), 0, 4, 1 },
        { "keywords skip the defaults", E("Kwargs", E("step", 2), E("stop", 8)), 0, 8, 2 },
        { "start, stop and step", E("Kwargs", E("start", 2), E("stop", 10), E("step", 3)), 2, 10, 3 },
    };
    for (size_t i = 0; i < 3; i++)
        assert(answers_are(mt_eval(m, E("collapse", E("py-iter", E(E("py-dot", E(mt_keep(np_arange), ranges[i].arg), "tolist"))))), E(arange(ranges[i].start, ranges[i].stop, ranges[i].step)))
               && ranges[i].claim);
    assert(answers_are(mt_eval(m, class_name(E(E("py-dot", mt_keep(np_random), "randint"), 25))), E(T("int"))) && "a submodule held and reached into");
    mt_drop(np_abs), mt_drop(np_array), mt_drop(np_arange), mt_drop(np_random);
    mt_close(m);
    return 0;
}
