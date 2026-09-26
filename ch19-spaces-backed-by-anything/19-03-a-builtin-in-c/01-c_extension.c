/* Purpose: a builtin written in C, called from MeTTa with nothing in
 *   between. The original compiles cbump.c against SWI's foreign interface
 *   and loads it through a Prolog loader; in C the builtin is a C function
 *   published with mt_def, which is the C seat's own door for exactly this,
 *   and BUMP is its body, one expression the C_ operators compile.
 *   A published function answers at once, so there is no runnable to wait
 *   for as the original's import has.
 * Guarantees: the original's guarded claim holds
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_ADD(a, b) ((a) + (b))

#define BUMP(ADD, x) ADD(x, 1)

static mt_status c_bump(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 0)) != MT_INT) return MT_FAIL;
    return mt_answer(call, N(BUMP(C_ADD, mt_int(mt_arg(call, 0)))));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    /* The original's own imports: its loader needs lib_import and its guard
       lib_file, and C needs neither, but &self holds what they define on
       both sides. */
    static const char *const libraries[] = { "lib_import", "lib_file" };
    for (size_t i = 0; i < sizeof libraries / sizeof *libraries; i++)
        require(libraries[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));
    require("c-bump", mt_def(m, (mt_op){ .name = "c-bump", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = c_bump }));
    assert(answers_are(mt_eval(m, E("c-bump", 41)), E(BUMP(C_ADD, 41))) && "(c-bump 41) is C's");
    mt_close(m);
    return 0;
}
