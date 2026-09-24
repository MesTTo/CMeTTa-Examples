/* Purpose: #// is C's own /, which C99 defines as truncation toward zero,
 *   and #div floors, which C writes as a correction of /. A table of
 *   operand pairs checks both against the C they name. C's arithmetic then
 *   rebuilds the dividend from the engine's #div and #mod and misses it by
 *   the divisor with #//. Backwards, mt_solve finds the divisor; and since 6
 *   and 7 both truncate to 3, the dividend is decided only once a guard term,
 *   (let True bound question), posts a second constraint.
 * Guarantees: all eleven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

#define WHERE(cond, answer) E("let", B(true), (cond), (answer))

/* Floored division: / truncates, so step down when the signs differ and a
   remainder is left. */
static int64_t floor_div(int64_t a, int64_t b)
{
    return a / b - (a % b != 0 && (a < 0) != (b < 0));
}

static const int64_t OPERANDS[][2] = { {7, 2}, {-7, 2}, {7, -2} };

int main(void)
{
    metta *m = open_engine();

    for (size_t i = 0; i < sizeof OPERANDS / sizeof *OPERANDS; i++) {
        int64_t a = OPERANDS[i][0], b = OPERANDS[i][1];
        check_answers("#// truncates like C's /", mt_eval(m, E("#//", a, b)), a / b);
        check_answers("#div floors", mt_eval(m, E("#div", a, b)), floor_div(a, b));
    }

    int64_t floored = mt_one_int(mt_eval(m, E("#div", -7, 2)));
    int64_t truncated = mt_one_int(mt_eval(m, E("#//", -7, 2)));
    int64_t remainder = mt_one_int(mt_eval(m, E("#mod", -7, 2)));
    check_int("#div and #mod rebuild the dividend", floored * 2 + remainder, -7);
    check_int("#// with #mod misses it by the divisor", truncated * 2 + remainder, -5);

    check_list("the divisor, solved for", mt_all(mt_solve(m, N(3), E("#//", 7, V("d")))), 2);
    mt_atom *halves_to_3 = E("let", 3, E("#//", V("n"), 2), V("n"));
    check_answers("below 7 it is 6", mt_eval(m, WHERE(E("#<", V("n"), 7), mt_keep(halves_to_3))), 6);
    check_answers("above 6 it is 7", mt_eval(m, WHERE(E("#>", V("n"), 6), halves_to_3)), 7);
    return done(m);
}
