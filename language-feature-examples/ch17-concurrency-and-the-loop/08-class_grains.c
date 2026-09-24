/* Purpose: the three class grains, a value, an entity whose fields are facts
 *   in its class space, and a prototype that is a space of its own, each
 *   scoped, and every scope's body a C function the scope evaluates. A value's
 *   accessors are rows C derives from its field table, (= (GrainPoint-x
 *   (GrainPoint $x $y)) $x), which is why the first claim can query one. The
 *   entity's methods are C functions over the handle: the constructor runs in
 *   mt_transaction, has the engine mint the token as the owner row's own,
 *   writes the fields and registers the retirement with scope-defer; the
 *   writer replaces a field in mt_transaction, so a body that fails after it
 *   rolls it back; retirement removes every field row of the account and its
 *   owner row that remain, where the original's remove-atom removes the first
 *   field row, a difference no claim reads, and like the original's it
 *   succeeds when nothing remains. The prototype's instance is a new space
 *   holding its label, and a rule written into one instance's space reduces
 *   only when evaluated there. C keeps its own model of each value, balance
 *   and label, and the engine answers the model. A deferred cleanup runs as
 *   its scope exits, and a scope's answer keeps the space its dependency
 *   query reaches.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static metta *engine;

static mt_space *space_of(const mt_atom *ref)
{
    mt_space *s = mt_space_open(engine, mt_name(ref));
    require("open a space", s != NULL);
    return s;
}

/* Whether a space holds an atom unifying with pattern. TAKES the pattern. */
static bool holds(mt_space *s, mt_atom *pattern)
{
    mt_atom *found = mt_first(mt_match(s, pattern));
    mt_drop(found);
    return found != NULL;
}

/* A new space the enclosing scope owns, and its handle. */
static mt_atom *new_space(mt_space **handle)
{
    mt_atom *ref = mt_first(mt_eval(engine, E("new-space")));
    require("new-space answers a space", ref != NULL);
    *handle = space_of(ref);
    return ref;
}

/* ---- the value grain: GrainPoint, whose accessors are derived rows ---- */

static const char *const point_fields[] = { "x", "y" };
#define POINT_FIELDS (sizeof point_fields / sizeof *point_fields)

/* The accessor row for field i: (= (GrainPoint-f (GrainPoint $x $y)) $f). */
static mt_atom *accessor_row(size_t i)
{
    mt_atom *vars[POINT_FIELDS + 1];
    char head[48];
    vars[0] = S("GrainPoint");
    for (size_t k = 0; k < POINT_FIELDS; k++) vars[k + 1] = V(point_fields[k]);
    snprintf(head, sizeof head, "GrainPoint-%s", point_fields[i]);
    return E("=", E(head, mt_exprv(POINT_FIELDS + 1, vars)), V(point_fields[i]));
}

/* ---- the entity grain: GrainAccount, its fields facts in &GrainAccount ---- */

static mt_space *accounts;

static mt_atom *field_fact(const char *field, const mt_atom *handle, mt_atom *value)
{
    char head[48];
    snprintf(head, sizeof head, "_field-%s", field);
    return E(head, mt_keep(handle), value);
}

static int64_t balance_of(const mt_atom *handle)
{
    mt_atom *fact = mt_first(mt_match(accounts, field_fact("balance", handle, V("n"))));
    int64_t n = fact ? mt_int(mt_at(fact, 2)) : -1;
    mt_drop(fact);
    return n;
}

typedef struct balance_write {
    const mt_atom *handle;
    int64_t n;
} balance_write;

static mt_status replace_balance(metta *m, void *user)
{
    (void)m;
    const balance_write *w = user;
    if (!mt_del(accounts, field_fact("balance", w->handle, V("old")))) return mt_ok() ? MT_FAIL : mt_error();
    return mt_add(accounts, field_fact("balance", w->handle, N(w->n))) ? MT_OK : mt_error();
}

static mt_status set_balance(const mt_atom *handle, int64_t n)
{
    return mt_transaction(engine, replace_balance, &(balance_write){ handle, n });
}

/* Every field row of the account and its owner row, whichever remain, so a
   second retirement, the scope's after a discard, removes nothing and
   succeeds, as the original's progn answers True whatever its removals
   found. False only when the engine failed. */
static bool retire(const mt_atom *handle)
{
    mt_clear();
    mt_list rows = mt_all(mt_match(accounts, E(V("field"), mt_keep(handle), V("value"))));
    for (size_t i = 0; i < rows.len; i++) (void)mt_del(accounts, mt_keep(rows.items[i]));
    mt_list_free(rows);
    (void)mt_del(accounts, E("owned-by", mt_keep(handle)));
    return mt_ok();
}

typedef struct creation {
    const mt_atom *owner;
    int64_t balance;
    mt_atom *handle;
} creation;

static mt_status create(metta *m, void *user)
{
    creation *c = user;
    mt_atom *token = mt_first(mt_eval(m, E("progn", E("add-atom", mt_spaceref(mt_space_name(accounts)),
                                                      E("owned-by", E("GrainAccount", V("id"))), V("id")),
                                            V("id"))));
    if (!token) return mt_ok() ? MT_FAIL : mt_error();
    c->handle = E("GrainAccount", token);
    bool written = mt_add(accounts, field_fact("owner", c->handle, mt_keep(c->owner))) &&
                   mt_add(accounts, field_fact("balance", c->handle, N(c->balance)));
    /* The scope that owns the account retires it as it exits, and keeps what
       the account's fields reach while it lives. */
    mt_atom *retirement = E("evalc", E("retire-GrainAccount", mt_keep(c->handle)), mt_spaceref(mt_space_name(accounts)));
    mt_atom *reaches = E("match", mt_spaceref(mt_space_name(accounts)), E(V("field"), mt_keep(c->handle), V("value")), V("value"));
    mt_atom *deferred = mt_first(mt_eval(m, E("scope-defer", mt_keep(c->handle), retirement, reaches)));
    bool registered = deferred != NULL;
    mt_drop(deferred);
    return written && registered ? MT_OK : mt_error();
}

/* ---- the prototype grain: GrainAgent, an instance that is a space ---- */

static mt_space *agents;

static mt_atom *label_of(const mt_atom *handle)
{
    mt_space *instance = space_of(mt_at(handle, 1));
    mt_atom *fact = mt_first(mt_match(instance, E("_field-label", V("label"))));
    mt_space_close(instance);
    mt_atom *label = fact ? mt_keep(mt_at(fact, 1)) : NULL;
    mt_drop(fact);
    return label;
}

static mt_atom *make_agent(const mt_atom *label)
{
    mt_space *instance;
    mt_atom *ref = new_space(&instance);
    bool written = mt_add(instance, E("from", mt_spaceref(mt_space_name(agents)))) &&
                   mt_add(instance, E("internal", "_field-label", "private_rule")) && mt_add(instance, E("_field-label", mt_keep(label)));
    mt_space_close(instance);
    mt_atom *handle = E("GrainAgent", ref);
    if (written && mt_add(agents, E("owned-by", mt_keep(handle)))) return handle;
    mt_drop(handle);
    return NULL;
}

/* ---- the published methods ---- */

typedef enum { ACCOUNT_BALANCE, ACCOUNT_SET, ACCOUNT_RETIRE, ACCOUNT_MAKE, AGENT_LABEL, AGENT_MAKE, DISCARD } method;

static mt_status call_method(mt_call *call, void *user)
{
    const mt_atom *a = mt_arg(call, 0);
    switch ((method)(intptr_t)user) {
    case ACCOUNT_BALANCE: {
        int64_t n = balance_of(a);
        return n < 0 ? MT_FAIL : mt_answer(call, N(n));
    }
    case ACCOUNT_SET: return set_balance(a, mt_int(mt_arg(call, 1))) == MT_OK ? mt_answer(call, B(true)) : mt_error();
    case ACCOUNT_RETIRE:
    case DISCARD: return retire(a) ? mt_answer(call, B(true)) : mt_error();
    case ACCOUNT_MAKE: {
        creation c = { a, mt_int(mt_arg(call, 1)), NULL };
        if (mt_transaction(mt_of(call), create, &c) == MT_OK) return mt_answer(call, c.handle);
        mt_drop(c.handle);
        return mt_error();
    }
    case AGENT_LABEL: {
        mt_atom *label = label_of(a);
        return label ? mt_answer(call, label) : MT_FAIL;
    }
    case AGENT_MAKE: break;
    }
    mt_atom *handle = make_agent(a);
    return handle ? mt_answer(call, handle) : mt_error();
}

/* ---- the three scopes' bodies ---- */

typedef struct model {
    const char *owner;
    int64_t balance;
    mt_atom *handle;
} model;

static int by_value(const void *a, const void *b) { return (*(const int64_t *)a > *(const int64_t *)b) - (*(const int64_t *)a < *(const int64_t *)b); }

static mt_status overdraw(metta *m, void *user)
{
    (void)m;
    if (set_balance(user, 0) != MT_OK) return mt_error();
    return mt_error_set(MT_ERROR, "withdraw overdraft");
}

/* The grains, in a scope: each claim of the original's tuple held here. */
static mt_status grains(mt_call *call, void *user)
{
    (void)user;
    mt_space *home;
    mt_atom *home_ref = new_space(&home);
    const char *class_names[] = { "&GrainPoint", mt_space_name(accounts), mt_space_name(agents) };
    for (size_t i = 0; i < sizeof class_names / sizeof *class_names; i++)
        require("home reads the class", mt_add(home, E("from", mt_spaceref(class_names[i]))));

    mt_atom *point_class = mt_spaceref("&GrainPoint");
    mt_space *points = space_of(point_class);
    mt_drop(point_class);
    check("the accessor is a row the class space holds", holds(points, accessor_row(0)));
    mt_space_close(points);
    const int64_t point[POINT_FIELDS] = { 3, 4 };
    check_answers("and it reads its field", mt_eval(home, E("GrainPoint-x", E("GrainPoint", point[0], point[1]))), N(point[0]));

    model alice = { "alice", 40, NULL }, bob = { "bob", 80, NULL };
    model *all[] = { &alice, &bob };
    const size_t population = sizeof all / sizeof *all;
    for (size_t i = 0; i < population; i++)
        require("make-GrainAccount", (all[i]->handle = mt_first(mt_eval(home, E("make-GrainAccount", T(all[i]->owner), all[i]->balance)))) != NULL);
    const int64_t written = 50;
    check_answers("the writer replaces a field", mt_eval(home, E("GrainAccount-balance!", mt_keep(alice.handle), written)), B(true));
    alice.balance = written;
    mt_list facts = mt_all(mt_match(accounts, E("_field-balance", V("account"), V("n"))));
    int64_t *held = malloc((facts.len + 1) * sizeof *held), *modelled = malloc(population * sizeof *modelled);
    require("room for the population", held && modelled);
    for (size_t i = 0; i < facts.len; i++) held[i] = mt_int(mt_at(facts.items[i], 2));
    for (size_t i = 0; i < population; i++) modelled[i] = all[i]->balance;
    qsort(held, facts.len, sizeof *held, by_value);
    qsort(modelled, population, sizeof *modelled, by_value);
    check("the population is one query over the class space", facts.len == population && memcmp(held, modelled, population * sizeof *held) == 0);
    free(held), free(modelled);
    mt_list_free(facts);
    mt_clear();
    check("a body failing after a write rolls it back", mt_transaction(engine, overdraw, alice.handle) == MT_ERROR);
    mt_clear();
    check_answers("so the balance stands", mt_eval(home, E("GrainAccount-balance", mt_keep(alice.handle))), N(alice.balance));
    check_answers("discard retires an account", mt_eval(home, E("discard", mt_keep(bob.handle))), B(true));
    mt_list owners = mt_all(mt_match(accounts, E("owned-by", V("account"))));
    check_int("so one owner remains", (int64_t)owners.len, (int64_t)population - 1);
    mt_list_free(owners);

    const char *labels[] = { "left", "right" };
    for (size_t i = 0; i < sizeof labels / sizeof *labels; i++) {
        mt_atom *agent = mt_first(mt_eval(home, E("make-GrainAgent", labels[i])));
        require("make-GrainAgent", agent != NULL);
        mt_space *instance = space_of(mt_at(agent, 1));
        require("the private rule", mt_add(instance, E("=", E("private_rule", V("receiver")), E("GrainAgent-label", V("receiver")))));
        check_answers("a private rule runs in its own space", mt_eval(instance, E("private_rule", mt_keep(agent))), S(labels[i]));
        mt_space_close(instance);
        if (i == 0) {
            mt_atom *outside = E("private_rule", mt_keep(agent));
            check_answers("and from home stays unreduced", mt_eval(home, mt_keep(outside)), mt_keep(outside));
            mt_drop(outside);
        }
        mt_drop(agent);
    }
    for (size_t i = 0; i < population; i++) mt_drop(all[i]->handle);
    mt_space_close(home);
    mt_drop(home_ref);
    return mt_answer(call, B(true));
}

/* Within a scope: a fact written to a space the scope does not own, and its
   removal deferred to the scope's exit. */
static mt_status during(mt_call *call, void *user)
{
    (void)user;
    mt_space *rows = space_of(mt_arg(call, 0));
    bool written = mt_add(rows, E("owned", "item"));
    mt_atom *deferred = mt_first(mt_eval(engine, E("scope_defer", "item", E("remove-atom", mt_keep(mt_arg(call, 0)), E("owned", "item")))));
    bool held = written && deferred && holds(rows, E("owned", V("x")));
    mt_drop(deferred);
    mt_space_close(rows);
    return mt_answer(call, B(held));
}

/* The inner scope: a child space whose one field the answer's dependency
   query reaches, so answering the field's name keeps the child. */
static mt_status inner(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *store_ref = mt_arg(call, 0);
    mt_space *store = space_of(store_ref), *child;
    mt_atom *child_ref = new_space(&child);
    const int64_t payload = 9;
    bool written = mt_add(child, E("payload", payload)) && mt_add(store, E("field", "scoped-value", mt_keep(child_ref)));
    mt_atom *deferred = mt_first(mt_eval(engine, E("scope-defer", "scoped-value",
                                                   E("remove-atom", mt_keep(store_ref), E("field", "scoped-value", V("old"))),
                                                   E("match", mt_keep(store_ref), E("field", "scoped-value", V("current")), V("current")))));
    bool ok = written && deferred;
    mt_drop(deferred), mt_drop(child_ref);
    mt_space_close(child), mt_space_close(store);
    return ok ? mt_answer(call, S("scoped-value")) : mt_error();
}

/* The outer scope: the inner scope's answer, then what its field reaches. */
static mt_status outer(mt_call *call, void *user)
{
    (void)user;
    mt_space *store;
    mt_atom *store_ref = new_space(&store);
    mt_atom *root = mt_first(mt_eval(engine, E("scope", E("c-grain-inner", mt_keep(store_ref)))));
    mt_atom *field = root ? mt_first(mt_match(store, E("field", mt_keep(root), V("current")))) : NULL;
    mt_status s = MT_FAIL;
    if (field) {
        mt_space *child = space_of(mt_at(field, 2));
        mt_list values = mt_all(mt_match(child, E("payload", V("value"))));
        mt_atom **found = malloc((values.len + 1) * sizeof *found);
        require("room for the payloads", found != NULL);
        for (size_t i = 0; i < values.len; i++) found[i] = mt_keep(mt_at(values.items[i], 1));
        s = mt_answer(call, mt_exprv(values.len, found));
        free(found);
        mt_list_free(values);
        mt_space_close(child);
    }
    mt_drop(field), mt_drop(root), mt_drop(store_ref);
    mt_space_close(store);
    return s;
}

int main(void)
{
    metta *m = engine = open_engine();
    require("import lib_thread", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_thread")))));
    mt_space *points = mt_space_open(m, "&GrainPoint");
    accounts = mt_space_open(m, "&GrainAccount");
    agents = mt_space_open(m, "&GrainAgent");
    require("open the class spaces", points && accounts && agents);
    require("(: GrainPoint ...)", mt_add(points, E(":", "GrainPoint", E("->", "Number", "Number", "GrainPoint"))));
    for (size_t i = 0; i < POINT_FIELDS; i++) require("an accessor row", mt_add(points, accessor_row(i)));
    require("(: GrainAccount ...)", mt_add(accounts, E(":", "GrainAccount", E("->", "Atom", "GrainAccount"))));
    require("its internal heads", mt_add(accounts, E("internal", "owned-by", "_field-owner", "_field-balance")));
    require("(: GrainAgent ...)", mt_add(agents, E(":", "GrainAgent", E("->", "SpaceType", "GrainAgent"))));
    require("its internal heads", mt_add(agents, E("internal", "owned-by", "_field-label")));
    const struct { const char *name; size_t arity; method method; enum mt_effect_class effect; } methods[] = {
        { "GrainAccount-balance", 1, ACCOUNT_BALANCE, MT_EFFECT_CLASS_READ_ONLY_LOOKUP }, { "GrainAccount-balance!", 2, ACCOUNT_SET, MT_EFFECT_CLASS_WRITES_STATE },
        { "retire-GrainAccount", 1, ACCOUNT_RETIRE, MT_EFFECT_CLASS_WRITES_STATE },  { "make-GrainAccount", 2, ACCOUNT_MAKE, MT_EFFECT_CLASS_WRITES_STATE },
        { "GrainAgent-label", 1, AGENT_LABEL, MT_EFFECT_CLASS_READ_ONLY_LOOKUP },         { "make-GrainAgent", 1, AGENT_MAKE, MT_EFFECT_CLASS_WRITES_STATE },
        { "discard", 1, DISCARD, MT_EFFECT_CLASS_WRITES_STATE },
    };
    for (size_t i = 0; i < sizeof methods / sizeof *methods; i++)
        require(methods[i].name, mt_def(m, (mt_op){ .name = methods[i].name, .arity = methods[i].arity, .effect = methods[i].effect,
                                                    .fn = call_method, .user = (void *)(intptr_t)methods[i].method }));
    require("the grains' body", mt_def(m, (mt_op){ .name = "c-grain-grains", .arity = 0, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = grains }));
    require("the deferring body", mt_def(m, (mt_op){ .name = "c-grain-during", .arity = 1, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = during }));
    require("the inner body", mt_def(m, (mt_op){ .name = "c-grain-inner", .arity = 1, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = inner }));
    require("the outer body", mt_def(m, (mt_op){ .name = "c-grain-outer", .arity = 0, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = outer }));

    check_answers("the grains, in a scope", mt_eval(m, E("scope", E("c-grain-grains"))), B(true));

    mt_atom *rows_ref = mt_first(mt_eval(m, E("new-space")));
    require("new-space answers a space", rows_ref != NULL);
    check_answers("the fact is there while the scope runs", mt_eval(m, E("scope", E("c-grain-during", mt_keep(rows_ref)))), B(true));
    mt_space *rows = space_of(rows_ref);
    check_none("and its deferred removal runs as the scope exits", mt_match(rows, E("owned", V("x"))));
    mt_space_close(rows);
    check_answers("drop-space releases the caller's space", mt_eval(m, E("drop-space", rows_ref)), B(true));
    mt_atom *other = mt_first(mt_eval(m, E("new-space")));
    require("new-space answers a space", other != NULL);
    check_answers("and space_drop is its longhand", mt_eval(m, E("space_drop", other)), B(true));

    const int64_t payload = 9;
    check_answers("a scope's answer keeps what its dependency query reaches", mt_eval(m, E("scope", E("c-grain-outer"))), E(N(payload)));
    mt_space_close(points), mt_space_close(accounts), mt_space_close(agents);
    return done(m);
}
