/* Purpose: a dependently typed backward chainer searching proofs over
 *   Metamath's demo0, at three levels of difficulty. The chainer's five
 *   recursive clauses are one shape for one to five premises, so C writes
 *   chain_clause(n) once and adds it for each n: recurse on the rule's
 *   abstraction, recurse on each premise, answer the fulfilled query. The
 *   Metamath vocabulary is a handful of C builders, so each axiom and each
 *   expected proof reads as the logic it is. The three knowledge bases are
 *   spaces C opens and fills; each claim asks the chainer for a proof at a
 *   depth and holds the proof it built to the one C builds, the medium level
 *   by membership among every proof found.
 * Guarantees: all nine claims of the original hold
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

/* ---- the Metamath vocabulary ---- */
static mt_atom *eq(mt_atom *l, mt_atom *r) { return E("⟨=⟩", l, r); }
static mt_atom *plus(mt_atom *l, mt_atom *r) { return E("⟨+⟩", l, r); }
static mt_atom *implies(mt_atom *p, mt_atom *q) { return E("⟨->⟩", p, q); }
static mt_atom *typed(mt_atom *x, mt_atom *type) { return E(":", x, type); }
static mt_atom *t(void) { return S("⟨t⟩"); }
static mt_atom *zero(void) { return S("⟨0⟩"); }
static mt_atom *term(void) { return S("⟨term⟩"); }
static mt_atom *wff(void) { return S("⟨wff⟩"); }
static mt_atom *t_plus_0(void) { return plus(t(), zero()); }

/* ---- the chainer ---- */

/* The names the original gives the n-premise clause's variables: the
   one-premise clause writes them without a digit. */
static mt_atom *premise(const char *what, size_t i, size_t n)
{
    char name[32];
    if (n == 1)
        snprintf(name, sizeof name, "premise_%s", what);
    else
        snprintf(name, sizeof name, "premise_%s%zu", what, i + 1);
    return V(name);
}

static mt_atom *kb(void) { return V("knowledge_base"); }

/* (bc $kb $depth <query>) for one binding of the clause's let*. */
static mt_atom *step(mt_atom *query) { return E("bc", kb(), V("depth"), query); }

/* The n-premise clause: recurse on the abstraction, then on each premise,
   and answer the fulfilled query. */
static mt_atom *chain_clause(size_t n)
{
    mt_atom **arrow = malloc((n + 2) * sizeof *arrow), **bindings = malloc((n + 1) * sizeof *bindings),
            **applied = malloc((n + 1) * sizeof *applied);
    require("room", arrow && bindings && applied);
    arrow[0] = S("->");
    applied[0] = V("proof_rule");
    for (size_t i = 0; i < n; i++) {
        arrow[i + 1] = typed(premise("proof", i, n), premise("type", i, n));
        applied[i + 1] = premise("proof", i, n);
    }
    arrow[n + 1] = V("theorem");
    mt_atom *abstraction = typed(V("proof_rule"), mt_exprv(n + 2, arrow));
    bindings[0] = E(mt_keep(abstraction), step(mt_keep(abstraction)));
    mt_drop(abstraction);
    for (size_t i = 0; i < n; i++) {
        mt_atom *each = typed(premise("proof", i, n), premise("type", i, n));
        bindings[i + 1] = E(mt_keep(each), step(each));
    }
    mt_atom *goal = typed(mt_exprv(n + 1, applied), V("theorem"));
    mt_atom *clause = E("=", E("bc", kb(), E("S", V("depth")), mt_keep(goal)), E("let*", mt_exprv(n + 1, bindings), goal));
    free(arrow), free(bindings), free(applied);
    return clause;
}

enum { MOST_PREMISES = 5 };

/* (bc &space (fromNumber depth) query). TAKES query. */
static mt_answers *proves(metta *m, const char *space, int64_t depth, mt_atom *query)
{
    return mt_eval(m, E("bc", mt_spaceref(space), E("fromNumber", depth), query));
}

static void axioms(mt_space *space, mt_atom **rows, size_t n)
{
    for (size_t i = 0; i < n; i++) require("an axiom", mt_add(space, rows[i]));
}
#define AXIOMS(space, ...) axioms((space), (mt_atom *[]){ __VA_ARGS__ }, MT_NARG(__VA_ARGS__))

static mt_atom *arrow2(mt_atom *a, mt_atom *b, mt_atom *c) { return E("->", a, b, c); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: Nat Type)", mt_add(m, typed(S("Nat"), S("Type"))));
    require("(: Z Nat)", mt_add(m, typed(S("Z"), S("Nat"))));
    require("(: S (-> Nat Nat))", mt_add(m, typed(S("S"), E("->", "Nat", "Nat"))));
    require("fromNumber's type", mt_add(m, typed(S("fromNumber"), E("->", "Number", "Nat"))));
    require("fromNumber", mt_add(m, E("=", E("fromNumber", V("n")),
                                       E("if", E("<=", V("n"), 0), "Z", E("S", E("fromNumber", E("-", V("n"), 1)))))));
    require("fromNat's type", mt_add(m, typed(S("fromNat"), E("->", "Nat", "Number"))));
    require("fromNat of Z", mt_add(m, E("=", E("fromNat", "Z"), 0)));
    require("fromNat of S", mt_add(m, E("=", E("fromNat", E("S", V("k"))), E("+", 1, E("fromNat", V("k"))))));
    require("bc's type", mt_add(m, typed(S("bc"), E("->", V("a"), "Nat", V("b"), V("b")))));
    require("the base clause",
            mt_add(m, E("=", E("bc", kb(), V("_"), V("query")),
                        E("let", typed(V("proof"), V("theorem")), V("query"),
                          E("match", kb(), typed(V("proof"), V("theorem")), typed(V("proof"), V("theorem")))))));
    for (size_t n = 1; n <= MOST_PREMISES; n++) require("a recursive clause", mt_add(m, chain_clause(n)));

    mt_space *easy = mt_space_open(m, "&kbe"), *medium = mt_space_open(m, "&kbm"), *hard = mt_space_open(m, "&kbh");
    require("three knowledge bases", easy && medium && hard);

    AXIOMS(easy, typed(S("a1"), arrow2(typed(V("ter"), eq(V("t"), V("r"))), typed(V("tes"), eq(V("t"), V("s"))), eq(V("r"), V("s")))),
           typed(S("a2"), eq(plus(V("t"), zero()), V("t"))));
    assert(answers_are(proves(m, "&kbe", 1, typed(V("prf"), eq(V("t"), V("t")))), E(typed(E("a1", "a2", "a2"), eq(V("t"), V("t")))))
           && "easy: a1 over two a2s");

    mt_atom *terms_and_wffs[] = {
        typed(zero(), term()),
        typed(S("⟨+⟩"), arrow2(typed(V("t"), term()), typed(V("r"), term()), term())),
        typed(S("⟨=⟩"), arrow2(typed(V("t"), term()), typed(V("r"), term()), wff())),
    };
    for (size_t i = 0; i < 3; i++) require("a term or wff rule", mt_add(medium, mt_keep(terms_and_wffs[i])));
    AXIOMS(medium,
           typed(S("a1"), E("->", typed(V("t"), term()), typed(V("r"), term()), typed(V("s"), term()),
                            typed(V("ter"), eq(V("t"), V("r"))), typed(V("tes"), eq(V("t"), V("s"))), eq(V("r"), V("s")))),
           typed(S("a2"), E("->", typed(V("t"), term()), eq(t_plus_0(), V("t")))), typed(t(), term()));
    mt_atom *medium_proof = typed(E("a1", t_plus_0(), t(), t(), E("a2", t()), E("a2", t())), eq(t(), t()));
    bool found = false;
    mt_each (answer, proves(m, "&kbm", 3, typed(V("prf"), eq(t(), t())))) found = found || mt_alpha_eq(answer, medium_proof);
    assert(found && "medium: the proof is among those found");
    mt_drop(medium_proof);

    for (size_t i = 0; i < 3; i++) require("a term or wff rule", mt_add(hard, terms_and_wffs[i]));
    AXIOMS(hard, typed(S("⟨->⟩"), arrow2(typed(V("P"), wff()), typed(V("Q"), wff()), wff())),
           typed(S("a1"), E("->", typed(V("t"), term()), typed(V("r"), term()), typed(V("s"), term()),
                            implies(eq(V("t"), V("r")), implies(eq(V("t"), V("s")), eq(V("r"), V("s")))))),
           typed(S("a2"), E("->", typed(V("t"), term()), eq(t_plus_0(), V("t")))),
           typed(S("mp"), E("->", typed(V("maj"), implies(V("P"), V("Q"))), typed(V("P"), wff()), typed(V("Q"), wff()),
                            typed(V("min"), V("P")), V("Q"))),
           typed(t(), term()));

    mt_atom *tt = eq(t(), t());
    assert(answers_are(proves(m, "&kbh", 1, typed(V("prf"), implies(mt_keep(tt), implies(mt_keep(tt), mt_keep(tt))))), E(typed(E("a1", t(), t(), t()), implies(mt_keep(tt), implies(mt_keep(tt), mt_keep(tt))))))
           && "hard: a1 at depth 1");
    mt_atom *zt = eq(t_plus_0(), t());
    assert(answers_are(proves(m, "&kbh", 2, typed(V("prf"), implies(mt_keep(zt), implies(mt_keep(zt), mt_keep(tt))))), E(typed(E("a1", t_plus_0(), t(), t()), implies(mt_keep(zt), implies(mt_keep(zt), mt_keep(tt))))))
           && "a1 over a sum");
    assert(answers_are(proves(m, "&kbh", 1, typed(V("prf"), mt_keep(zt))), E(typed(E("a2", t()), mt_keep(zt)))) && "a2");
    assert(answers_are(proves(m, "&kbh", 2, typed(mt_keep(zt), wff())), E(typed(mt_keep(zt), wff()))) && "a wff of a sum");
    assert(answers_are(proves(m, "&kbh", 1, typed(mt_keep(tt), wff())), E(typed(mt_keep(tt), wff()))) && "a wff");
    mt_atom *major = E("a1", t_plus_0(), t(), t());
    mt_atom *first_mp = E("mp", mt_keep(major), mt_keep(zt), implies(mt_keep(zt), mt_keep(tt)), E("a2", t()));
    assert(answers_are(proves(m, "&kbh", 4, typed(V("prf"), implies(mt_keep(zt), mt_keep(tt)))), E(typed(mt_keep(first_mp), implies(mt_keep(zt), mt_keep(tt)))))
           && "modus ponens at depth 4");
    assert(answers_are(proves(m, "&kbh", 5, typed(V("prf"), mt_keep(tt))), E(typed(E("mp", mt_keep(first_mp), mt_keep(zt), mt_keep(tt), E("a2", t())), mt_keep(tt))))
           && "and twice at depth 5");
    mt_drop(major), mt_drop(first_mp), mt_drop(zt), mt_drop(tt);
    mt_space_close(easy), mt_space_close(medium), mt_space_close(hard);
    mt_close(m);
    return 0;
}
