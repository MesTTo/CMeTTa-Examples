/* Purpose: the engine's bit operations are C's operators over unbounded
 *   integers. A table pairs each operation with the C expression it names,
 *   & | ^ << >> computed by the compiler; ~ is bit-not; where MeTTa's
 *   integers outgrow int64, 3 << 62 is C's unsigned shift read back with
 *   mt_unum, and a right shift of a negative number is the arithmetic one,
 *   which C leaves to the implementation and the engine defines. A float is
 *   refused in the operation's own words, and a mask reads as a predicate
 *   with C's comparison on the engine's bit-and.
 * Guarantees: all fifteen claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    for (size_t i = 0; i < sizeof BINARY / sizeof *BINARY; i++)
        assert(answers_are(mt_eval(m, E(BINARY[i].op, BINARY[i].a, BINARY[i].b)), E(BINARY[i].c)) && BINARY[i].op);
    assert(answers_are(mt_eval(m, E("bit-shift-left", 3, 62)), E(mt_unum(UINT64_C(3) << 62))) && "3 << 62 outgrows int64");

    assert(answers_are(mt_eval(m, E("bit-not", 0)), E(~INT64_C(0))) && "bit-not is ~");
    assert(answers_are(mt_eval(m, E("bit-not", 12)), E(~INT64_C(12))) && "~12 is -13");
    assert(answers_are(mt_eval(m, E("bit-not", E("bit-not", 12))), E(12)) && "~~12 is 12");

    assert(answers_are(mt_eval(m, E("bit-and", 1.5, 2)), E(E("Error", E("bit-and", 1.5, 2), T("bit-and expects two integers"))))
           && "a float is refused");
    assert(answers_are(mt_eval(m, E("bit-shift-left", 1.5, 2)), E(E("Error", E("bit-shift-left", 1.5, 2),
                                                                    T("bit-shift-left expects two arguments: integer (value) and non-negative integer (count)"))))
           && "and a shift says what its count must be");

    assert(mt_one_int(mt_eval(m, E("bit-and", 12, 1 << 2))) == 1 << 2 && "bit 2 of 12 is set");
    assert(mt_one_int(mt_eval(m, E("bit-and", 12, 1 << 1))) != 1 << 1 && "bit 1 of 12 is not");
    mt_close(m);
    return 0;
}
