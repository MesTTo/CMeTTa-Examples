/* Purpose: exact posterior marginals for independent Bernoulli candidates
 *   conditioned on one additive observation, held to C's model of them: the
 *   exhaustive definition lib_statistics' own tests compare against, every
 *   subset of the candidates weighed as the product of its members' priors
 *   and its non-members' complements, the observation's mass the sum over
 *   the subsets whose losses make the target, and each marginal the share of
 *   that mass in the subsets holding the candidate, all as reduced ratios
 *   [source: lib/lib_statistics/lib.metta, weighted-subset-mass-independent
 *   and its tested guarantee test_weighted_subset_matches_exhaustive;
 *   commit=8d651070dedaa190e25cc388c029172a63e967be]. The candidates are C's
 *   tables, each ID an atom C builds, an integer, a float, a runnable
 *   expression and an Error among them, so identity is structural and
 *   nothing is run. C finds the library's own equation with mt_match and the
 *   engine turns it into a function; C matches the space's events and makes
 *   them candidate rows itself.
 * Guarantees: all eleven claims of the original hold
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

typedef struct ratio {
    int64_t n, d;
} ratio;

static int64_t gcd(int64_t a, int64_t b)
{
    while (b) {
        int64_t r = a % b;
        a = b, b = r;
    }
    return a < 0 ? -a : a;
}

static ratio reduced(int64_t n, int64_t d)
{
    int64_t g = gcd(n, d);
    return g ? (ratio){ n / g, d / g } : (ratio){ 0, 1 };
}
static ratio times(ratio a, ratio b) { return reduced(a.n * b.n, a.d * b.d); }
static ratio plus(ratio a, ratio b) { return reduced(a.n * b.d + b.n * a.d, a.d * b.d); }
static ratio over(ratio a, ratio b) { return reduced(a.n * b.d, a.d * b.n); }
static ratio complement(ratio p) { return reduced(p.d - p.n, p.d); }
static mt_atom *ratio_atom(ratio r) { return E("ratio", r.n, r.d); }

typedef struct candidate {
    mt_atom *id;
    int64_t loss;
    ratio prior;
} candidate;

/* The observation's mass, and in HOLDING, where given, each candidate's
   share of it. Time: 2^n subsets, each n multiplications, n the candidates,
   whose subsets a 64-bit mask enumerates. */
static ratio mass_of(const candidate *c, size_t n, int64_t target, ratio *holding)
{
    require("few enough candidates to enumerate their subsets", n < 64);
    ratio mass = { 0, 1 };
    for (size_t i = 0; holding && i < n; i++) holding[i] = (ratio){ 0, 1 };
    for (uint64_t subset = 0; subset < (UINT64_C(1) << n); subset++) {
        int64_t sum = 0;
        ratio weight = { 1, 1 };
        for (size_t i = 0; i < n; i++) {
            bool in = subset >> i & 1;
            sum += in ? c[i].loss : 0;
            weight = times(weight, in ? c[i].prior : complement(c[i].prior));
        }
        if (sum != target) continue;
        mass = plus(mass, weight);
        for (size_t i = 0; holding && i < n; i++)
            if (subset >> i & 1) holding[i] = plus(holding[i], weight);
    }
    return mass;
}

/* ((candidate ID LOSS (ratio N D)) ...) */
static mt_atom *rows(const candidate *c, size_t n)
{
    mt_list list = { NULL, 0 };
    for (size_t i = 0; i < n; i++) {
        mt_atom **grown = mt_resize(list.items, (list.len + 1) * sizeof *grown);
        require("room", grown != NULL);
        list.items = grown;
        list.items[list.len++] = E("candidate", mt_keep(c[i].id), c[i].loss, ratio_atom(c[i].prior));
    }
    mt_atom *atom = mt_exprv(list.len, list.items);
    mt_free(list.items);
    return atom;
}

static mt_atom *mass_answer(const candidate *c, size_t n, int64_t target) { return ratio_atom(mass_of(c, n, target, NULL)); }

/* (subset-posterior MASS ((candidate-posterior ID MARGINAL) ...)). */
static mt_atom *posterior_answer(const candidate *c, size_t n, int64_t target)
{
    ratio *holding = malloc((n ? n : 1) * sizeof *holding);
    require("room", holding != NULL);
    ratio mass = mass_of(c, n, target, holding);
    require("an observation with mass to condition on", mass.n > 0);
    mt_list marginals = { NULL, 0 };
    for (size_t i = 0; i < n; i++) {
        mt_atom **grown = mt_resize(marginals.items, (marginals.len + 1) * sizeof *grown);
        require("room", grown != NULL);
        marginals.items = grown;
        marginals.items[marginals.len++] = E("candidate-posterior", mt_keep(c[i].id), ratio_atom(over(holding[i], mass)));
    }
    mt_atom *answer = E("subset-posterior", ratio_atom(mass), mt_exprv(marginals.len, marginals.items));
    mt_free(marginals.items), free(holding);
    return answer;
}

static void drop_ids(candidate *c, size_t n)
{
    for (size_t i = 0; i < n; i++) mt_drop(c[i].id);
}

#define COUNT(table) (sizeof table / sizeof *table)

static void posterior_claim(metta *m, const char *claim, candidate *c, size_t n, int64_t target)
{
    assert(answers_are(mt_eval(m, E("weighted-subset-posterior-independent", rows(c, n), target)), E(posterior_answer(c, n, target))) && claim);
    drop_ids(c, n);
}

static void mass_claim(metta *m, const char *claim, candidate *c, size_t n, int64_t target)
{
    assert(answers_are(mt_eval(m, E("weighted-subset-mass-independent", rows(c, n), target)), E(mass_answer(c, n, target))) && claim);
    drop_ids(c, n);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_statistics", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_statistics")))));

    candidate pumps[] = { { S("pump-a"), 2, { 1, 2 } }, { S("pump-b"), 3, { 1, 4 } } };
    posterior_claim(m, "only pump-a explains a loss of 2", pumps, COUNT(pumps), 2);
    candidate halves[] = { { S("left"), 1, { 1, 2 } }, { S("right"), 1, { 1, 2 } } };
    posterior_claim(m, "equal losses keep their identities", halves, COUNT(halves), 1);
    candidate flags[] = { { S("metadata-flag"), 0, { 1, 3 } }, { S("failing-part"), 2, { 1, 2 } } };
    posterior_claim(m, "a zero loss keeps its prior", flags, COUNT(flags), 2);
    candidate absent[] = { { S("absent"), 5, { 0, 1 } } };
    mass_claim(m, "an unreachable observation has no mass", absent, COUNT(absent), 5);
    candidate scaled[] = { { S("scaled-loss"), 125, { 2, 5 } } };
    mass_claim(m, "a fixed-point loss", scaled, COUNT(scaled), 125);
    posterior_claim(m, "no candidates, certain mass", NULL, 0, 0);
    candidate numbers[] = { { N(1), 0, { 1, 2 } }, { R(1.0), 0, { 1, 3 } } };
    posterior_claim(m, "1 and 1.0 are different events", numbers, COUNT(numbers), 0);
    candidate runnable[] = { { E("+", 1, 2), 0, { 1, 2 } }, { E("Error", "a", "b"), 0, { 1, 3 } } };
    posterior_claim(m, "runnable IDs are not run", runnable, COUNT(runnable), 0);

    /* Alternative rows stay separate answers, repeats included. */
    candidate laws[][1] = { { { S("a"), 1, { 1, 2 } } }, { { S("a"), 1, { 1, 2 } } }, { { S("b"), 1, { 1, 3 } } } };
    mt_atom *alternatives = E(rows(laws[0], 1), rows(laws[1], 1), rows(laws[2], 1));
    assert(answers_are(mt_eval(m, E("let", V("rows"), E("superpose", alternatives), E("weighted-subset-mass-independent", V("rows"), 1))), E(mass_answer(laws[0], 1, 1), mass_answer(laws[1], 1, 1), mass_answer(laws[2], 1, 1)))
           && "each law its own answer");
    for (size_t i = 0; i < COUNT(laws); i++) drop_ids(laws[i], 1);

    /* The library's own equation, found by C and made a function. */
    mt_atom *equation = mt_first(mt_match(m, E("=", E("weighted-subset-mass-independent", V("rows"), V("target")), V("body"))));
    require("the library's equation", equation && mt_len(equation) == 3);
    const mt_atom *head = mt_at(equation, 1);
    mt_atom *function = E("|->", E(mt_keep(mt_at(head, 1)), mt_keep(mt_at(head, 2))), mt_keep(mt_at(equation, 2)));
    candidate third[] = { { S("a"), 1, { 1, 3 } } };
    assert(answers_are(mt_eval(m, E("let", V("mass"), E("eval", E("quote", function)), E(V("mass"), E("quote", rows(third, 1)), 1))), E(mass_answer(third, 1, 1)))
           && "the equation as data, applied");
    drop_ids(third, 1), mt_drop(equation);

    /* The space's events, matched by C and made candidate rows. */
    candidate events[] = { { S("from-space"), 2, { 1, 3 } }, { S("other"), 3, { 1, 4 } } };
    for (size_t i = 0; i < COUNT(events); i++)
        require("an event", mt_add(m, E("subset-event", mt_keep(events[i].id), events[i].loss, ratio_atom(events[i].prior))));
    mt_list found = mt_all(mt_match(m, E("subset-event", V("id"), V("loss"), V("prior"))));
    mt_atom **matched = malloc((found.len ? found.len : 1) * sizeof *matched);
    require("room", matched != NULL);
    for (size_t i = 0; i < found.len; i++)
        matched[i] = E("candidate", mt_keep(mt_at(found.items[i], 1)), mt_keep(mt_at(found.items[i], 2)), mt_keep(mt_at(found.items[i], 3)));
    assert(answers_are(mt_eval(m, E("weighted-subset-mass-independent", mt_exprv(found.len, matched), 2)), E(mass_answer(events, COUNT(events), 2)))
           && "matching supplies the candidates");
    free(matched), mt_list_free(found), drop_ids(events, COUNT(events));
    mt_close(m);
    return 0;
}
