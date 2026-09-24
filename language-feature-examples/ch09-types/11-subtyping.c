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
 * Guarantees: all eight claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    program p = { 0 };
    add(m, &p, E(":", "Rex", "Dog"));
    add(m, &p, E(":<", "Dog", "Animal"));
    check_answers("a value and its supertype", mt_eval(m, E("collapse", E("get-type", "Rex"))), symbol_types(&p, "Rex"));

    add(m, &p, E(":", "speak", E("->", "Animal", "String")));
    mt_atom *noise = E("=", E("speak", V("a")), T("some noise"));
    add(m, &p, noise);
    mt_atom *rex = symbol_types(&p, "Rex"), *animal = S("Animal");
    require("an admitted argument", member(rex, mt_len(rex), animal));
    check_answers("a Dog is admitted where an Animal is asked", mt_eval(m, E("speak", "Rex")), mt_keep(mt_at(noise, 2)));
    mt_drop(rex), mt_drop(animal);

    add(m, &p, E(":<", "Animal", "LivingThing"));
    check_answers("widening is transitive", mt_eval(m, E("collapse", E("get-type", "Rex"))), symbol_types(&p, "Rex"));

    add(m, &p, E(":<", "Number", "Countable"));
    check_answers("a literal's type is not widened", mt_eval(m, E("collapse", E("get-type", 1))), E("Number"));

    add(m, &p, E(":", "half", E("->", "Number", "Fraction")));
    add(m, &p, E("=", E("half", V("n")), E("/", V("n"), 2)));
    add(m, &p, E(":<", "Fraction", "Rational"));
    check_answers("nor an operation's result", mt_eval(m, E("collapse", E("get-type", E("half", 3)))), application_types(&p, "half"));
    add(m, &p, E(":", "ratio", E("->", "Number", "Number", "Fraction")));
    check_answers("a constructor's is", mt_eval(m, E("collapse", E("get-type", E("ratio", 1, 2)))), application_types(&p, "ratio"));

    const char *edges[][2] = { { "Bat", "Bird" }, { "Bat", "Mammal" }, { "Bird", "Animal2" }, { "Mammal", "Animal2" } };
    for (size_t i = 0; i < 4; i++) add(m, &p, E(":<", edges[i][0], edges[i][1]));
    add(m, &p, E(":", "Stellaluna", "Bat"));
    check_answers("a diamond answers its join twice", mt_eval(m, E("collapse", E("get-type", "Stellaluna"))), symbol_types(&p, "Stellaluna"));

    add(m, &p, E(":<", E("Boxed", V("t")), "Container"));
    add(m, &p, E(":", "crate", E("Boxed", "Apple")));
    check_answers("a pattern edge reaches an instance", mt_eval(m, E("collapse", E("get-type", "crate"))), symbol_types(&p, "crate"));

    for (size_t i = 0; i < p.n; i++) mt_drop(p.atoms[i]);
    free(p.atoms);
    return done(m);
}
