/* Purpose: functions applied right to left. lib_patrick's compose applies
 *   the innermost function to the argument list and each one outside it to
 *   the single answer inside; inc and double are C functions, and C
 *   composes the same list itself over a table of function pointers, the
 *   innermost of any arity. Composing a longer list is composing the
 *   shorter ones.
 * Guarantees: all nine claims of the original hold
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

static int64_t inc_(int64_t x) { return x + 1; }
static int64_t double_(int64_t x) { return x * 2; }
static int64_t plus_(int64_t a, int64_t b) { return a + b; }

static mt_status inc(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(inc_(mt_int(mt_arg(call, 0)))));
}
static mt_status twice(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(double_(mt_int(mt_arg(call, 0)))));
}

/* A function in a composition: its engine name and, in C, one arity. */
typedef struct { const char *name; int64_t (*one)(int64_t); int64_t (*two)(int64_t, int64_t); } fn;
static const fn INC = { "inc", inc_, NULL }, DOUBLE = { "double", double_, NULL }, PLUS = { "+", NULL, plus_ };

/* C's compose: the last function takes the arguments, each earlier one the
   answer after it. */
static int64_t composed(size_t n, const fn *const *fns, const int64_t *args)
{
    const fn *inner = fns[n - 1];
    int64_t x = inner->two ? inner->two(args[0], args[1]) : inner->one(args[0]);
    for (size_t i = n - 1; i-- > 0;) x = fns[i]->one(x);
    return x;
}

static mt_atom *names(size_t n, const fn *const *fns)
{
    mt_atom *kids[4];
    for (size_t i = 0; i < n; i++) kids[i] = S(fns[i]->name);
    return mt_exprv(n, kids);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_patrick", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_patrick")))));
    require("(: inc (-> Number Number))", mt_add(m, E(":", "inc", E("->", "Number", "Number"))));
    require("(: double (-> Number Number))", mt_add(m, E(":", "double", E("->", "Number", "Number"))));
    require("publish inc", mt_def(m, (mt_op){ .name = "inc", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = inc }));
    require("publish double", mt_def(m, (mt_op){ .name = "double", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = twice }));

    const int64_t five[] = { 5 }, two_three[] = { 2, 3 };
    const struct { const char *claim; size_t n; const fn *fns[3]; const int64_t *args; size_t argc; } rows[] = {
        { "one function is application", 1, { &INC }, five, 1 },
        { "another", 1, { &DOUBLE }, five, 1 },
        { "inc after double", 2, { &INC, &DOUBLE }, five, 1 },
        { "double after inc", 2, { &DOUBLE, &INC }, five, 1 },
        { "the innermost takes every argument", 1, { &PLUS }, two_three, 2 },
        { "and hands one answer out", 2, { &INC, &PLUS }, two_three, 2 },
        { "through two more", 3, { &DOUBLE, &INC, &PLUS }, two_three, 2 },
    };
    for (size_t i = 0; i < sizeof rows / sizeof *rows; i++) {
        mt_atom *args[2];
        for (size_t j = 0; j < rows[i].argc; j++) args[j] = N(rows[i].args[j]);
        assert(answers_are(mt_eval(m, E("compose", names(rows[i].n, rows[i].fns), mt_exprv(rows[i].argc, args))), E(composed(rows[i].n, rows[i].fns, rows[i].args)))
               && rows[i].claim);
    }

    mt_atom *inner = E("compose", E("double"), E(5));
    assert(answers_are(mt_eval(m, E("compose", E("inc"), E(mt_keep(inner)))), E(inc_(double_(5))))
           && "composing the composition");
    assert(answers_are(mt_eval(m, E("==", E("compose", E("inc", "double"), E(5)), E("compose", E("inc"), E(inner)))), E(B(true)))
           && "is composing the longer list");
    mt_close(m);
    return 0;
}
