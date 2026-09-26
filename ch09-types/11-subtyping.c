/* Purpose: subtyping widens a value's list of types, and C runs the widening
 *   the original describes over the atoms it has added: a symbol's declared
 *   types, then rounds that append, for each type present when the round
 *   began, the supertype of every edge (:< sub super) whose sub unifies with
 *   it, unless that round's starting list held the supertype already; rounds
 *   repeat until one adds nothing. So Rex gains Animal and then LivingThing,
 *   a diamond appends its join once per path, and a pattern edge reaches an
 *   instance. A number's type is not widened, nor an implemented
 *   operation's result; a constructor's, an arrow with no equation, is. An
 *   argument is admitted when the parameter's type is in its widened list.
 * Guarantees: all eight claims of the original hold
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

/* The atoms C has added, in order: what its types are read from. */
typedef struct program {
    mt_atom **atoms;
    size_t n, cap;
} program;

static void add(metta *m, program *p, mt_atom *atom)
{
    if (p->n == p->cap) {
        p->cap = p->cap ? 2 * p->cap : 16;
        p->atoms = realloc(p->atoms, p->cap * sizeof *p->atoms);
        require("room for the program", p->atoms != NULL);
    }
    require("add the atom", mt_add(m, mt_keep(atom)));
    p->atoms[p->n++] = atom;
}

static bool headed(const mt_atom *x, const char *symbol)
{
    return mt_kind_of(x) == MT_EXPR && mt_len(x) == 3 && mt_kind_of(mt_at(x, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(x, 0)), symbol) == 0;
}

static bool member(const mt_atom *list, size_t n, const mt_atom *x)
{
    for (size_t i = 0; i < n; i++)
        if (mt_eq(mt_at(list, i), x)) return true;
    return false;
}

/* Each type (: x T) declares, in order. */
static mt_atom *declared(const program *p, const mt_atom *x)
{
    mt_atom **types = malloc((p->n + 1) * sizeof *types);
    size_t k = 0;
    require("room for the types", types != NULL);
    for (size_t i = 0; i < p->n; i++)
        if (headed(p->atoms[i], ":") && mt_eq(mt_at(p->atoms[i], 1), x)) types[k++] = mt_keep(mt_at(p->atoms[i], 2));
    mt_atom *out = mt_exprv(k, types);
    free(types);
    return out;
}

/* The widening rounds. Time: rounds * |list| * |edges| unifications. */
static mt_atom *widened(const program *p, mt_atom *types)
{
    for (bool grew = true; grew;) {
        size_t start = mt_len(types), cap = start + 1, n = start;
        mt_atom **next = malloc(cap * sizeof *next);
        require("room for the types", next != NULL);
        for (size_t i = 0; i < start; i++) next[i] = mt_keep(mt_at(types, i));
        grew = false;
        for (size_t i = 0; i < start; i++)
            for (size_t e = 0; e < p->n; e++) {
                mt_bindings *b = headed(p->atoms[e], ":<") ? mt_unify(mt_at(p->atoms[e], 1), mt_at(types, i)) : NULL;
                if (!b) continue;
                mt_atom *super = mt_substitute(mt_at(p->atoms[e], 2), b);
                mt_bindings_free(b);
                if (member(types, start, super)) {
                    mt_drop(super);
                    continue;
                }
                if (n == cap) next = realloc(next, (cap *= 2) * sizeof *next), require("room for the types", next != NULL);
                next[n++] = super, grew = true;
            }
        mt_drop(types);
        types = mt_exprv(n, next);
        free(next);
    }
    return types;
}

/* An application's types: its head's arrow result, widened only when no
   equation implements the head, which makes it a constructor. */
static mt_atom *application_types(const program *p, const char *head)
{
    mt_atom *h = S(head), *arrows = declared(p, h);
    bool implemented = false;
    for (size_t i = 0; i < p->n; i++)
        implemented |= headed(p->atoms[i], "=") && mt_kind_of(mt_at(p->atoms[i], 1)) == MT_EXPR && mt_eq(mt_at(mt_at(p->atoms[i], 1), 0), h);
    require("one arrow", mt_len(arrows) == 1);
    const mt_atom *arrow = mt_at(arrows, 0);
    mt_atom *result = E(mt_keep(mt_at(arrow, mt_len(arrow) - 1)));
    mt_drop(h), mt_drop(arrows);
    return implemented ? result : widened(p, result);
}

static mt_atom *symbol_types(const program *p, const char *symbol)
{
    mt_atom *s = S(symbol), *out = widened(p, declared(p, s));
    mt_drop(s);
    return out;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    program p = { 0 };
    add(m, &p, E(":", "Rex", "Dog"));
    add(m, &p, E(":<", "Dog", "Animal"));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", "Rex"))), E(symbol_types(&p, "Rex"))) && "a value and its supertype");

    add(m, &p, E(":", "speak", E("->", "Animal", "String")));
    mt_atom *noise = E("=", E("speak", V("a")), T("some noise"));
    add(m, &p, noise);
    mt_atom *rex = symbol_types(&p, "Rex"), *animal = S("Animal");
    require("an admitted argument", member(rex, mt_len(rex), animal));
    assert(answers_are(mt_eval(m, E("speak", "Rex")), E(mt_keep(mt_at(noise, 2)))) && "a Dog is admitted where an Animal is asked");
    mt_drop(rex), mt_drop(animal);

    add(m, &p, E(":<", "Animal", "LivingThing"));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", "Rex"))), E(symbol_types(&p, "Rex"))) && "widening is transitive");

    add(m, &p, E(":<", "Number", "Countable"));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", 1))), E(E("Number"))) && "a literal's type is not widened");

    add(m, &p, E(":", "half", E("->", "Number", "Fraction")));
    add(m, &p, E("=", E("half", V("n")), E("/", V("n"), 2)));
    add(m, &p, E(":<", "Fraction", "Rational"));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", E("half", 3)))), E(application_types(&p, "half"))) && "nor an operation's result");
    add(m, &p, E(":", "ratio", E("->", "Number", "Number", "Fraction")));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", E("ratio", 1, 2)))), E(application_types(&p, "ratio"))) && "a constructor's is");

    const char *edges[][2] = { { "Bat", "Bird" }, { "Bat", "Mammal" }, { "Bird", "Animal2" }, { "Mammal", "Animal2" } };
    for (size_t i = 0; i < 4; i++) add(m, &p, E(":<", edges[i][0], edges[i][1]));
    add(m, &p, E(":", "Stellaluna", "Bat"));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", "Stellaluna"))), E(symbol_types(&p, "Stellaluna"))) && "a diamond answers its join twice");

    add(m, &p, E(":<", E("Boxed", V("t")), "Container"));
    add(m, &p, E(":", "crate", E("Boxed", "Apple")));
    assert(answers_are(mt_eval(m, E("collapse", E("get-type", "crate"))), E(symbol_types(&p, "crate"))) && "a pattern edge reaches an instance");

    for (size_t i = 0; i < p.n; i++) mt_drop(p.atoms[i]);
    free(p.atoms);
    mt_close(m);
    return 0;
}
