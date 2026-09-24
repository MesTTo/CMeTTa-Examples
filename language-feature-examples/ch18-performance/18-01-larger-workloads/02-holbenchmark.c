/* Purpose: four million-step kernels in MeTTa's recursive style, run by the
 *   engine under the raised branch budget the original states, each held
 *   against what C computes the way C computes it. The equations are the
 *   original's, lowered from C tokens. C writes each kernel as a loop over an
 *   array and passes the function as a C function pointer where the original
 *   passes (+ 1) or +: map-flat over range is a map over the array range
 *   fills, fold-nested over deep-nest is a fold over its rows' cells,
 *   apply-many is n applications, and poly is a sum of f(k) for k from n down
 *   to 1. One body for both languages, as the recursion chapter writes them,
 *   would recurse a million C frames deep for poly, so the C side is its own
 *   loop and the engine's answers must be that loop's.
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* deep-nest's row is (range 50); a macro, so the lowered equation and the C
   fold read one width. */
#define ROW 50

typedef int64_t (*unary)(int64_t);
typedef int64_t (*binary)(int64_t, int64_t);

static int64_t increment(int64_t x) { return x + 1; }       /* (+ 1) */
static int64_t plus(int64_t a, int64_t b) { return a + b; } /* +     */

/* (range n) is n, n-1, ..., 1, the order its recursion conses them. */
static void range(int64_t *out, int64_t n)
{
    for (int64_t i = 0; i < n; i++) out[i] = n - i;
}

/* (map-flat f xs) in place, keeping the array's length. Time: n calls of f. */
static int64_t map_flat(unary f, int64_t *xs, int64_t n)
{
    for (int64_t i = 0; i < n; i++) xs[i] = f(xs[i]);
    return n;
}

/* (fold-nested f init rows) over rows of equal width, left to right, which is
   the order the original descends into each row. Time: rows * width calls. */
static int64_t fold_nested(binary f, int64_t init, const int64_t *cells, int64_t count)
{
    for (int64_t i = 0; i < count; i++) init = f(init, cells[i]);
    return init;
}

static int64_t apply_many(unary f, int64_t n, int64_t x)
{
    while (n-- > 0) x = f(x);
    return x;
}

/* (poly f n) is (+ (f n) (poly f (- n 1))) down to 0. */
static int64_t poly(unary f, int64_t n)
{
    int64_t sum = 0;
    for (int64_t k = n; k > 0; k--) sum += f(k);
    return sum;
}

/* A goal under the original's budget, (with-pragma! ((max-stack-depth
   100000000)) goal). TAKES goal. */
static mt_atom *deep(mt_atom *goal) { return E("with-pragma!", E(E("max-stack-depth", 100000000)), goal); }

int main(void)
{
    metta *m = open_engine();
    require("map-flat", mt_lower(m, (map-flat $f ()), ()));
    require("map-flat", mt_lower(m, (map-flat $f (cons $x $xs)),
                                 (let $head ($f $x) (let $rest (map-flat $f $xs) (cons $head $rest)))));
    require("range", mt_lower(m, (range $n), (if (== $n 0) () (let $rest (range (- $n 1)) (cons $n $rest)))));
    require("fold-nested", mt_lower(m, (fold-nested $f $init ()), $init));
    require("fold-nested", mt_lower(m, (fold-nested $f $init (cons $x $xs)),
                                    (if (is-expr $x) (fold-nested $f (fold-nested $f $init $x) $xs)
                                                     (fold-nested $f ($f $init $x) $xs))));
    require("deep-nest", mt_lower(m, (deep-nest $n),
                                  (if (== $n 0) () (let $row (range ROW) (let $rest (deep-nest (- $n 1)) (cons $row $rest))))));
    require("apply-many", mt_lower(m, (apply-many $f $n $x), (if (== $n 0) $x (apply-many $f (- $n 1) ($f $x)))));
    require("poly", mt_lower(m, (poly $f $n), (if (== $n 0) 0 (+ ($f $n) (poly $f (- $n 1))))));

    const int64_t n = 1000000, rows = 20000, steps = 100000;
    int64_t *cells = malloc((size_t)(n > rows * ROW ? n : rows * ROW) * sizeof *cells);
    require("room for the C kernels", cells != NULL);

    range(cells, n);
    check_answers("a map keeps its list's length", mt_eval(m, deep(E("let", V("temp"), E("map-flat", E("+", 1), E("range", n)),
                                                                     E("length", V("temp"))))),
                  map_flat(increment, cells, n));

    for (int64_t r = 0; r < rows; r++) range(cells + r * ROW, ROW);
    check_answers("a fold over twenty thousand rows", mt_eval(m, deep(E("fold-nested", "+", 0, E("deep-nest", rows)))),
                  fold_nested(plus, 0, cells, rows * ROW));

    check_answers("a hundred thousand applications", mt_eval(m, deep(E("apply-many", E("+", 1), steps, 0))),
                  apply_many(increment, steps, 0));
    check_answers("a sum over a million terms", mt_eval(m, deep(E("poly", E("+", 1), n))), poly(increment, n));
    free(cells);
    return done(m);
}
