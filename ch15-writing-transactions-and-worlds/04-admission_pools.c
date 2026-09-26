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
 *   asked. With two capacity rows the first the count reaches decides,
 *   which C's within() walks every row for and the original's chain does
 *   since superproject 47855fa71 [source: examples 3372c22,
 *   metta-admission-within].
 * Guarantees: all nine claims of the original hold, with its ten
 *   unasserted forms checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

/* The verdict that lets the offered atom in, or `as` in its place; takes `as`. */
static inline mt_atom *accepting(mt_atom *as) { return as ? mt_expr("Accept", as) : mt_expr("Accept"); }

/* The verdict that raises with the words; takes them. */
static inline mt_atom *refusing(mt_atom *words) { return mt_expr("Refuse", words); }

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
    assert(answers_are(mt_eval(j->m, E("space-admission-verdict", mt_keep(pool), mt_keep(atom))), E(verdict(j, pool, atom)))
           && "the builtin answers C's verdict");
    mt_drop(atom);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(mt_add(j.catalog, E("admits", mt_keep(ref), admitted)) && "the pool admits Tickets");
    assert(mt_add(j.catalog, E("capacity", mt_keep(ref), capacity)) && "and holds at most two");
    const char *tickets[] = { "a", "b" };
    for (size_t i = 0; i < sizeof tickets / sizeof *tickets; i++)
        require("declare a ticket", mt_add(m, E(":", E("ticket", tickets[i]), admitted)));
    assert(answers_are(mt_eval(m, E("declare-pre-add!", mt_keep(ref), "metta-pool-guard")), E(mt_unit())) && "the guard claims the write door");

    mt_atom *first = E("ticket", tickets[0]), *stowaway = E("stowaway", 1);
    assert(mt_add(pool, mt_keep(first)) && "a Ticket enters");
    assert(answers_are(mt_match(pool, E("ticket", V("x"))), E(mt_keep(first))) && "and lands");
    assert(answers_are(mt_eval(m, E("catch", E("add-atom", mt_keep(ref), mt_keep(stowaway)))), E(refused(ref, stowaway, verdict(&j, ref, stowaway))))
           && "an atom carrying no Ticket is refused");
    assert(mt_add(pool, E("ticket", tickets[1])) && "the second Ticket fills the pool");
    assert(answers_are(mt_eval(m, E("catch", E("add-atom", mt_keep(ref), mt_keep(first)))), E(refused(ref, first, verdict(&j, ref, first))))
           && "so a third is refused");

    check_agrees(&j, ref, mt_keep(stowaway));
    check_agrees(&j, ref, mt_keep(first));
    assert(mt_del(j.catalog, E("capacity", mt_keep(ref), capacity)) && "the capacity row goes");
    check_agrees(&j, ref, mt_keep(first));
    assert(answers_are(mt_eval(m, E("space-admission-verdict", mt_keep(ref), mt_keep(first))), E(accepting(NULL)))
           && "and with no bound the builtin admits a Ticket");

    /* Every capacity row binds, not only the first: the rows 5 then 2 the
       original adds, over the two tickets held, so the second decides. */
    const int64_t limits[] = { 5, capacity };
    for (size_t i = 0; i < sizeof limits / sizeof *limits; i++)
        assert(mt_add(j.catalog, E("capacity", mt_keep(ref), limits[i])) && "a capacity row");
    check_agrees(&j, ref, mt_keep(first));
    assert(answers_are(mt_eval(m, E("metta-admission-verdict", mt_keep(ref), mt_keep(first))), E(refusing(E("pool-at-capacity", capacity))))
           && "the chain refuses at the row the count reaches");
    for (size_t i = 0; i < sizeof limits / sizeof *limits; i++)
        assert(mt_del(j.catalog, E("capacity", mt_keep(ref), limits[i])) && "and it goes");

    mt_drop(first), mt_drop(stowaway), mt_drop(ref);
    mt_space_close(pool);
    mt_close(m);
    return 0;
}
