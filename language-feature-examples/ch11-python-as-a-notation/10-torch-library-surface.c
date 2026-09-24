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
 *   [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

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
    metta *m = open_engine();
    require("import lib_torch", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_torch")))));
    mt_atom *probe = E("if-error", E("catch", E("py-call", E("torch.zeros", 1))), "no", "yes");
    require("record the probe", mt_one_truth(mt_eval(m, E("add-reduct", "&torch-status", E("available", mt_keep(probe))))));
    mt_atom *verdict = mt_one(mt_eval(m, probe)), *recorded = mt_one(mt_eval(m, E("match", "&torch-status", E("available", V("v")), V("v"))));
    check("the recorded status is the probe's verdict", verdict && recorded && mt_eq(verdict, recorded));
    bool available = verdict && strcmp(mt_name(verdict), "yes") == 0;
    mt_drop(verdict), mt_drop(recorded);
    if (!available) {
        puts("SKIPPED torch-library-surface: torch is not importable from this interpreter");
        return done(m);
    }

    const double zeros[3] = { 0 }, ones[4] = { 1, 1, 1, 1 };
    check_answers("zeros", mt_eval(m, tolist(E("torch-zeros", 3))), reals(zeros, 3));
    check_answers("ones", mt_eval(m, tolist(E("torch-ones", 3))), reals(ones, 3));
    check_answers("an integer range", mt_eval(m, tolist(E("torch-arange", 4))), E(0, 1, 2, 3));
    check_answers("a draw of five", mt_eval(m, E("size-atom", tolist(E("torch-randn", 5)))), N(5));
    mt_atom *first = mt_one(mt_eval(m, tolist(E("torch-randn", 8)))), *second = mt_one(mt_eval(m, tolist(E("torch-randn", 8))));
    check("two draws differ", first && second && !mt_eq(first, second));
    mt_drop(first), mt_drop(second);

    const struct { const char *claim, *name; char op; double a[2], b[2]; } binary[] = {
        { "add", "torch-add", '+', { 1, 2 }, { 10, 20 } },
        { "subtract", "torch-sub", '-', { 10, 20 }, { 1, 2 } },
        { "multiply", "torch-mul", '*', { 2, 3 }, { 4, 5 } },
        { "divide", "torch-div", '/', { 10, 20 }, { 4, 5 } },
    };
    for (size_t i = 0; i < 4; i++)
        check_answers(binary[i].claim, mt_eval(m, tolist(E(binary[i].name, tensor(binary[i].a, 2), tensor(binary[i].b, 2)))),
                      elementwise(binary[i].op, binary[i].a, binary[i].b, 2));

    const double three[3] = { 1, 2, 3 };
    check_answers("a mean", mt_eval(m, E("torch-item", E("torch-mean", tensor(three, 3)))), mt_real(mean(three, 3)));
    check_answers("a mean of ones", mt_eval(m, E("torch-item", E("torch-mean", E("torch-ones", 4)))), mt_real(mean(ones, 4)));
    const double hinge[3] = { -2, 0, 3 };
    double relu[3];
    for (size_t i = 0; i < 3; i++) relu[i] = fmax(0, hinge[i]);
    check_answers("relu", mt_eval(m, tolist(E("torch-relu", tensor(hinge, 3)))), reals(relu, 3));
    check_answers("sigmoid at zero", mt_eval(m, E("torch-item", E("torch-sigmoid", E("torch-tensor", 0.0)))), mt_real(1 / (1 + exp(-0.0))));

    const double twos[3] = { 2, 2, 2 };
    double shifted[3];
    for (size_t i = 0; i < 3; i++) shifted[i] = fmax(0, three[i] - twos[i]);
    mt_atom *third = mt_one(mt_eval(m, E("torch-item", E("torch-mean", E("torch-relu", E("torch-sub", tensor(three, 3), tensor(twos, 3)))))));
    check("a pipeline within float32 of C's double", third && fabs(mt_float(third) - mean(shifted, 3)) < 0.000001);
    mt_drop(third), mt_drop(probe);
    return done(m);
}
