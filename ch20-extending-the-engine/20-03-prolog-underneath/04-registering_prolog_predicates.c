/* Purpose: registration, from the two engine operations underneath it.
 *   check_prolog_function_names asks whether a list of names may be
 *   registered before its source loads, and import_prolog_functions
 *   registers every name or none. C holds each refusal's ball to the one it
 *   builds: a builtin's name refused for registration, alone or anywhere in
 *   a list, and a name no predicate stands behind. Once registered, the
 *   fixture's predicates answer as C's table of them says. The Prolog
 *   database is C's own model of the rung_fact clauses: assertz appends,
 *   asserta puts a clause first, retract takes the first that unifies and
 *   answers whether one did, and callPredicate answers every clause in
 *   order. A library path is mt_library, the C seat's door onto
 *   register_metta_library_path: registering one twice holds twice, the
 *   aliased file's function answers as C's model of it, and a directory that
 *   is not there is refused naming it.
 * Guarantees: all nineteen claims of the original hold
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

static const char *const fixtures = "./examples/ch20-extending-the-engine/20-03-prolog-underneath/_fixtures";

/* What the fixture's functions answer, as C models them. */
enum { DOUBLED = 2, ALIASED_OFFSET = 100 };
static const char *const greeting = "world";

/* The answer (catch goal) gave, which must be (Error ball context), and its
   ball held to WANT. TAKES goal and want. */
static void refused(metta *m, const char *claim, mt_atom *goal, mt_atom *want)
{
    mt_atom *reported = mt_first(mt_eval(m, E("catch", goal)));
    bool shaped = reported && mt_kind_of(reported) == MT_EXPR && mt_len(reported) == 3 &&
                  mt_kind_of(mt_at(reported, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(reported, 0)), "Error") == 0;
    require("the refusal is an Error", shaped);
    assert(atom_is(mt_keep(mt_at(reported, 1)), want) && claim);
    mt_drop(reported);
}

/* C's model of the rung_fact clauses, first to last. */
typedef struct clauses {
    const char **fact;
    size_t n, cap;
} clauses;

static void insert(clauses *c, size_t at, const char *fact)
{
    if (c->n == c->cap) {
        size_t cap = c->cap ? 2 * c->cap : 4;
        const char **grown = realloc(c->fact, cap * sizeof *grown);
        require("room for a clause", grown != NULL);
        c->fact = grown;
        c->cap = cap;
    }
    memmove(c->fact + at + 1, c->fact + at, (c->n - at) * sizeof *c->fact);
    c->fact[at] = fact;
    c->n++;
}

static bool retracted(clauses *c, const char *fact)
{
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->fact[i], fact) == 0) {
            memmove(c->fact + i, c->fact + i + 1, (c->n - i - 1) * sizeof *c->fact);
            c->n--;
            return true;
        }
    return false;
}

static mt_atom *predicate(const char *fact) { return E("Predicate", E("rung_fact", fact)); }

/* Every rung_fact clause, through callPredicate, against C's model. */
static void holds(metta *m, const char *claim, const clauses *c)
{
    mt_atom **want = malloc((c->n ? c->n : 1) * sizeof *want);
    require("room", want != NULL);
    for (size_t i = 0; i < c->n; i++) want[i] = S(c->fact[i]);
    assert(answers_are(mt_eval(m, E("let", V("t"), E("callPredicate", E("Predicate", E("rung_fact", V("x")))), V("x"))), mt_exprv(c->n, want))
           && claim);
    free(want);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_import", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_import")))));
    require("(: Predicate (-> Expression %Undefined%))", mt_add(m, E(":", "Predicate", E("->", "Expression", "%Undefined%"))));

    assert(answers_are(mt_eval(m, E("check_prolog_function_names", E("noeval", E("rung_fresh_one", "rung_fresh_two")), T("example.pl"))), E(B(true)))
           && "fresh names may be registered");
    refused(m, "a builtin's name is refused before the source loads",
            E("check_prolog_function_names", E("noeval", E("car-atom")), T("example.pl")),
            E("permission_error", "register", "metta_builtin", "car-atom"));
    refused(m, "anywhere in the list",
            E("check_prolog_function_names", E("noeval", E("rung_fresh_one", "car-atom")), T("example.pl")),
            E("permission_error", "register", "metta_builtin", "car-atom"));
    refused(m, "and a name no predicate stands behind",
            E("import_prolog_functions", E("noeval", E("rung_absent_predicate"))),
            E("existence_error", "procedure", "rung_absent_predicate"));

    char functions[256];
    snprintf(functions, sizeof functions, "%s/rung_functions.pl", fixtures);
    require("consult the fixture", mt_one_truth(mt_eval(m, E("import_prolog_functions_from_file", T(functions), mt_exprv(0, NULL)))));
    assert(answers_are(mt_eval(m, E("import_prolog_functions", E("rung_double", "rung_greeting"))), E(B(true)))
           && "with the predicates there, both register");
    const int64_t x = 21;
    assert(mt_one_int(mt_eval(m, E("rung_double", x))) == DOUBLED * x && "rung_double is a function");
    assert(answers_are(mt_eval(m, E("rung_greeting")), E(greeting)) && "and so is rung_greeting");

    clauses facts = { NULL, 0, 0 };
    static const char *const appended[] = { "a", "b" };
    for (size_t i = 0; i < 2; i++) {
        insert(&facts, facts.n, appended[i]);
        assert(answers_are(mt_eval(m, E("assertzPredicate", predicate(appended[i]))), E(B(true))) && "assertz appends a clause");
    }
    holds(m, "callPredicate answers every clause in order", &facts);
    static const char *const taken[] = { "a", "zzz" };
    for (size_t i = 0; i < 2; i++) {
        bool was = retracted(&facts, taken[i]);
        assert(answers_are(mt_eval(m, E("retractPredicate", predicate(taken[i]))), E(B(was))) && "retract answers whether a clause went");
        if (i == 0) holds(m, "the first unifying clause went", &facts);
    }
    insert(&facts, 0, "earliest");
    assert(answers_are(mt_eval(m, E("assertaPredicate", predicate("earliest"))), E(B(true))) && "asserta puts a clause first");
    holds(m, "so it answers first", &facts);
    free(facts.fact);

    for (int i = 0; i < 2; i++) assert(mt_library(m, "rung-alias", fixtures) && "a library path registers, and again");
    require("import through the alias",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "rung-alias", "rung_library.metta")))));
    const int64_t y = 5;
    assert(mt_one_int(mt_eval(m, E("rung-aliased", y))) == y + ALIASED_OFFSET && "the aliased file's function");
    static const char *const nowhere = "./no/such/directory";
    mt_clear();
    bool registered = mt_library(m, "rung-nowhere", nowhere);
    assert(!registered && mt_error() == MT_ERROR && mt_errmsg() && strstr(mt_errmsg(), nowhere) != NULL
           && "a directory that is not there is refused naming it");
    mt_clear();
    mt_close(m);
    return 0;
}
