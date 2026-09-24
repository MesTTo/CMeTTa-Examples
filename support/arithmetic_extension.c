/* Purpose: a plugin, compiled on its own against cmetta.h alone, that
 *   registers plugin-triple when mt_extension() calls its mt_extension_init().
 * Owns resources: the runtime keeps this shared object loaded while its
 *   registered function can be called.
 * Guarantees: integration/shared_extension.c calls what it publishes
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#include <cmetta.h>

/* (plugin-triple x) asks the engine for (* 3 x): a plugin may call back in. */
static mt_status triple(mt_call *call, void *user)
{
    (void)user;
    mt_atom *product = mt_one(mt_eval(mt_of(call), mt_expr("*", 3, mt_keep(mt_arg(call, 0)))));
    return product ? mt_answer(call, product) : mt_error();
}

bool mt_extension_init(metta *m)
{
    return mt_def(m, (mt_op){ .name = "plugin-triple", .arity = 1,
                              .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = triple });
}
