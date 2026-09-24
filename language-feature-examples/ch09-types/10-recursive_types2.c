/* Purpose: a recursive type and a recursive function over it. Nat is Z or S
 *   of a Nat, and Greater's three equations peel one S from each side until
 *   one runs out; C reads a Nat as the number of S around its Z and compares
 *   those.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* The number a Nat counts: the S wrapped around its Z. */
static int64_t counted(const mt_atom *nat)
{
    int64_t n = 0;
    for (; mt_kind_of(nat) == MT_EXPR; nat = mt_at(nat, 1)) n++;
    return n;
}

static mt_atom *nat(int64_t n) { return n ? E("S", nat(n - 1)) : S("Z"); }

int main(void)
{
    metta *m = open_engine();
    require("(: Z Nat)", mt_add(m, E(":", "Z", "Nat")));
    require("(: S (-> Nat Nat))", mt_add(m, E(":", "S", E("->", "Nat", "Nat"))));
    require("(: Greater (-> Nat Nat Bool))", mt_add(m, E(":", "Greater", E("->", "Nat", "Nat", "Bool"))));
    require("(= (Greater (S $x) Z) True)", mt_add(m, E("=", E("Greater", E("S", V("x")), "Z"), B(true))));
    require("(= (Greater Z $x) False)", mt_add(m, E("=", E("Greater", "Z", V("x")), B(false))));
    require("(= (Greater (S $x) (S $y)) (Greater $x $y))", mt_add(m, E("=", E("Greater", E("S", V("x")), E("S", V("y"))), E("Greater", V("x"), V("y")))));
    const int64_t pairs[][2] = { { 1, 1 }, { 2, 1 } };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *a = nat(pairs[i][0]), *b = nat(pairs[i][1]);
        check_answers(i ? "two is greater than one" : "one is not greater than one", mt_eval(m, E("Greater", mt_keep(a), mt_keep(b))),
                      B(counted(a) > counted(b)));
        mt_drop(a), mt_drop(b);
    }
    return done(m);
}
