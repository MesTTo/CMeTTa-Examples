/* Purpose: C evolves, MeTTa decides when to stop. Genomes are bytes scored
 *   and mutated by C functions; the population lives as (member id genome)
 *   facts; and the stopping rule is two equations the engine evaluates,
 *   calling back into C for the next generation until the best genome is
 *   perfect.
 * Decides: a fixed xorshift seed and elitism make the run reproducible.
 * Guarantees: the search reaches fitness 8 while the population stays 16
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

enum { POPULATION = 16, GENOME_BITS = 8, TARGET = 0xb5 };

typedef struct search { uint32_t random; unsigned generations; } search;

static unsigned fitness(unsigned genome)
{
    unsigned wrong = 0;
    for (unsigned bits = (genome ^ TARGET) & 0xffu; bits; bits >>= 1) wrong += bits & 1u;
    return GENOME_BITS - wrong;
}

static mt_status score(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t genome = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "fitness wants a genome");
    return mt_answer(call, N(fitness((unsigned)genome)));
}

/* Replace the population with the best genome and its one-bit mutants.
   Time O(P * B) for P members and B genome bits. */
static mt_status next_generation(mt_call *call, void *user)
{
    search *s = user;
    metta *m = mt_of(call);
    mt_list members = mt_all(mt_match(m, E("member", V("id"), V("genome"))));
    if (!mt_ok()) return mt_error();
    unsigned best = 0;
    for (size_t i = 0; i < members.len; i++) {
        unsigned genome = (unsigned)mt_int(mt_at(members.items[i], 2));
        if (fitness(genome) > fitness(best)) best = genome;
        if (!mt_del(m, mt_keep(members.items[i]))) { mt_list_free(members); return mt_error(); }
    }
    size_t size = members.len;
    mt_list_free(members);
    for (size_t i = 0; i < size; i++) {
        s->random ^= s->random << 13;
        s->random ^= s->random >> 17;
        s->random ^= s->random << 5;
        unsigned genome = i == 0 ? best : best ^ (1u << (s->random % GENOME_BITS));
        if (!mt_add(m, E("member", (int64_t)i, (int64_t)genome))) return mt_error();
    }
    s->generations++;
    return mt_answer(call, B(true));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    search s = { 11, 0 };
    require("publish fitness", mt_def(m, (mt_op){ .name = "fitness", .arity = 1,
                                                  .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = score }));
    require("publish next-generation", mt_def(m, (mt_op){ .name = "next-generation", .arity = 0,
        .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = next_generation, .user = &s }));
    for (int64_t i = 0; i < POPULATION; i++)
        require("seed the population", mt_add(m, E("member", i, i)));

    /* (= (best) (max-atom (collapse (match &self (member $i $g) (fitness $g))))) */
    require("define best", mt_add(m, E("=", E("best"), E("max-atom",
        E("collapse", E("match", "&self", E("member", V("i"), V("g")), E("fitness", V("g"))))))));
    /* (= (evolve $n) (if (== (best) 8) $n
                          (if (> $n 60) stopped (let $step (next-generation) (evolve (+ $n 1)))))) */
    require("define evolve", mt_add(m, E("=", E("evolve", V("n")),
        E("if", E("==", E("best"), GENOME_BITS), V("n"),
          E("if", E(">", V("n"), 60), "stopped",
            E("let", V("step"), E("next-generation"), E("evolve", E("+", V("n"), 1))))))));

    int64_t generations = mt_one_int(mt_eval(m, E("evolve", 0)));
    assert(mt_ok() && generations > 0 && (uint64_t)generations == s.generations
           && "the search stops at a perfect genome, one C generation per step");
    assert(mt_one_int(mt_eval(m, E("best"))) == GENOME_BITS && "whose fitness is 8");
    mt_list population = mt_all(mt_match(m, E("member", V("id"), V("genome"))));
    assert((int64_t)population.len == POPULATION && "and the population is still 16");
    mt_list_free(population);
    require("withdraw next-generation", mt_undef(m, "next-generation"));
    require("withdraw fitness", mt_undef(m, "fitness"));
    mt_close(m);
    return 0;
}
