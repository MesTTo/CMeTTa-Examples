/* Purpose: an admission pool, a space whose pre-add hook is claimed by a
 *   judge over the (admits <pool> <type>) and (capacity <pool> <n>) contract
 *   atoms in &metta. The engine ships that judge as space-admission-verdict,
 *   the original writes it again in MeTTa as its specification, and this
 *   twin writes it in C: one C function per head of the MeTTa chain,
 *   published under that head, with each recursion a loop. An atom must carry
 *   every admitted type, which C asks the engine's has-declared-type, the
 *   witness reading the builtin uses. The pool must stay under every capacity
 *   row, counted with mt_count, the store's own count. The chain's type
 *   declarations mask the judged atom for a C operation as for an equation,
 *   so the pool judges the offered atom as itself. metta-pool-guard is the
 *   judge closed over &metta-pool. A refused write raises the Error C builds
 *   from its own verdict, and the builtin answers C's verdict for every atom
 *   asked, a pool with two capacity rows included, where the original's
 *   chain reads only the first row [measured 2026-09-24: the chain's
 *   definitions over rows 5 and 2 and three held atoms answer (accept), the
 *   builtin (refuse (pool-at-capacity 2))].
 * Guarantees: all seven claims of the original hold, with its six
 *   unasserted forms checked as well [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "verdicts.h"

typedef struct judge {
    metta *m;
    mt_space *catalog;
} judge;

/* Each x of the (head pool x) rows the catalog holds, in order. */
static mt_atom *contract(const judge *j, const char *head, const mt_atom *pool)
{
    mt_list rows = mt_all(mt_match(j->catalog, E(head, mt_keep(pool), V("x"))));
    mt_atom **xs = malloc((rows.len + 1) * sizeof *xs);
    require("room for the contract", xs != NULL);
    for (size_t i = 0; i < rows.len; i++) xs[i] = mt_keep(mt_at(rows.items[i], 2));
    mt_atom *out = mt_exprv(rows.len, xs);
    free(xs);
    mt_list_free(rows);
    return out;
}

/* metta-admission-within: refuse at the first limit the pool's count has
   reached, or accept. */
static mt_atom *within(const judge *j, const mt_atom *pool, const mt_atom *limits)
{
    mt_space *space = mt_space_open(j->m, mt_name(pool));
    if (!space) return NULL;
    int64_t count = (int64_t)mt_count(space);
    mt_space_close(space);
    for (size_t i = 0; i < mt_len(limits); i++)
        if (count >= mt_int(mt_at(limits, i))) return refusing(E("pool-at-capacity", mt_keep(mt_at(limits, i))));
    return accepting(NULL);
}

/* metta-admission-bounded: the pool's capacity rows decide. */
static mt_atom *bounded(const judge *j, const mt_atom *pool)
{
    mt_atom *limits = contract(j, "capacity", pool);
    mt_atom *verdict = within(j, pool, limits);
    mt_drop(limits);
    return verdict;
}

/* metta-admission-typed: refuse at the first type the atom does not carry;
   past every type, the bound decides. NULL when the engine failed. */
static mt_atom *typed(const judge *j, const mt_atom *pool, const mt_atom *atom, const mt_atom *types)
{
    for (size_t i = 0; i < mt_len(types); i++) {
        const mt_atom *type = mt_at(types, i);
        mt_clear();
        bool carried = mt_one_truth(mt_eval(j->m, E("has-declared-type", mt_keep(atom), mt_keep(type))));
        if (!carried && !mt_ok()) return NULL;
        if (!carried) return refusing(E("does-not-carry", mt_keep(type)));
    }
    return bounded(j, pool);
}

/* metta-admission-verdict: the pool's admitted types decide first. */
static mt_atom *verdict(const judge *j, const mt_atom *pool, const mt_atom *atom)
{
    mt_atom *types = contract(j, "admits", pool);
    mt_atom *out = typed(j, pool, atom, types);
    mt_drop(types);
    return out;
}

static mt_status answer(mt_call *call, mt_atom *verdict) { return verdict ? mt_answer(call, verdict) : mt_error(); }
static mt_status verdict_op(mt_call *call, void *j) { return answer(call, verdict(j, mt_arg(call, 0), mt_arg(call, 1))); }
static mt_status typed_op(mt_call *call, void *j) { return answer(call, typed(j, mt_arg(call, 0), mt_arg(call, 1), mt_arg(call, 2))); }
static mt_status bounded_op(mt_call *call, void *j) { return answer(call, bounded(j, mt_arg(call, 0))); }
static mt_status within_op(mt_call *call, void *j) { return answer(call, within(j, mt_arg(call, 0), mt_arg(call, 1))); }

/* The judge closed over one pool: (metta-pool-guard $incoming). */
typedef struct guard {
    const judge *judge;
    mt_atom *pool;
} guard;

static mt_status guard_op(mt_call *call, void *user)
{
    const guard *g = user;
    return answer(call, verdict(g->judge, g->pool, mt_arg(call, 0)));
}

/* A write the judge refused, as catch answers it. TAKES the verdict. */
static mt_atom *refused(const mt_atom *pool, const mt_atom *atom, mt_atom *verdict)
{
    mt_atom *out = verdict ? E("Error", E("metta_add_refused", mt_keep(pool), mt_keep(atom), mt_keep(mt_at(verdict, 1))), "none") : NULL;
    mt_drop(verdict);
    return out;
}

/* The shipped builtin answers C's verdict. */
static void check_agrees(const judge *j, const mt_atom *pool, mt_atom *atom)
{
    check_answers("the builtin answers C's verdict", mt_eval(j->m, E("space-admission-verdict", mt_keep(pool), mt_keep(atom))),
                  verdict(j, pool, atom));
    mt_drop(atom);
}

int main(void)
{
    metta *m = open_engine();
    judge j = { .m = m, .catalog = mt_catalog(m) };
    mt_space *pool = mt_space_open(m, "&metta-pool");
    require("open &metta-pool", pool != NULL);
    mt_atom *ref = mt_spaceref(mt_space_name(pool));
    guard g = { .judge = &j, .pool = ref };
    const mt_op ops[] = {
        { .name = "metta-admission-verdict", .arity = 2, .effect = MT_EFFECT_CLASS_READ_ONLY_LOOKUP, .fn = verdict_op, .user = &j },
        { .name = "metta-admission-typed", .arity = 3, .effect = MT_EFFECT_CLASS_READ_ONLY_LOOKUP, .fn = typed_op, .user = &j },
        { .name = "metta-admission-bounded", .arity = 1, .effect = MT_EFFECT_CLASS_READ_ONLY_LOOKUP, .fn = bounded_op, .user = &j },
        { .name = "metta-admission-within", .arity = 2, .effect = MT_EFFECT_CLASS_READ_ONLY_LOOKUP, .fn = within_op, .user = &j },
        { .name = "metta-pool-guard", .arity = 1, .effect = MT_EFFECT_CLASS_READ_ONLY_LOOKUP, .fn = guard_op, .user = &g },
    };
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++) require(ops[i].name, mt_def(m, ops[i]));
    require("mask verdict's atom", mt_add(m, E(":", "metta-admission-verdict", E("->", "%Undefined%", "Atom", "%Undefined%"))));
    require("mask typed's atom", mt_add(m, E(":", "metta-admission-typed", E("->", "%Undefined%", "Atom", "%Undefined%", "%Undefined%"))));

    const char *admitted = "Ticket";
    const int64_t capacity = 2;
    check("the pool admits Tickets", mt_add(j.catalog, E("admits", mt_keep(ref), admitted)));
    check("and holds at most two", mt_add(j.catalog, E("capacity", mt_keep(ref), capacity)));
    const char *tickets[] = { "a", "b" };
    for (size_t i = 0; i < sizeof tickets / sizeof *tickets; i++)
        require("declare a ticket", mt_add(m, E(":", E("ticket", tickets[i]), admitted)));
    check_answers("the guard claims the write door", mt_eval(m, E("declare-pre-add!", mt_keep(ref), "metta-pool-guard")), mt_unit());

    mt_atom *first = E("ticket", tickets[0]), *stowaway = E("stowaway", 1);
    check("a Ticket enters", mt_add(pool, mt_keep(first)));
    check_answers("and lands", mt_match(pool, E("ticket", V("x"))), mt_keep(first));
    check_answers("an atom carrying no Ticket is refused", mt_eval(m, E("catch", E("add-atom", mt_keep(ref), mt_keep(stowaway)))),
                  refused(ref, stowaway, verdict(&j, ref, stowaway)));
    check("the second Ticket fills the pool", mt_add(pool, E("ticket", tickets[1])));
    check_answers("so a third is refused", mt_eval(m, E("catch", E("add-atom", mt_keep(ref), mt_keep(first)))),
                  refused(ref, first, verdict(&j, ref, first)));

    check_agrees(&j, ref, mt_keep(stowaway));
    check_agrees(&j, ref, mt_keep(first));
    check("the capacity row goes", mt_del(j.catalog, E("capacity", mt_keep(ref), capacity)));
    check_agrees(&j, ref, mt_keep(first));
    check_answers("and with no bound the builtin admits a Ticket", mt_eval(m, E("space-admission-verdict", mt_keep(ref), mt_keep(first))),
                  accepting(NULL));

    /* Every capacity row binds, not only the first. */
    const int64_t limits[] = { capacity + 1, capacity };
    for (size_t i = 0; i < sizeof limits / sizeof *limits; i++)
        require("a capacity row", mt_add(j.catalog, E("capacity", mt_keep(ref), limits[i])));
    check_agrees(&j, ref, mt_keep(first));
    for (size_t i = 0; i < sizeof limits / sizeof *limits; i++)
        require("remove it", mt_del(j.catalog, E("capacity", mt_keep(ref), limits[i])));

    mt_drop(first), mt_drop(stowaway), mt_drop(ref);
    mt_space_close(pool);
    return done(m);
}
