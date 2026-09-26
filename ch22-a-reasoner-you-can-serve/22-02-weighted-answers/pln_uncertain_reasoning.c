/* Purpose: PLN from C. lib_pln's modus ponens combines an implication's
 *   truth value with its premise's, and the answer's (stv strength
 *   confidence) is read back as two C doubles.
 * Guarantees: strength follows the premise and confidence stays strictly
 *   between 0 and 1 [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_pln",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln")))));
    mt_atom *truth = mt_one(mt_eval(m, E("Truth_ModusPonens", E("stv", 1.0, 0.95), E("stv", 0.6, 0.9))));
    require("a truth value", truth != NULL && mt_len(truth) == 3);
    assert(strcmp(mt_name(mt_at(truth, 0)), "stv") == 0 && "it is an stv");
    double strength = mt_float(mt_at(truth, 1)), confidence = mt_float(mt_at(truth, 2));
    assert(strength == 0.6 && "the strength is the premise's");
    assert(confidence > 0.0 && confidence < 1.0 && "and confidence stays uncertain");
    mt_drop(truth);
    mt_close(m);
    return 0;
}
