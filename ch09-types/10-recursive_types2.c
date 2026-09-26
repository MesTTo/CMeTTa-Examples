/* Purpose: a recursive type and a recursive function over it. Nat is Z or S
 *   of a Nat, and Greater's three equations peel one S from each side until
 *   one runs out; C reads a Nat as the number of S around its Z and compares
 *   those.
 * Guarantees: both claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: Z Nat)", mt_add(m, E(":", "Z", "Nat")));
    require("(: S (-> Nat Nat))", mt_add(m, E(":", "S", E("->", "Nat", "Nat"))));
    require("(: Greater (-> Nat Nat Bool))", mt_add(m, E(":", "Greater", E("->", "Nat", "Nat", "Bool"))));
    require("(= (Greater (S $x) Z) True)", mt_add(m, E("=", E("Greater", E("S", V("x")), "Z"), B(true))));
    require("(= (Greater Z $x) False)", mt_add(m, E("=", E("Greater", "Z", V("x")), B(false))));
    require("(= (Greater (S $x) (S $y)) (Greater $x $y))", mt_add(m, E("=", E("Greater", E("S", V("x")), E("S", V("y"))), E("Greater", V("x"), V("y")))));
    const int64_t pairs[][2] = { { 1, 1 }, { 2, 1 } };
    for (size_t i = 0; i < 2; i++) {
        mt_atom *a = nat(pairs[i][0]), *b = nat(pairs[i][1]);
        assert(answers_are(mt_eval(m, E("Greater", mt_keep(a), mt_keep(b))), E(B(counted(a) > counted(b))))
               && (i ? "two is greater than one" : "one is not greater than one"));
        mt_drop(a), mt_drop(b);
    }
    mt_close(m);
    return 0;
}
