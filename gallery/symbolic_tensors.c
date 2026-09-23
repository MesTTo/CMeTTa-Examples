/* Purpose: lower a double transpose to one CBLAS matrix multiplication.
 * Owns resources: callback arrays are freed before returning owned result atoms.
 * Assumes: pkg-config openblas supplies cblas.h and its library.
 * Guarantees: numeric result, call count and shape refusal are checked
 *   [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
#include <cblas.h>
#include <limits.h>
typedef struct matrix { size_t rows, cols; double *cells; } matrix;
static bool decode(const mt_atom *atom, matrix *out)
{
    const char *head = mt_name(mt_at(atom, 0));
    if (!head || strcmp(head, "Matrix") || mt_len(atom) < 2) return false;
    out->rows = mt_len(atom) - 1;
    out->cols = mt_len(mt_at(atom, 1));
    if (out->cols < 2) return false;
    --out->cols;
    if (out->rows > INT_MAX || out->cols > INT_MAX || out->rows > SIZE_MAX / out->cols / sizeof(double)) return false;
    out->cells = calloc(out->rows * out->cols, sizeof(double));
    if (!out->cells) return false;
    for (size_t r = 0; r < out->rows; ++r) {
        const mt_atom *row = mt_at(atom, r + 1);
        head = mt_name(mt_at(row, 0));
        if (!head || strcmp(head, "Row") || mt_len(row) != out->cols + 1) return false;
        for (size_t c = 0; c < out->cols; ++c) out->cells[r*out->cols+c] = mt_float(mt_at(row, c+1));
    }
    return mt_ok();
}
/* Dense GEMM: O(MKN) arithmetic, O(MK+KN+MN) owned numeric cells. */
static mt_status multiply(mt_call *call, void *user)
{
    matrix a = {0}, b = {0};
    if (!decode(mt_arg(call, 0), &a) || !decode(mt_arg(call, 1), &b) || a.cols != b.rows ||
        a.rows > SIZE_MAX / b.cols / sizeof(double)) {
        free(a.cells); free(b.cells); return mt_fail(call, "GEMM needs compatible rectangular numeric matrices");
    }
    double *c = calloc(a.rows*b.cols, sizeof(*c));
    if (!c) { free(a.cells); free(b.cells); return mt_fail(call, "allocate GEMM result"); }
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, (int)a.rows, (int)b.cols,
        (int)a.cols, 1.0, a.cells, (int)a.cols, b.cells, (int)b.cols, 0.0, c, (int)b.cols);
    ++*(size_t *)user;
    mt_atom **rows = mt_calloc(a.rows+1, sizeof(*rows));
    check("allocate matrix rows", rows != NULL); rows[0] = mt_sym("Matrix");
    for (size_t r = 0; r < a.rows; ++r) {
        mt_atom **cells = mt_calloc(b.cols+1, sizeof(*cells)); check("allocate row", cells != NULL);
        cells[0] = mt_sym("Row");
        for (size_t col = 0; col < b.cols; ++col) cells[col+1] = mt_real(c[r*b.cols+col]);
        rows[r+1] = mt_exprv(b.cols+1, cells); mt_free(cells);
    }
    mt_atom *result = mt_exprv(a.rows+1, rows); mt_free(rows);
    free(a.cells); free(b.cells); free(c);
    return mt_answer(call, result);
}
int main(void)
{
    metta *m = open_engine(); size_t calls = 0;
    check("publish GEMM", mt_def(m, (mt_op){.name="gemm", .arity=2, .effect=MT_PURE, .fn=multiply, .user=&calls}));
    check("lower symbolic identity", mt_lower(m, (MM (T (T $x)) $y), (gemm $x $y)));
    check_answers("symbolic identity reaches BLAS", mt_eval_under(m, mt_sym("tropical"),
        mt_parse("(MM (T (T (Matrix (Row 1.0 2.0)))) (Matrix (Row 3.0) (Row 4.0)))")), "((Matrix (Row 11.0)) 0)");
    check("one numeric operation", calls == 1);
    mt_list invalid = mt_all(mt_run(m, "!(gemm (Matrix (Row 1 2)) (Matrix (Row 3)))"));
    check("shape mismatch refuses", !mt_ok() && invalid.len == 0); mt_list_free(invalid); mt_clear();
    check("invalid shape never reaches BLAS", calls == 1);
    check("withdraw GEMM", mt_undef(m, "gemm"));
    return done(m, "symbolic_tensors");
}
