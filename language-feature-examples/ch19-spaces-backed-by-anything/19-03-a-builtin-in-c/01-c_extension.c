/* Purpose: a builtin written in C, called from MeTTa with nothing in
 *   between. The original compiles cbump.c against SWI's foreign interface
 *   and loads it through a Prolog loader; in C the builtin is a C function
 *   published with mt_def, which is the C seat's own door for exactly this,
 *   and BUMP is its body, one expression lowering.h's C operators compile.
 *   A published function answers at once, so there is no runnable to wait
 *   for as the original's import has.
 * Guarantees: the original's guarded claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define BUMP(ADD, x) ADD(x, 1)

static mt_status c_bump(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 0)) != MT_INT) return MT_FAIL;
    return mt_answer(call, N(BUMP(C_ADD, mt_int(mt_arg(call, 0)))));
}

int main(void)
{
    metta *m = open_engine();
    /* The original's own imports: its loader needs lib_import and its guard
       lib_file, and C needs neither, but &self holds what they define on
       both sides. */
    static const char *const libraries[] = { "lib_import", "lib_file" };
    for (size_t i = 0; i < sizeof libraries / sizeof *libraries; i++)
        require(libraries[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));
    require("c-bump", mt_def(m, (mt_op){ .name = "c-bump", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = c_bump }));
    check_answers("(c-bump 41) is C's", mt_eval(m, E("c-bump", 41)), BUMP(C_ADD, 41));
    return done(m);
}
