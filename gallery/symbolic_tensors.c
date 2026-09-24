/* Purpose: symbols decide, BLAS computes. A double transpose (T (T x)) is
 *   rewritten away by an equation, and what is left, (gemm x y), is a C
 *   function calling cblas_dgemm on arrays decoded from (Matrix (Row ...))
 *   atoms, so the numeric kernel runs once and only on work the symbols
 *   could not remove.
 * Assumes: pkg-config openblas supplies cblas.h and its library.
 * Owns resources: the callback frees its three arrays before answering.
 * Guarantees: (MM (T (T A)) B) is A*B = ((11.0)), one GEMM call, and a shape
 *   mismatch is refused before BLAS sees it [tested: make check;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <cblas.h>
#include <limits.h>

typedef struct matrix { size_t rows, cols; double *cells; } matrix;

/* (Matrix (Row a b ...) ...) into a row-major array; false on any other shape. */
static bool decode(const mt_atom *atom, matrix *out)
{
    const char *head = mt_name(mt_at(atom, 0));
    if (!head || strcmp(head, "Matrix") != 0 || mt_len(atom) < 2) return false;
    out->rows = mt_len(atom) - 1;
    out->cols = mt_len(mt_at(atom, 1)) - 1;
    if (out->cols == 0 || out->rows > INT_MAX || out->cols > INT_MAX) return false;
    if (!(out->cells = calloc(out->rows * out->cols, sizeof *out->cells))) return false;
    mt_clear();
    for (size_t r = 0; r < out->rows; r++) {
        const mt_atom *row = mt_at(atom, r + 1);
        head = mt_name(mt_at(row, 0));
        if (!head || strcmp(head, "Row") != 0 || mt_len(row) != out->cols + 1) return false;
        for (size_t c = 0; c < out->cols; c++)
            out->cells[r * out->cols + c] = mt_float(mt_at(row, c + 1));
    }
    return mt_ok();
}

static mt_atom *encode(const double *cells, size_t rows, size_t cols)
{
    mt_atom **matrix_rows = mt_calloc(rows + 1, sizeof *matrix_rows);
    if (!matrix_rows) return NULL;
    matrix_rows[0] = S("Matrix");
    for (size_t r = 0; r < rows; r++) {
        mt_atom **row = mt_calloc(cols + 1, sizeof *row);
        if (!row) { for (size_t i = 0; i <= r; i++) mt_drop(matrix_rows[i]); mt_free(matrix_rows); return NULL; }
        row[0] = S("Row");
        for (size_t c = 0; c < cols; c++) row[c + 1] = R(cells[r * cols + c]);
        matrix_rows[r + 1] = mt_exprv(cols + 1, row);
        mt_free(row);
    }
    mt_atom *result = mt_exprv(rows + 1, matrix_rows);
    mt_free(matrix_rows);
    return result;
}

/* Dense GEMM: O(M*K*N) arithmetic and O(M*K + K*N + M*N) doubles. */
static mt_status gemm(mt_call *call, void *user)
{
    matrix a = {0}, b = {0};
    if (!decode(mt_arg(call, 0), &a) || !decode(mt_arg(call, 1), &b) || a.cols != b.rows) {
        free(a.cells); free(b.cells);
        return mt_fail(call, "gemm needs two numeric matrices whose inner dimensions agree");
    }
    double *c = calloc(a.rows * b.cols, sizeof *c);
    if (!c) { free(a.cells); free(b.cells); return mt_fail(call, "out of memory for the product"); }
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, (int)a.rows, (int)b.cols,
                (int)a.cols, 1.0, a.cells, (int)a.cols, b.cells, (int)b.cols, 0.0, c, (int)b.cols);
    ++*(size_t *)user;
    mt_atom *product = encode(c, a.rows, b.cols);
    free(a.cells); free(b.cells); free(c);
    return product ? mt_answer(call, product) : mt_fail(call, "out of memory for the answer");
}

int main(void)
{
    metta *m = open_engine();
    size_t calls = 0;
    require("publish gemm", mt_def(m, (mt_op){ .name = "gemm", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL,
                                               .fn = gemm, .user = &calls }));
    /* (= (MM (T (T $x)) $y) (gemm $x $y)): the symbolic identity */
    require("rewrite a double transpose away",
            mt_add(m, E("=", E("MM", E("T", E("T", V("x"))), V("y")), E("gemm", V("x"), V("y")))));

    mt_atom *a = E("Matrix", E("Row", 1.0, 2.0));
    mt_atom *b = E("Matrix", E("Row", 3.0), E("Row", 4.0));
    check_answers("the identity reaches BLAS, costed in the tropical carrier",
                  mt_eval_under(m, S("tropical"), E("MM", E("T", E("T", a)), b)),
                  E(E("Matrix", E("Row", 11.0)), 0));
    check_int("one numeric call", (int64_t)calls, 1);

    mt_clear();
    mt_atom *refused = mt_first(mt_eval(m, E("gemm", E("Matrix", E("Row", 1, 2)),
                                             E("Matrix", E("Row", 3)))));
    check("a shape mismatch is refused", refused == NULL && mt_error() == MT_ERROR);
    mt_clear();
    check_int("before BLAS sees it", (int64_t)calls, 1);
    require("withdraw gemm", mt_undef(m, "gemm"));
    return done(m);
}
