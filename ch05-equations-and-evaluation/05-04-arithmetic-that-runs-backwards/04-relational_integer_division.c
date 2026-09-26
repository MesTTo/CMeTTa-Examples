/* Purpose: #// is C's own /, which C99 defines as truncation toward zero,
 *   and #div floors, which C writes as a correction of /. A table of
 *   operand pairs checks both against the C they name. C's arithmetic then
 *   rebuilds the dividend from the engine's #div and #mod and misses it by
 *   the divisor with #//. Backwards, mt_solve finds the divisor; and since 6
 *   and 7 both truncate to 3, the dividend is decided only once a guard term,
 *   (let True bound question), posts a second constraint.
 * Guarantees: all eleven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    for (size_t i = 0; i < sizeof OPERANDS / sizeof *OPERANDS; i++) {
        int64_t a = OPERANDS[i][0], b = OPERANDS[i][1];
        assert(answers_are(mt_eval(m, E("#//", a, b)), E(a / b)) && "#// truncates like C's /");
        assert(answers_are(mt_eval(m, E("#div", a, b)), E(floor_div(a, b))) && "#div floors");
    }

    int64_t floored = mt_one_int(mt_eval(m, E("#div", -7, 2)));
    int64_t truncated = mt_one_int(mt_eval(m, E("#//", -7, 2)));
    int64_t remainder = mt_one_int(mt_eval(m, E("#mod", -7, 2)));
    assert(floored * 2 + remainder == -7 && "#div and #mod rebuild the dividend");
    assert(truncated * 2 + remainder == -5 && "#// with #mod misses it by the divisor");

    assert(list_is(mt_all(mt_solve(m, N(3), E("#//", 7, V("d")))), E(2)) && "the divisor, solved for");
    mt_atom *halves_to_3 = E("let", 3, E("#//", V("n"), 2), V("n"));
    assert(answers_are(mt_eval(m, WHERE(E("#<", V("n"), 7), mt_keep(halves_to_3))), E(6)) && "below 7 it is 6");
    assert(answers_are(mt_eval(m, WHERE(E("#>", V("n"), 6), halves_to_3)), E(7)) && "above 6 it is 7");
    mt_close(m);
    return 0;
}
