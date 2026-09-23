/* Purpose: the fence, and what its refusal says. One name as both a gap and
 *   a term has no finite reading outside an equation head, so the ask
 *   refuses; a caught refusal is an Error atom C holds, whose payload is a
 *   Prolog compound, a handle to C, which the engine reads apart with
 *   index-atom as the original does. The refusal travels with the pattern,
 *   so an arm nothing reaches cannot stop a program; the remedy is two
 *   names; an equation head admits the mix; and a two-sided pair that fits
 *   no fragment refuses with no_certificate.
 * Guarantees: all twelve claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "segments.h"

static mt_atom *mixed(void) { return E("Order", seg("m"), V("m")); }

/* The caught refusal of an ask, an Error atom. */
static mt_atom *refusal_of(metta *m, mt_atom *ask)
{
    return mt_one(mt_eval(m, E("catch", ask)));
}

/* Part `i` of the refusal's payload, which is its second child. */
static mt_answers *part(metta *m, const mt_atom *refusal, int64_t i)
{
    mt_atom *payload = E("index-atom", mt_keep(refusal), 1);
    return mt_eval(m, i == 0 ? E("car-atom", payload) : E("index-atom", payload, i));
}

int main(void)
{
    metta *m = open_engine();
    mt_atom *seven = E("Order", 7, "x", "y");
    require("(Order 7 x y)", mt_add(m, mt_keep(seven)));

    check_answers("one name in both roles refuses",
                  mt_eval(m, E("if-error", E("catch", E("match", "&self", mixed(), "hit")), "refused", "answered")), "refused");
    mt_atom *why = refusal_of(m, E("match", "&self", mixed(), "hit"));
    require("the refusal is caught", why != NULL);
    check("its payload is an engine term C holds as a handle", mt_kind_of(mt_at(why, 1)) == MT_HANDLE);
    check_answers("naming its kind", part(m, why, 0), "metta_seq_outside_fragment");
    check_answers("its rule", part(m, why, 4), "mixed_roles");
    check_answers("the pattern", part(m, why, 1), mixed());
    check_answers("and the names in both roles", part(m, why, 3), E(V("m")));
    mt_drop(why);

    check_answers("an arm nothing reaches does not refuse",
                  mt_eval(m, E("case", E("Order", 8), E(E(E("Order", 8), "plain"), E(mixed(), "never"), E(V("_"), "other")))),
                  "plain");
    check_answers("two names are the remedy",
                  mt_eval(m, E("match", "&self", E("Order", seg("run"), V("last")), E("pair", V("run"), V("last")))),
                  E("pair", run(seven, 1, 3), "y"));
    check_answers("compared where comparison belongs",
                  mt_eval(m, E("let", E("Order", seg("run"), V("last")), E("Order", E("x", "y"), E("x", "y")),
                               E("if", E("==", V("run"), E(V("last"))), "same", "different"))), "same");

    require("echoes", mt_add(m, E("=", E("echoes", E(seg("xs"), "tag", V("xs"))), "yes")));
    check_answers("an equation head admits the mix", mt_eval(m, E("echoes", E("a", "b", "tag", E("a", "b")))), "yes");

    mt_atom *nested = E("unify", E("f", E("g", seg("u"), "b")), E("f", E("g", "a", seg("v"))), "yes", "no");
    check_answers("gaps below the root, not last, refuse",
                  mt_eval(m, E("if-error", E("catch", mt_keep(nested)), "refused", "answered")), "refused");
    mt_atom *uncertified = refusal_of(m, nested);
    require("the second refusal is caught", uncertified != NULL);
    check_answers("for want of a certificate", part(m, uncertified, 4), "no_certificate");
    mt_drop(uncertified);
    mt_atom *commuting = refusal_of(m, E("unify", E("f", seg("x"), "a"), E("f", "a", seg("x")), "yes", "no"));
    require("the witness's refusal is caught", commuting != NULL);
    check_answers("so does Kutsia's own witness", part(m, commuting, 4), "no_certificate");
    check_answers("with no name in both roles", part(m, commuting, 3), mt_unit());
    mt_drop(commuting);
    mt_drop(seven);
    return done(m);
}
