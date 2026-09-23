/* Purpose: evolve C bit genomes while MeTTa owns population facts and stopping.
 * Owns resources: each generation drains its old fact snapshot and replaces it.
 * Decides: deterministic xorshift variation and elitism make the run reproducible.
 * Guarantees: fitness improves and population size is preserved [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct search { uint32_t random; unsigned generations; } search;
static unsigned fitness(unsigned genome)
{
    unsigned bits = genome ^ 0xb5u, wrong = 0;
    for (; bits; bits >>= 1) wrong += bits & 1u;
    return 8 - wrong;
}
static mt_status score(mt_call *call, void *user)
{ (void)user; return mt_answer(call, mt_num(fitness((unsigned)mt_int(mt_arg(call, 0))))); }
/* O(P + B), P population size, B genome bits; one elite parent plus mutants. */
static mt_status generation(mt_call *call, void *user)
{
    search *s = user; metta *m = mt_of(call);
    mt_list rows = mt_all(mt_match(m, mt_parse("(member $id $genome)")));
    if (!mt_ok()) return mt_error();
    unsigned best = 0;
    for (size_t i = 0; i < rows.len; ++i) {
        unsigned genome = (unsigned)mt_int(mt_at(rows.items[i], 2));
        if (fitness(genome) > fitness(best)) best = genome;
        if (!mt_del(m, mt_keep(rows.items[i]))) { mt_list_free(rows); return mt_error(); }
    }
    size_t count = rows.len; mt_list_free(rows);
    for (size_t i = 0; i < count; ++i) {
        s->random ^= s->random << 13; s->random ^= s->random >> 17; s->random ^= s->random << 5;
        unsigned genome = i == 0 ? best : best ^ (1u << (s->random % 8));
        if (!mt_add(m, mt_expr("member", (int64_t)i, (int64_t)genome))) return mt_error();
    }
    ++s->generations; return mt_answer(call, mt_bool(true));
}
int main(void)
{
    metta *m = open_engine(); search s = {11,0};
    check("fitness operation", mt_def(m, (mt_op){.name="fitness", .arity=1, .effect=MT_PURE, .fn=score}));
    check("generation operation", mt_def(m, (mt_op){.name="next-generation", .arity=0, .effect=MT_WRITES, .fn=generation, .user=&s}));
    for (int i = 0; i < 16; ++i) check("initial population", mt_add(m, mt_expr("member", i, i)));
    check("stopping rule", mt_do(m,
      "(= (best) (max-atom (collapse (match &self (member $i $g) (fitness $g))))) "
      "(= (evolve $n) (if (== (best) 8) $n (if (> $n 60) stopped (let $step (next-generation) (evolve (+ $n 1))))))"));
    int64_t generations = mt_one_int(mt_eval(m, mt_expr("evolve", 0)));
    check("perfect genome reached", mt_ok() && generations > 0 && (uint64_t)generations == s.generations);
    check_answers("best score", mt_run(m, "!(best)"), "8");
    mt_list population = mt_all(mt_match(m, mt_parse("(member $id $g)")));
    check("population size preserved", population.len == 16); mt_list_free(population);
    check("withdraw variation", mt_undef(m, "next-generation")); check("withdraw scoring", mt_undef(m, "fitness"));
    return done(m, "evolutionary_search");
}
