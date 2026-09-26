/* Purpose: a pattern selects on shape. h is a C function that reads its first
 *   argument's shape the way the original's head (h (justdata haha $B) $C)
 *   does: an expression of three children whose first two are the symbols
 *   justdata and haha. Any other shape is no match, and h answers nothing.
 * Guarantees: (h (justdata haha 30) 40) is 70
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish h", mt_def(m, (mt_op){ .name = "h", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = h }));
    assert(mt_one_int(mt_eval(m, E("h", E("justdata", "haha", 30), 40))) == 70
           && "(h (justdata haha 30) 40) is 70");
    mt_close(m);
    return 0;
}
