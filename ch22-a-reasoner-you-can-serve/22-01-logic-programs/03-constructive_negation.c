/* Purpose: constructive negation, held to C's own model of every relation
 *   it negates. The facts are C tables turned into equations answering
 *   True, and each rule is the term it is. C decides each answer from its
 *   tables: the birds that fly are the birds that are not penguins, the
 *   unmarried students the students nobody married, a person's entitlements
 *   the pensions the rules give, or nothing where they give none. A
 *   negation on an unbound variable leaves a constraint, and binding the
 *   variable afterwards keeps it exactly when C's own predicate says the
 *   bound value satisfies the negation: no outgoing edge, a dif that holds,
 *   no passing mark, no child in &kin, a CLP(FD) bound that fails. != asks
 *   now and binds later, so a later binding never contradicts it. A case, a
 *   superpose and the comparisons negate by what each answers, the latter by
 *   C's own comparison. An Atom-typed argument stays written on both sides.
 * Guarantees: all fifty claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_EQ(a, b) ((a) == (b))
#define T_EQ(a, b) mt_expr("==", a, b)
#define C_LT(a, b) ((a) < (b))
#define C_GT(a, b) ((a) > (b))
#define T_GT(a, b) mt_expr(">", a, b)
#define T_MUL(a, b) mt_expr("*", a, b)

typedef struct fact {
    const char *relation, *who;
} fact;

static const fact facts[] = {
    { "bird", "tweety" }, { "bird", "polly" }, { "penguin", "polly" },
    { "student", "bill" }, { "married", "joe" },
    { "invalid", "mc-tavish" }, { "over-65", "mc-tavish" }, { "over-65", "mc-donald" }, { "over-65", "mc-duff" },
    { "paid-up", "mc-tavish" }, { "paid-up", "mc-donald" },
};
#define FACTS (sizeof facts / sizeof *facts)

static bool holds(const char *relation, const char *who)
{
    for (size_t i = 0; i < FACTS; i++)
        if (strcmp(facts[i].relation, relation) == 0 && strcmp(facts[i].who, who) == 0) return true;
    return false;
}

static const char *const fallible[] = { "penguin", "bird", "student", "married", "invalid", "over-65", "paid-up", "marks", "edge" };

/* The pensions the three rules give a person, in the rules' order. */
typedef struct pension_rule {
    const char *pension, *first, *second;
} pension_rule;
static const pension_rule pensions[] = {
    { "invalid-pension", "invalid", NULL },
    { "old-age-pension", "over-65", "paid-up" },
    { "supplementary-benefit", "over-65", NULL },
};
#define PENSIONS (sizeof pensions / sizeof *pensions)

static bool gets(const pension_rule *r, const char *who)
{
    return holds(r->first, who) && (!r->second || holds(r->second, who));
}

static const char *const edges[][2] = { { "a", "b" }, { "b", "c" } };
static bool has_outgoing(const char *who)
{
    for (size_t i = 0; i < 2; i++)
        if (strcmp(edges[i][0], who) == 0) return true;
    return false;
}

static const struct { const char *who; int64_t mark; } marks[] = { { "carol", 90 }, { "carol", 30 }, { "dave", 10 }, { "dave", 20 } };
#define MARKS (sizeof marks / sizeof *marks)
enum { PASS = 50 };
static bool any_pass(const char *who)
{
    for (size_t i = 0; i < MARKS; i++)
        if (strcmp(marks[i].who, who) == 0 && C_GT(marks[i].mark, PASS)) return true;
    return false;
}

static const char *const parents[][2] = { { "alice", "bob" }, { "carol", "dave" } };
static bool has_child(const char *who)
{
    for (size_t i = 0; i < 2; i++)
        if (strcmp(parents[i][0], who) == 0) return true;
    return false;
}

static mt_answers *truly(metta *m, mt_atom *goal, mt_atom *answer) { return mt_eval(m, E("let", B(true), goal, answer)); }

static void negated(metta *m, const char *claim, mt_atom *goal, bool provable)
{
    assert(answers_are(mt_eval(m, E("not-provable", goal)), E(B(!provable))) && claim);
}

/* (let True CONSTRAINT (let $var VALUE $var)): VALUE where C's predicate
   KEEPS it, nothing where it does not. TAKES constraint and value. */
static void bound_later(metta *m, const char *claim, mt_atom *constraint, const char *var, mt_atom *value, bool keeps)
{
    mt_answers *answers = truly(m, constraint, E("let", V(var), mt_keep(value), V(var)));
    if (keeps)
        assert(answers_are(answers, E(value)) && claim);
    else {
        mt_drop(value);
        assert(!mt_first(answers) && mt_ok() && claim);
    }
}

static mt_atom *as(const char *relation, mt_atom *who) { return E(relation, who); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    for (size_t i = 0; i < sizeof fallible / sizeof *fallible; i++)
        require("fail where no equation matches", mt_add(mt_catalog(m), E("dispatch-policy", fallible[i], "NoMatchEnum",
                                                                          mt_no_match_enum_names[MT_NO_MATCH_ENUM_NO_MATCH_FAIL])));
    for (size_t i = 0; i < FACTS; i++) require("a fact", mt_add(m, E("=", E(facts[i].relation, facts[i].who), B(true))));

    assert(!mt_first(mt_eval(m, E("not", E("penguin", "tweety")))) && mt_ok() && "not cannot say it");
    static const char *const birds[] = { "tweety", "polly" };
    for (size_t i = 0; i < 2; i++)
        negated(m, "not-provable can", E("penguin", birds[i]), holds("penguin", birds[i]));
    require("flies", mt_add(m, E("=", E("flies", V("x")), E("and", E("bird", V("x")), E("not-provable", E("penguin", V("x")))))));
    mt_atom *fliers[2];
    size_t nfliers = 0;
    for (size_t i = 0; i < 2; i++)
        if (holds("bird", birds[i]) && !holds("penguin", birds[i])) fliers[nfliers++] = S(birds[i]);
    assert(answers_are(truly(m, E("flies", V("x")), V("x")), mt_exprv(nfliers, fliers)) && "birds fly unless they are penguins");

    require("unmarried-student", mt_add(m, E("=", E("unmarried-student", V("x")),
                                              E("and", E("not-provable", E("married", V("x"))), E("student", V("x"))))));
    mt_atom *unmarried[FACTS];
    size_t nunmarried = 0;
    for (size_t i = 0; i < FACTS; i++)
        if (strcmp(facts[i].relation, "student") == 0 && !holds("married", facts[i].who)) unmarried[nunmarried++] = S(facts[i].who);
    assert(answers_are(truly(m, E("unmarried-student", V("x")), V("x")), mt_exprv(nunmarried, unmarried))
           && "the negation runs first and still finds the student");
    require("two-but-not-one", mt_add(m, E("=", E("two-but-not-one", V("x")),
                                            E("and", E("not-provable", T_EQ(V("x"), 1)), E("let", V("x"), 2, B(true))))));
    mt_atom *two[1];
    size_t ntwo = 0;
    if (!C_EQ(2, 1)) two[ntwo++] = N(2);
    assert(answers_are(truly(m, E("two-but-not-one", V("x")), V("x")), mt_exprv(ntwo, two)) && "a disequality left behind admits 2");

    for (size_t i = 0; i < PENSIONS; i++) {
        mt_atom *body = pensions[i].second ? E("and", as(pensions[i].first, V("p")), as(pensions[i].second, V("p")))
                                           : as(pensions[i].first, V("p"));
        require("a pension rule", mt_add(m, E("=", E("pension", V("p"), pensions[i].pension), body)));
    }
    require("entitled to a pension", mt_add(m, E("=", E("entitlement", V("p"), V("what")), E("pension", V("p"), V("what")))));
    require("or to nothing",
            mt_add(m, E("=", E("entitlement", V("p"), "nothing"), E("not-provable", E("pension", V("p"), V("any"))))));
    static const char *const people[] = { "mc-tavish", "mc-duff", "someone-else" };
    for (size_t p = 0; p < 3; p++) {
        mt_atom *want[PENSIONS + 1];
        size_t n = 0;
        for (size_t i = 0; i < PENSIONS; i++)
            if (gets(&pensions[i], people[p])) want[n++] = S(pensions[i].pension);
        if (n == 0) want[n++] = S("nothing");
        assert(answers_are(truly(m, E("entitlement", people[p], V("w")), V("w")), mt_exprv(n, want)) && "entitlements are the pensions, or nothing");
    }

    for (size_t i = 0; i < 2; i++) require("an edge", mt_add(m, E("=", E("edge", edges[i][0], edges[i][1]), B(true))));
    require("has-no-outgoing", mt_add(m, E("=", E("has-no-outgoing", V("x")), E("not-provable", E("edge", V("x"), V("y"))))));
    static const char *const asked[] = { "c", "a" };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, E("has-no-outgoing", asked[i])), E(B(!has_outgoing(asked[i])))) && "a node with no edge out");
    static const char *const later[] = { "c", "zzz", "a" };
    for (size_t i = 0; i < 3; i++)
        bound_later(m, "the constraint decides a later binding", E("has-no-outgoing", V("x")), "x", S(later[i]),
                    !has_outgoing(later[i]));

    assert(answers_are(mt_eval(m, E("dif", 1, 2)), E(B(1 != 2))) && "dif holds of two values");
    static const int64_t difs[] = { 6, 5 };
    for (size_t i = 0; i < 2; i++)
        bound_later(m, "dif makes a later equal binding fail", E("dif", V("q"), 5), "q", N(difs[i]), difs[i] != 5);
    static const int64_t compared[][2] = { { 1, 2 }, { 1, 1 } };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, E("!=", compared[i][0], compared[i][1])), E(B(compared[i][0] != compared[i][1]))) && "!= asks now");
    bound_later(m, "and lets a later binding contradict it", E("!=", V("r"), 5), "r", N(5), true);

    for (size_t i = 0; i < MARKS; i++) require("a mark", mt_add(m, E("=", E("marks", marks[i].who), marks[i].mark)));
    require("any-pass", mt_add(m, E("=", E("any-pass", V("w")), E("let", V("m"), E("marks", V("w")), T_GT(V("m"), PASS)))));
    mt_atom *carol[MARKS];
    size_t ncarol = 0;
    for (size_t i = 0; i < MARKS; i++)
        if (strcmp(marks[i].who, "carol") == 0) carol[ncarol++] = B(C_GT(marks[i].mark, PASS));
    assert(answers_are(mt_eval(m, E("any-pass", "carol")), mt_exprv(ncarol, carol)) && "one answer per mark");
    static const char *const students[] = { "carol", "dave", "nobody" };
    for (size_t i = 0; i < 3; i++)
        negated(m, "a let is True when some mark passes", E("any-pass", students[i]), any_pass(students[i]));
    static const char *const failing[] = { "dave", "carol", "erin" };
    for (size_t i = 0; i < 3; i++)
        bound_later(m, "the negation narrows who fails", E("not-provable", E("any-pass", V("w"))), "w", S(failing[i]),
                    !any_pass(failing[i]));

    mt_space *kin = mt_space_open(m, "&kin");
    require("open &kin", kin != NULL);
    for (size_t i = 0; i < 2; i++) require("a parent", mt_add(kin, E("parent", parents[i][0], parents[i][1])));
    require("has-child", mt_add(m, E("=", E("has-child", V("x")), E("match", mt_spaceref("&kin"), E("parent", V("x"), V("y")), B(true)))));
    static const char *const family[] = { "alice", "bob", "stranger" };
    for (size_t i = 0; i < 3; i++) negated(m, "a space query negated", E("has-child", family[i]), has_child(family[i]));
    static const char *const childless[] = { "bob", "alice", "nobody" };
    for (size_t i = 0; i < 3; i++)
        bound_later(m, "and narrowed over the space", E("not-provable", E("has-child", V("w"))), "w", S(childless[i]),
                    !has_child(childless[i]));
    mt_space_close(kin);

    require("band", mt_add(m, E("=", E("band", V("n")), E("case", V("n"), E(E(90, B(true)), E(40, B(false)))))));
    static const int64_t bands[] = { 90, 40, 55 };
    for (size_t i = 0; i < 3; i++) negated(m, "a case negates arm by arm", E("band", bands[i]), bands[i] == 90);
    static const struct { bool items[2]; size_t n; } superposed[] = { { { false, true }, 2 }, { { false, false }, 2 }, { { false, false }, 0 } };
    for (size_t i = 0; i < 3; i++) {
        mt_atom *items[2];
        bool any = false;
        for (size_t j = 0; j < superposed[i].n; j++) items[j] = B(superposed[i].items[j]), any = any || superposed[i].items[j];
        negated(m, "a superpose is not True when none is", E("superpose", mt_exprv(superposed[i].n, items)), any);
    }
    negated(m, "comparisons negate to comparisons", T_GT(1, 2), C_GT(1, 2));
    negated(m, "both ways", T_GT(2, 1), C_GT(2, 1));
    negated(m, "and equality", T_EQ(1, 1), C_EQ(1, 1));

    negated(m, "a CLP(FD) bound negates", E("#<", 5, 1), C_LT(5, 1));
    static const int64_t below[] = { 7, 3 };
    for (size_t i = 0; i < 2; i++)
        bound_later(m, "and narrows a domain", E("not-provable", E("#<", V("x"), 5)), "x", N(below[i]), !C_LT(below[i], 5));
    static const int64_t equal[] = { 9, 4 };
    for (size_t i = 0; i < 2; i++)
        bound_later(m, "an equality negated", E("not-provable", E("#=", V("y"), 4)), "y", N(equal[i]), !C_EQ(equal[i], 4));

    require("(: mask-example-double (-> Number Number))", mt_add(m, E(":", "mask-example-double", E("->", "Number", "Number"))));
    require("mask-example-double", mt_add(m, E("=", E("mask-example-double", V("x")), T_MUL(V("x"), 2))));
    require("(: mask-example-holds (-> Atom Bool))", mt_add(m, E(":", "mask-example-holds", E("->", "Atom", "Bool"))));
    require("mask-example-holds", mt_add(m, E("=", E("mask-example-holds", 10), B(true))));
    require("it fails where it does not hold", mt_add(mt_catalog(m), E("dispatch-policy", "mask-example-holds", "NoMatchEnum",
                                                                       mt_no_match_enum_names[MT_NO_MATCH_ENUM_NO_MATCH_FAIL])));
    negated(m, "an Atom argument stays written", E("mask-example-holds", E("mask-example-double", 5)), false);
    negated(m, "and a written 10 holds", E("mask-example-holds", 10), true);
    assert(!mt_first(mt_eval(m, E("mask-example-holds", E("mask-example-double", 5)))) && mt_ok() && "the written call is not 10");
    mt_close(m);
    return 0;
}
