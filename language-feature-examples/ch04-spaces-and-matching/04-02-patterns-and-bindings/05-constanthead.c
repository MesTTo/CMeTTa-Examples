/* Purpose: a pattern selects on shape. h is a C function that reads its first
 *   argument's shape the way the original's head (h (justdata haha $B) $C)
 *   does: an expression of three children whose first two are the symbols
 *   justdata and haha. Any other shape is no match, and h answers nothing.
 * Guarantees: (h (justdata haha 30) 40) is 70 [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static bool is_symbol(const mt_atom *atom, const char *name)
{
    return mt_kind_of(atom) == MT_SYMBOL && strcmp(mt_name(atom), name) == 0;
}

static mt_status h(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *data = mt_arg(call, 0);
    if (mt_len(data) != 3 || !is_symbol(mt_at(data, 0), "justdata") ||
        !is_symbol(mt_at(data, 1), "haha"))
        return MT_FAIL;                     /* not this shape: no answer */
    mt_clear();
    int64_t b = mt_int(mt_at(data, 2)), c = mt_int(mt_arg(call, 1));
    if (!mt_ok()) return mt_fail(call, "h adds two integers");
    return mt_answer(call, N(b + c));
}

int main(void)
{
    metta *m = open_engine();
    require("publish h", mt_def(m, (mt_op){ .name = "h", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = h }));
    check_int("(h (justdata haha 30) 40) is 70",
              mt_one_int(mt_eval(m, E("h", E("justdata", "haha", 30), 40))), 70);
    return done(m);
}
