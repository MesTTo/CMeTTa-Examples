/* Purpose: preserve an Atom argument while reducing an ordinary value argument.
 * Owns resources: callback arguments are borrowed; answers retain their atoms.
 * Guarantees: declared arrow types control evaluation at the C boundary
 *   [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
static mt_status identity(mt_call *call, void *user)
{ (void)user; return mt_answer(call, mt_keep(mt_arg(call, 0))); }
int main(void)
{
    metta *m = open_engine();
    check("publish atom function", mt_def(m, (mt_op){.name="written-term", .arity=1, .effect=MT_PURE, .fn=identity}));
    check("publish value function", mt_def(m, (mt_op){.name="reduced-value", .arity=1, .effect=MT_PURE, .fn=identity}));
    check("declare argument contracts", mt_do(m,
        "(: written-term (-> Atom Atom)) (: reduced-value (-> Number Number))"));
    check_answers("Atom stays written", mt_run(m, "!(written-term (+ 20 22))"), "(+ 20 22)");
    check_answers("Number is reduced", mt_run(m, "!(reduced-value (+ 20 22))"), "42");
    check("withdraw atom function", mt_undef(m, "written-term"));
    check("withdraw value function", mt_undef(m, "reduced-value"));
    return done(m, "annotation_contracts");
}
