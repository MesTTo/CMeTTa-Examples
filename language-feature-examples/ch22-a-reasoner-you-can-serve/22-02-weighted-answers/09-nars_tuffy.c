/* Purpose: the smokers-and-friends network asked of NARS: does Edward get
 *   cancer? The knowledge base is C's table of sentences, each term built
 *   from a small vocabulary, friends as a product related by friend,
 *   properties as instance sets, rules as implications over variables, and
 *   added as the kb equation it is. The answer is held to the proof its
 *   stamp (2 5 6 9 10) names, each step one of nars_truth.h's functions:
 *   Anna and Edward are both friends of Frank, so abduction guesses Edward
 *   is like Anna; Anna smokes, so deduction gives Edward smoking; Edward is
 *   also known to smoke, and revision adds the two; and rule 2 carries
 *   smoking to cancer by deduction. Each premise is found in C's table by
 *   its term, and each step merges its premises' stamps as derive.h's loop
 *   does.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "nars_truth.h"
#include "derive.h"

/* The vocabulary. TAKES its atom arguments. */
static mt_atom *friends(mt_atom *a, mt_atom *b) { return E("-->", E("×", a, b), "friend"); }
static mt_atom *has(mt_atom *x, const char *property) { return E("-->", x, E("[]", property)); }
static mt_atom *implies(mt_atom *p, mt_atom *q) { return E("==>", p, q); }

/* A sentence the knowledge base holds, as C keeps it. */
typedef struct belief {
    int64_t id;
    mt_atom *term;
    truth tv;
} belief;

static const belief *find(const belief *kb, size_t n, mt_atom *term /* TAKEN */)
{
    const belief *found = NULL;
    for (size_t i = 0; i < n && !found; i++)
        if (mt_eq(kb[i].term, term)) found = &kb[i];
    mt_drop(term);
    require("a premise the knowledge base holds", found != NULL);
    return found;
}

/* One step of a proof: its truth, and its premises' stamps merged as the
   loop merges them. */
typedef struct derived {
    truth tv;
    mt_atom *stamp;
} derived;

static derived premise(const belief *s) { return (derived){ s->tv, E(s->id) }; }

/* TAKES both premises' stamps. */
static derived step(truth tv, derived a, derived b)
{
    derived d = { tv, stamps_merged(a.stamp, b.stamp) };
    mt_drop(a.stamp), mt_drop(b.stamp);
    return d;
}

int main(void)
{
    metta *m = open_engine();
    require("lib_nars", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_nars")))));

    const truth given = { 1.0, 0.9 };
    belief kb[] = {
        { 1, implies(friends(V("1"), V("2")), implies(has(V("1"), "smokes"), has(V("2"), "smokes"))), { 0.4, 0.9 } },
        { 2, implies(has(V("1"), "smokes"), has(V("1"), "cancerous")), { 0.6, 0.9 } },
        { 3, friends(S("Anna"), S("Bob")), given },
        { 4, friends(S("Anna"), S("Edward")), given },
        { 5, friends(S("Anna"), S("Frank")), given },
        { 6, friends(S("Edward"), S("Frank")), given },
        { 7, friends(S("Gary"), S("Helen")), given },
        { 8, friends(S("Gary"), S("Frank")), { 0.0, 0.9 } },
        { 9, has(S("Anna"), "smokes"), given },
        { 10, has(S("Edward"), "smokes"), given },
    };
    enum { KB = sizeof kb / sizeof *kb };
    mt_atom *sentences[KB];
    for (size_t i = 0; i < KB; i++) sentences[i] = E("Sentence", E(mt_keep(kb[i].term), stv(kb[i].tv)), E(kb[i].id));
    require("the knowledge base", mt_add(m, E("=", E("kb"), mt_exprv(KB, sentences))));

    /* The proof: abduction, deduction, revision, deduction. */
    const belief *anna_frank = find(kb, KB, friends(S("Anna"), S("Frank"))), *edward_frank = find(kb, KB, friends(S("Edward"), S("Frank"))),
                   *anna_smokes = find(kb, KB, has(S("Anna"), "smokes")), *edward_smokes = find(kb, KB, has(S("Edward"), "smokes")),
                   *rule = find(kb, KB, implies(has(V("1"), "smokes"), has(V("1"), "cancerous")));
    derived like_anna = step(abduction(anna_frank->tv, edward_frank->tv), premise(anna_frank), premise(edward_frank));
    derived smokes_once = step(deduction(like_anna.tv, anna_smokes->tv), like_anna, premise(anna_smokes));
    derived smokes = step(revision(smokes_once.tv, edward_smokes->tv), smokes_once, premise(edward_smokes));
    derived cancer = step(deduction(smokes.tv, rule->tv), smokes, premise(rule));

    check_answers("Edward's cancer, by the proof its stamp names",
                  mt_eval(m, E("NARS.Query", E("kb"), has(S("Edward"), "cancerous"))), E(stv(cancer.tv), cancer.stamp));
    for (size_t i = 0; i < KB; i++) mt_drop(kb[i].term);
    return done(m);
}
