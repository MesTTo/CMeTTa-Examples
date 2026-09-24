/* Purpose: functions applied right to left. lib_patrick's compose applies
 *   the innermost function to the argument list and each one outside it to
 *   the single answer inside; inc and double are C functions, and C
 *   composes the same list itself over a table of function pointers, the
 *   innermost of any arity. Composing a longer list is composing the
 *   shorter ones.
 * Guarantees: all nine claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
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
        check_answers(rows[i].claim, mt_eval(m, E("compose", names(rows[i].n, rows[i].fns), mt_exprv(rows[i].argc, args))),
                      composed(rows[i].n, rows[i].fns, rows[i].args));
    }

    mt_atom *inner = E("compose", E("double"), E(5));
    check_answers("composing the composition", mt_eval(m, E("compose", E("inc"), E(mt_keep(inner)))),
                  inc_(double_(5)));
    check_answers("is composing the longer list",
                  mt_eval(m, E("==", E("compose", E("inc", "double"), E(5)), E("compose", E("inc"), E(inner)))), B(true));
    return done(m);
}
