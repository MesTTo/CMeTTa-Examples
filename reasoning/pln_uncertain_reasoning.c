/* Purpose: Read uncertain modus-ponens truth values through the C surface.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check("load PLN", mt_do(m, "!(import! &self (library lib_pln))"));
    mt_atom *result = mt_one(mt_run(m, "!(Truth_ModusPonens (stv 1.0 0.95) (stv 0.6 0.9))"));
    check("truth value structure", mt_len(result) == 3 && strcmp(mt_name(mt_at(result, 0)), "stv") == 0);
    double strength = mt_float(mt_at(result, 1)), confidence = mt_float(mt_at(result, 2));
    check("modus ponens strength", strength == 0.6);
    check("uncertainty retained", confidence > 0.0 && confidence < 1.0);
    mt_drop(result);
    return done(m, "pln_uncertain_reasoning");
}
