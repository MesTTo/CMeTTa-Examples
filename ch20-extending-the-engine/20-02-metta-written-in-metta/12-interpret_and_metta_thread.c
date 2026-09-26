/* Purpose: three doors into the evaluator, each taking an atom, a type and
 *   the space to run it in. C keeps them as a table: metta and interpret
 *   take the carrier one step and are declared (-> Atom Type SpaceType
 *   Atom), metta-thread steps until nothing reduces and is undeclared. C's
 *   model of what a door answers follows from its row: on an ordinary call
 *   every door answers C's value, and on a body whose answer is itself
 *   reducible, a held sum or a function frame returning one, a one-step door
 *   answers the sum as written and metta-thread C's sum. Each door answers
 *   once per branch, and an atom nothing reduces is its own answer. twice
 *   is one body over the C_ and T_ operators in &self and another in
 *   &elsewhere, and a door evaluates against the space it is handed.
 * Guarantees: all nineteen claims of the original hold
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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define TWICE(MUL, x) MUL(2, x)
#define TWICE_ELSEWHERE(ADD, x) ADD(1000, x)

typedef struct door {
    const char *name;
    bool fixpoint, declared;
} door;

static const door metta_door = { "metta", false, true }, interpret = { "interpret", false, true },
                  metta_thread = { "metta-thread", true, false };

/* What DOOR answers for a body whose one step gives STEPPED and whose
   fixpoint is REDUCED. TAKES both. */
static mt_atom *through(const door *d, mt_atom *stepped, mt_atom *reduced)
{
    mt_drop(d->fixpoint ? stepped : reduced);
    return d->fixpoint ? reduced : stepped;
}

static mt_answers *asked(metta *m, const door *d, mt_atom *atom, const char *type, const char *space)
{
    return mt_eval(m, E(d->name, atom, type, mt_spaceref(space)));
}

static mt_atom *door_type(const door *d)
{
    return d->declared ? E("->", "Atom", "Type", "SpaceType", "Atom") : S("%Undefined%");
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: twice (-> Number Number))", mt_add(m, E(":", "twice", E("->", "Number", "Number"))));
    require("twice", mt_add(m, E("=", E("twice", V("x")), TWICE(T_MUL, V("x")))));
    const door *doors[] = { &metta_door, &interpret, &metta_thread };
    enum { DOORS = sizeof doors / sizeof *doors };
    for (size_t i = 0; i < DOORS; i++)
        assert(mt_one_int(asked(m, doors[i], E("twice", 5), "%Undefined%", "&self")) == TWICE(C_MUL, 5)
               && "an ordinary call takes one step");
    assert(mt_one_int(asked(m, &interpret, T_ADD(1, 2), "Number", "&self")) == C_ADD(1, 2) && "a typed door");

    static const int64_t branches[] = { 1, 2 };
    require("gen", mt_add(m, E("=", E("gen"), E("superpose", E(branches[0], branches[1])))));
    const door *branching[] = { &interpret, &metta_thread };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(asked(m, branching[i], E("gen"), "%Undefined%", "&self"), E(branches[0], branches[1])) && "once per branch");

    require("held", mt_add(m, E("=", E("held"), E("noeval", T_ADD(1, 2)))));
    require("framed", mt_add(m, E("=", E("framed"), E("function", E("return", T_ADD(1, 2))))));
    const struct { const char *body; const door *door; } reducible[] = {
        { "held", &interpret }, { "held", &metta_door }, { "held", &metta_thread },
        { "framed", &interpret }, { "framed", &metta_thread },
    };
    for (size_t i = 0; i < sizeof reducible / sizeof *reducible; i++)
        assert(answers_are(asked(m, reducible[i].door, E(reducible[i].body), "%Undefined%", "&self"), E(through(reducible[i].door, T_ADD(1, 2), N(C_ADD(1, 2)))))
               && "a door steps as its row says");
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(asked(m, branching[i], S("nosuchhead"), "%Undefined%", "&self"), E("nosuchhead"))
               && "an irreducible atom is its own answer");
    for (size_t i = 0; i < DOORS; i++)
        assert(answers_are(mt_eval(m, E("get-type", doors[i]->name)), E(door_type(doors[i]))) && "a declared door masks its atom");

    mt_space *elsewhere = mt_space_open(m, "&elsewhere");
    require("open &elsewhere", elsewhere != NULL);
    require("twice elsewhere", mt_add(elsewhere, E("=", E("twice", V("x")), TWICE_ELSEWHERE(T_ADD, V("x")))));
    for (size_t i = 0; i < 2; i++)
        assert(mt_one_int(asked(m, branching[i], E("twice", 5), "%Undefined%", "&elsewhere")) == TWICE_ELSEWHERE(C_ADD, 5)
               && "the space handed in decides");
    assert(mt_one_int(asked(m, &interpret, E("twice", 5), "%Undefined%", "&self")) == TWICE(C_MUL, 5) && "and &self keeps its own");
    mt_space_close(elsewhere);
    mt_close(m);
    return 0;
}
