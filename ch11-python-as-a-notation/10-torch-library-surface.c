/* Purpose: lib_torch's tensor surface held against C's own loops over
 *   doubles, wherever torch is importable, which C asks once, as the
 *   original does, and records in &torch-status. Construction is C filling
 *   an array; the four elementwise operations are C loops; a mean is C's sum
 *   over the count; relu is fmax(0, x) and sigmoid 1 / (1 + exp(-x)). Two
 *   normal draws differ, which C tells with mt_eq. The pipeline's third is
 *   float32 on torch's side and a double on C's, so it is held within the
 *   original's tolerance. Where torch is absent the twin says so and checks
 *   only that the recorded status is the probe's verdict.
 * Guarantees: the original's claims hold where torch is importable
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 10-torch-library-surface.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

static mt_atom *reals(const double *xs, size_t n)
{
    mt_atom **items = malloc((n + 1) * sizeof *items);
    require("room for a tensor", items != NULL);
    for (size_t i = 0; i < n; i++) items[i] = mt_real(xs[i]);
    mt_atom *out = mt_exprv(n, items);
    free(items);
    return out;
}

static mt_atom *tensor(const double *xs, size_t n) { return E("torch-tensor", reals(xs, n)); }
static mt_atom *tolist(mt_atom *t) { return E("torch-tolist", t); }

/* C's elementwise operation over two arrays of n. */
static mt_atom *elementwise(char op, const double *a, const double *b, size_t n)
{
    double *out = malloc((n + 1) * sizeof *out);
    require("room for a result", out != NULL);
    for (size_t i = 0; i < n; i++) out[i] = op == '+' ? a[i] + b[i] : op == '-' ? a[i] - b[i] : op == '*' ? a[i] * b[i] : a[i] / b[i];
    mt_atom *result = reals(out, n);
    free(out);
    return result;
}

static double mean(const double *xs, size_t n)
{
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += xs[i];
    return sum / (double)n;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_torch", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_torch")))));
    mt_atom *probe = E("if-error", E("catch", E("py-call", E("torch.zeros", 1))), "no", "yes");
    require("record the probe", mt_one_truth(mt_eval(m, E("add-reduct", "&torch-status", E("available", mt_keep(probe))))));
    mt_atom *verdict = mt_one(mt_eval(m, probe)), *recorded = mt_one(mt_eval(m, E("match", "&torch-status", E("available", V("v")), V("v"))));
    assert(verdict && recorded && mt_eq(verdict, recorded) && "the recorded status is the probe's verdict");
    bool available = verdict && strcmp(mt_name(verdict), "yes") == 0;
    mt_drop(verdict), mt_drop(recorded);
    if (!available) {
        puts("SKIPPED torch-library-surface: torch is not importable from this interpreter");
        mt_close(m);
        return 0;
    }

    const double zeros[3] = { 0 }, ones[4] = { 1, 1, 1, 1 };
    assert(answers_are(mt_eval(m, tolist(E("torch-zeros", 3))), E(reals(zeros, 3))) && "zeros");
    assert(answers_are(mt_eval(m, tolist(E("torch-ones", 3))), E(reals(ones, 3))) && "ones");
    assert(answers_are(mt_eval(m, tolist(E("torch-arange", 4))), E(E(0, 1, 2, 3))) && "an integer range");
    assert(answers_are(mt_eval(m, E("size-atom", tolist(E("torch-randn", 5)))), E(N(5))) && "a draw of five");
    mt_atom *first = mt_one(mt_eval(m, tolist(E("torch-randn", 8)))), *second = mt_one(mt_eval(m, tolist(E("torch-randn", 8))));
    assert(first && second && !mt_eq(first, second) && "two draws differ");
    mt_drop(first), mt_drop(second);

    const struct { const char *claim, *name; char op; double a[2], b[2]; } binary[] = {
        { "add", "torch-add", '+', { 1, 2 }, { 10, 20 } },
        { "subtract", "torch-sub", '-', { 10, 20 }, { 1, 2 } },
        { "multiply", "torch-mul", '*', { 2, 3 }, { 4, 5 } },
        { "divide", "torch-div", '/', { 10, 20 }, { 4, 5 } },
    };
    for (size_t i = 0; i < 4; i++)
        assert(answers_are(mt_eval(m, tolist(E(binary[i].name, tensor(binary[i].a, 2), tensor(binary[i].b, 2)))), E(elementwise(binary[i].op, binary[i].a, binary[i].b, 2)))
               && binary[i].claim);

    const double three[3] = { 1, 2, 3 };
    assert(answers_are(mt_eval(m, E("torch-item", E("torch-mean", tensor(three, 3)))), E(mt_real(mean(three, 3)))) && "a mean");
    assert(answers_are(mt_eval(m, E("torch-item", E("torch-mean", E("torch-ones", 4)))), E(mt_real(mean(ones, 4)))) && "a mean of ones");
    const double hinge[3] = { -2, 0, 3 };
    double relu[3];
    for (size_t i = 0; i < 3; i++) relu[i] = fmax(0, hinge[i]);
    assert(answers_are(mt_eval(m, tolist(E("torch-relu", tensor(hinge, 3)))), E(reals(relu, 3))) && "relu");
    assert(answers_are(mt_eval(m, E("torch-item", E("torch-sigmoid", E("torch-tensor", 0.0)))), E(mt_real(1 / (1 + exp(-0.0))))) && "sigmoid at zero");

    const double twos[3] = { 2, 2, 2 };
    double shifted[3];
    for (size_t i = 0; i < 3; i++) shifted[i] = fmax(0, three[i] - twos[i]);
    mt_atom *third = mt_one(mt_eval(m, E("torch-item", E("torch-mean", E("torch-relu", E("torch-sub", tensor(three, 3), tensor(twos, 3)))))));
    assert(third && fabs(mt_float(third) - mean(shifted, 3)) < 0.000001 && "a pipeline within float32 of C's double");
    mt_drop(third), mt_drop(probe);
    mt_close(m);
    return 0;
}
