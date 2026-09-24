/* Purpose: PLN from C. lib_pln's modus ponens combines an implication's
 *   truth value with its premise's, and the answer's (stv strength
 *   confidence) is read back as two C doubles.
 * Guarantees: strength follows the premise and confidence stays strictly
 *   between 0 and 1 [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_pln",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln")))));
    mt_atom *truth = mt_one(mt_eval(m, E("Truth_ModusPonens", E("stv", 1.0, 0.95), E("stv", 0.6, 0.9))));
    require("a truth value", truth != NULL && mt_len(truth) == 3);
    check_text("it is an stv", mt_name(mt_at(truth, 0)), "stv");
    double strength = mt_float(mt_at(truth, 1)), confidence = mt_float(mt_at(truth, 2));
    check_real("the strength is the premise's", strength, 0.6);
    check("and confidence stays uncertain", confidence > 0.0 && confidence < 1.0);
    mt_drop(truth);
    return done(m);
}
