/* Purpose: a declared type can hold its argument. OpaquePayload is a
 *   DontEvalType, so inspect-opaque's argument reaches the equation as
 *   written, and the metatype it answers is the written term's, the one
 *   metatype() gives the expression C built.
 * Guarantees: the original's one claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("(: OpaquePayload DontEvalType)", mt_add(m, E(":", "OpaquePayload", "DontEvalType")));
    require("(: inspect-opaque (-> OpaquePayload Symbol))", mt_add(m, E(":", "inspect-opaque", E("->", "OpaquePayload", "Symbol"))));
    require("(= (inspect-opaque $written) (get-metatype $written))",
            mt_add(m, E("=", E("inspect-opaque", V("written")), E("get-metatype", V("written")))));
    mt_atom *written = E("+", 1, 2);
    check_answers("the argument arrives unevaluated", mt_eval(m, E("inspect-opaque", mt_keep(written))), S(metatype(written)));
    mt_drop(written);
    return done(m);
}
