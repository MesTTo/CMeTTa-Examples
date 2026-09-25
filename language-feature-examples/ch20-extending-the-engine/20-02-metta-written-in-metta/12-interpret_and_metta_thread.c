/* Purpose: three doors into the evaluator, each taking an atom, a type and
 *   the space to run it in. C keeps them as a table: metta and interpret
 *   take the carrier one step and are declared (-> Atom Type SpaceType
 *   Atom), metta-thread steps until nothing reduces and is undeclared. C's
 *   model of what a door answers follows from its row: on an ordinary call
 *   every door answers C's value, and on a body whose answer is itself
 *   reducible, a held sum or a function frame returning one, a one-step door
 *   answers the sum as written and metta-thread C's sum. Each door answers
 *   once per branch, and an atom nothing reduces is its own answer. twice
 *   is one body over lowering.h's operators in &self and another in
 *   &elsewhere, and a door evaluates against the space it is handed.
 * Guarantees: all nineteen claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

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
    metta *m = open_engine();
    require("(: twice (-> Number Number))", mt_add(m, E(":", "twice", E("->", "Number", "Number"))));
    require("twice", mt_add(m, E("=", E("twice", V("x")), TWICE(T_MUL, V("x")))));
    const door *doors[] = { &metta_door, &interpret, &metta_thread };
    enum { DOORS = sizeof doors / sizeof *doors };
    for (size_t i = 0; i < DOORS; i++)
        check_int("an ordinary call takes one step", mt_one_int(asked(m, doors[i], E("twice", 5), "%Undefined%", "&self")),
                  TWICE(C_MUL, 5));
    check_int("a typed door", mt_one_int(asked(m, &interpret, T_ADD(1, 2), "Number", "&self")), C_ADD(1, 2));

    static const int64_t branches[] = { 1, 2 };
    require("gen", mt_add(m, E("=", E("gen"), E("superpose", E(branches[0], branches[1])))));
    const door *branching[] = { &interpret, &metta_thread };
    for (size_t i = 0; i < 2; i++)
        check_answers("once per branch", asked(m, branching[i], E("gen"), "%Undefined%", "&self"), branches[0], branches[1]);

    require("held", mt_add(m, E("=", E("held"), E("noeval", T_ADD(1, 2)))));
    require("framed", mt_add(m, E("=", E("framed"), E("function", E("return", T_ADD(1, 2))))));
    const struct { const char *body; const door *door; } reducible[] = {
        { "held", &interpret }, { "held", &metta_door }, { "held", &metta_thread },
        { "framed", &interpret }, { "framed", &metta_thread },
    };
    for (size_t i = 0; i < sizeof reducible / sizeof *reducible; i++)
        check_answers("a door steps as its row says", asked(m, reducible[i].door, E(reducible[i].body), "%Undefined%", "&self"),
                      through(reducible[i].door, T_ADD(1, 2), N(C_ADD(1, 2))));
    for (size_t i = 0; i < 2; i++)
        check_answers("an irreducible atom is its own answer", asked(m, branching[i], S("nosuchhead"), "%Undefined%", "&self"),
                      "nosuchhead");
    for (size_t i = 0; i < DOORS; i++)
        check_answers("a declared door masks its atom", mt_eval(m, E("get-type", doors[i]->name)), door_type(doors[i]));

    mt_space *elsewhere = mt_space_open(m, "&elsewhere");
    require("open &elsewhere", elsewhere != NULL);
    require("twice elsewhere", mt_add(elsewhere, E("=", E("twice", V("x")), TWICE_ELSEWHERE(T_ADD, V("x")))));
    for (size_t i = 0; i < 2; i++)
        check_int("the space handed in decides", mt_one_int(asked(m, branching[i], E("twice", 5), "%Undefined%", "&elsewhere")),
                  TWICE_ELSEWHERE(C_ADD, 5));
    check_int("and &self keeps its own", mt_one_int(asked(m, &interpret, E("twice", 5), "%Undefined%", "&self")), TWICE(C_MUL, 5));
    mt_space_close(elsewhere);
    return done(m);
}
