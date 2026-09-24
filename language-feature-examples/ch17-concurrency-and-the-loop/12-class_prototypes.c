/* Purpose: a prototype is an instance that is a knowledge base of its own: a
 *   handle (Class <space>) around a space holding the instance's fields as
 *   facts and its private rules as equations. C names each instance's space
 *   from a counter, opens it, and writes into it the reference to its class
 *   and its mood. The methods are C functions over the handle, each reading
 *   the instance through the space the handle carries: Agent-mood reads the
 *   field, and decide dispatches on the handle's constructor through a C
 *   table, each class answering its own verb over the mood. A private rule is
 *   an equation C writes into one instance's space. Evaluating in that space,
 *   mt_eval on the space, which is evalc, reduces it, and from &self the same
 *   call answers itself unreduced. Scout's space references Scout's class,
 *   whose :< edge makes it an Agent.
 * Guarantees: all five claims of the original hold, with its three
 *   constructions checked as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* The two classes: each one's constructor, the verb its decide answers with,
   and its base, NULL for the root. */
typedef struct agent_class {
    const char *name, *verb, *base;
} agent_class;

static const agent_class agent_classes[] = {
    { .name = "Agent", .verb = "act-on" },
    { .name = "Scout", .verb = "scouting", .base = "Agent" },
};
#define CLASSES (sizeof agent_classes / sizeof *agent_classes)

static metta *engine;
static unsigned instances;

static const agent_class *class_named(const char *name)
{
    for (size_t i = 0; i < CLASSES; i++)
        if (strcmp(agent_classes[i].name, name) == 0) return &agent_classes[i];
    return NULL;
}

/* The class and space a handle (Class <space>) carries. */
static const agent_class *receiver(const mt_atom *handle, mt_space **space)
{
    if (mt_kind_of(handle) != MT_EXPR || mt_len(handle) != 2 || mt_kind_of(mt_at(handle, 0)) != MT_SYMBOL) return NULL;
    const agent_class *c = class_named(mt_name(mt_at(handle, 0)));
    return c && (*space = mt_space_open(engine, mt_name(mt_at(handle, 1)))) ? c : NULL;
}

/* The instance's mood: its one _field-mood fact. */
static mt_atom *mood_of(mt_space *space)
{
    mt_atom *fact = mt_first(mt_match(space, E("_field-mood", V("mood"))));
    mt_atom *mood = fact ? mt_keep(mt_at(fact, 1)) : NULL;
    mt_drop(fact);
    return mood;
}

/* make-Agent and make-Scout: a fresh space referencing the class, holding
   the mood. */
static mt_status make(mt_call *call, void *user)
{
    const agent_class *c = user;
    char name[48];
    snprintf(name, sizeof name, "&%s-%u", c->name, ++instances);
    mt_space *space = mt_space_open(mt_of(call), name);
    if (!space) return mt_error();
    char class_space[40];
    snprintf(class_space, sizeof class_space, "&%s", c->name);
    bool written = mt_add(space, E("from", mt_spaceref(class_space))) && mt_add(space, E("_field-mood", mt_keep(mt_arg(call, 0))));
    mt_space_close(space);
    return written ? mt_answer(call, E(c->name, mt_spaceref(name))) : mt_error();
}

typedef enum { MOOD, DECIDE, OWN_DECIDE } method;

static mt_status call_method(mt_call *call, method which, const agent_class *own)
{
    mt_space *space = NULL;
    const agent_class *c = receiver(mt_arg(call, 0), &space);
    if (!c || (own && c != own)) {
        mt_space_close(space);
        return MT_FAIL;
    }
    mt_atom *mood = mood_of(space);
    mt_space_close(space);
    if (!mood) return MT_FAIL;
    return mt_answer(call, which == MOOD ? mood : E(c->verb, mood));
}

static mt_status mood_op(mt_call *call, void *user) { (void)user; return call_method(call, MOOD, NULL); }
static mt_status decide_op(mt_call *call, void *user) { (void)user; return call_method(call, DECIDE, NULL); }
static mt_status own_decide_op(mt_call *call, void *user) { return call_method(call, OWN_DECIDE, user); }

static void publish(metta *m, const char *name, mt_fn fn, const void *user)
{
    require(name, mt_def(m, (mt_op){ .name = name, .arity = 1, .effect = MT_LOOKUP, .fn = fn, .user = (void *)user }));
}

int main(void)
{
    metta *m = engine = open_engine();
    publish(m, "Agent-mood", mood_op, NULL);
    publish(m, "decide", decide_op, NULL);
    static char heads[CLASSES][2][32];
    mt_space *spaces[CLASSES];
    for (size_t i = 0; i < CLASSES; i++) {
        const agent_class *c = &agent_classes[i];
        snprintf(heads[i][0], sizeof heads[i][0], "make-%s", c->name);
        snprintf(heads[i][1], sizeof heads[i][1], "%s-decide", c->name);
        require(heads[i][0], mt_def(m, (mt_op){ .name = heads[i][0], .arity = 1, .effect = MT_WRITES, .fn = make, .user = (void *)c }));
        publish(m, heads[i][1], own_decide_op, c);
        char name[40];
        snprintf(name, sizeof name, "&%s", c->name);
        require("open the class space", (spaces[i] = mt_space_open(m, name)) != NULL);
        require("its type", mt_add(spaces[i], E(":", c->name, E("->", "SpaceType", c->name))));
        if (c->base) require("its base", mt_add(spaces[i], E(":<", c->name, c->base)));
        require("(from &Class)", mt_add(m, E("from", mt_spaceref(name))));
    }

    const agent_class *agent = &agent_classes[0], *scout = &agent_classes[1];
    const char *moods[] = { "calm", "alert" };
    mt_atom *agents[sizeof moods / sizeof *moods];
    for (size_t i = 0; i < sizeof moods / sizeof *moods; i++) {
        agents[i] = mt_first(mt_eval(m, E(heads[0][0], moods[i])));
        check("make-Agent answers a handle", agents[i] != NULL);
    }
    for (size_t i = 0; i < sizeof moods / sizeof *moods; i++)
        check_answers("an agent decides by its own facts", mt_eval(m, E("decide", mt_keep(agents[i]))), E(agent->verb, moods[i]));

    mt_space *calm_space = NULL;
    require("the calm agent's space", receiver(agents[0], &calm_space) != NULL);
    require("its private rule", mt_add(calm_space, E("=", E("private-rule", V("agent")), E("secret", E("Agent-mood", V("agent"))))));
    mt_atom *rule_call = E("private-rule", mt_keep(agents[0]));
    check_answers("which runs in its space", mt_eval(calm_space, mt_keep(rule_call)), E("secret", moods[0]));
    check_answers("and stays unreduced everywhere else", mt_eval(m, mt_keep(rule_call)), mt_keep(rule_call));
    mt_drop(rule_call);
    mt_space_close(calm_space);

    const char *curious = "curious";
    mt_atom *scout_handle = mt_first(mt_eval(m, E(heads[1][0], curious)));
    check("make-Scout answers a handle", scout_handle != NULL);
    check_answers("a Scout decides by its own class", mt_eval(m, E("decide", mt_keep(scout_handle))), E(scout->verb, curious));

    mt_drop(scout_handle);
    for (size_t i = 0; i < sizeof moods / sizeof *moods; i++) mt_drop(agents[i]);
    for (size_t i = 0; i < CLASSES; i++) mt_space_close(spaces[i]);
    return done(m);
}
