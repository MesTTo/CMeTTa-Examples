/* Purpose: the engine's bit operations are C's operators over unbounded
 *   integers. A table pairs each operation with the C expression it names,
 *   & | ^ << >> computed by the compiler; ~ is bit-not; where MeTTa's
 *   integers outgrow int64, 3 << 62 is C's unsigned shift read back with
 *   mt_unum, and a right shift of a negative number is the arithmetic one,
 *   which C leaves to the implementation and the engine defines. A float is
 *   refused in the operation's own words, and a mask reads as a predicate
 *   with C's comparison on the engine's bit-and.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static const struct { const char *op; int64_t a, b, c; } BINARY[] = {
    { "bit-and", 12, 10, 12 & 10 },         /* 1100 & 1010 = 1000 */
    { "bit-or", 12, 10, 12 | 10 },          /* 1110 */
    { "bit-xor", 12, 10, 12 ^ 10 },         /* 0110 */
    { "bit-shift-left", 1, 4, 1 << 4 },
    { "bit-shift-right", 16, 2, 16 >> 2 },
    { "bit-shift-right", -8, 1, -4 },       /* arithmetic: the sign stays */
    { "bit-shift-right", -1, 40, -1 },
};

int main(void)
{
    metta *m = open_engine();

    for (size_t i = 0; i < sizeof BINARY / sizeof *BINARY; i++)
        check_answers(BINARY[i].op, mt_eval(m, E(BINARY[i].op, BINARY[i].a, BINARY[i].b)), BINARY[i].c);
    check_answers("3 << 62 outgrows int64", mt_eval(m, E("bit-shift-left", 3, 62)), mt_unum(UINT64_C(3) << 62));

    check_answers("bit-not is ~", mt_eval(m, E("bit-not", 0)), ~INT64_C(0));
    check_answers("~12 is -13", mt_eval(m, E("bit-not", 12)), ~INT64_C(12));
    check_answers("~~12 is 12", mt_eval(m, E("bit-not", E("bit-not", 12))), 12);

    check_answers("a float is refused", mt_eval(m, E("bit-and", 1.5, 2)),
                  E("Error", E("bit-and", 1.5, 2), T("bit-and expects two integers")));
    check_answers("and a shift says what its count must be", mt_eval(m, E("bit-shift-left", 1.5, 2)),
                  E("Error", E("bit-shift-left", 1.5, 2),
                    T("bit-shift-left expects two arguments: integer (value) and non-negative integer (count)")));

    check("bit 2 of 12 is set", mt_one_int(mt_eval(m, E("bit-and", 12, 1 << 2))) == 1 << 2);
    check("bit 1 of 12 is not", mt_one_int(mt_eval(m, E("bit-and", 12, 1 << 1))) != 1 << 1);
    return done(m);
}
