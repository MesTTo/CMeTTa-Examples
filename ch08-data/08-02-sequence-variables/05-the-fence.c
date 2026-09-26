/* Purpose: the fence, and what its refusal says. One name as both a gap and
 *   a term has no finite reading outside an equation head, so the ask
 *   refuses; a caught refusal is an Error atom whose payload is an engine
 *   compound, which arrives in the wire grammar as an expression, so C reads
 *   it apart by position with mt_at, as the original reads it with
 *   index-atom. The refusal travels with the pattern, so an arm nothing
 *   reaches cannot stop a program; the remedy is two names; an equation head
 *   admits the mix; and a two-sided pair that fits no fragment refuses with
 *   no_certificate.
 * Guarantees: all twelve claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/segments.h"

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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

static mt_atom *mixed(void) { return E("Order", seg("m"), V("m")); }

/* The caught refusal of an ask, an Error atom. */
static mt_atom *refusal_of(metta *m, mt_atom *ask)
{
    return mt_one(mt_eval(m, E("catch", ask)));
}

/* Part `i` of the refusal's payload, which is its second child. */
static mt_atom *part(const mt_atom *refusal, size_t i)
{
    const mt_atom *payload = mt_at(refusal, 1);
    return mt_keep(mt_at(payload, i));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *seven = E("Order", 7, "x", "y");
    require("(Order 7 x y)", mt_add(m, mt_keep(seven)));

    assert(answers_are(mt_eval(m, E("if-error", E("catch", E("match", "&self", mixed(), "hit")), "refused", "answered")), E("refused"))
           && "one name in both roles refuses");
    mt_atom *why = refusal_of(m, E("match", "&self", mixed(), "hit"));
    require("the refusal is caught", why != NULL);
    assert(mt_kind_of(mt_at(why, 1)) == MT_EXPR && "its payload is an expression");
    assert(atom_is(part(why, 0), S("metta_seq_outside_fragment")) && "naming its kind");
    assert(atom_is(part(why, 4), S("mixed_roles")) && "its rule");
    assert(atom_is(part(why, 1), mixed()) && "the pattern");
    assert(atom_is(part(why, 3), E(V("m"))) && "and the names in both roles");
    mt_drop(why);

    assert(answers_are(mt_eval(m, E("case", E("Order", 8), E(E(E("Order", 8), "plain"), E(mixed(), "never"), E(V("_"), "other")))), E("plain"))
           && "an arm nothing reaches does not refuse");
    assert(answers_are(mt_eval(m, E("match", "&self", E("Order", seg("run"), V("last")), E("pair", V("run"), V("last")))), E(E("pair", run(seven, 1, 3), "y")))
           && "two names are the remedy");
    assert(answers_are(mt_eval(m, E("let", E("Order", seg("run"), V("last")), E("Order", E("x", "y"), E("x", "y")),
                                    E("if", E("==", V("run"), E(V("last"))), "same", "different"))), E("same"))
           && "compared where comparison belongs");

    require("echoes", mt_add(m, E("=", E("echoes", E(seg("xs"), "tag", V("xs"))), "yes")));
    assert(answers_are(mt_eval(m, E("echoes", E("a", "b", "tag", E("a", "b")))), E("yes")) && "an equation head admits the mix");

    mt_atom *nested = E("unify", E("f", E("g", seg("u"), "b")), E("f", E("g", "a", seg("v"))), "yes", "no");
    assert(answers_are(mt_eval(m, E("if-error", E("catch", mt_keep(nested)), "refused", "answered")), E("refused"))
           && "gaps below the root, not last, refuse");
    mt_atom *uncertified = refusal_of(m, nested);
    require("the second refusal is caught", uncertified != NULL);
    assert(atom_is(part(uncertified, 4), S("no_certificate")) && "for want of a certificate");
    mt_drop(uncertified);
    mt_atom *commuting = refusal_of(m, E("unify", E("f", seg("x"), "a"), E("f", "a", seg("x")), "yes", "no"));
    require("the witness's refusal is caught", commuting != NULL);
    assert(atom_is(part(commuting, 4), S("no_certificate")) && "so does Kutsia's own witness");
    assert(atom_is(part(commuting, 3), mt_unit()) && "with no name in both roles");
    mt_drop(commuting);
    mt_drop(seven);
    mt_close(m);
    return 0;
}
