/* Purpose: answers as a stream. range is a C generator counting from K to
 *   N; C loops over its lazy cursor to write every answer into &s1, and
 *   mt_first commits to one answer for &s2, as once does. gen answers three
 *   values from a C array, and foldall's sum is C's sum over the same
 *   cursor.
 * Guarantees: all three claims of the original hold
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

typedef struct { int64_t next, end; } counter;
static mt_status count_next(void *state, mt_atom **answer)
{
    counter *c = state;
    if (c->next >= c->end) { *answer = NULL; return MT_DONE; }
    *answer = N(c->next++);
    return MT_ROW;
}
static void free_counter(void *state) { free(state); }

/* (range K N): K, K+1, ..., N-1, one answer at a time. */
static mt_status range(mt_call *call, void *user)
{
    (void)user;
    counter *c = malloc(sizeof *c);
    if (!c) return mt_fail(call, "no memory for range's answers");
    *c = (counter){ mt_int(mt_arg(call, 0)), mt_int(mt_arg(call, 1)) };
    return mt_answer_iter(call, (mt_iterator){ c, count_next, free_counter });
}

/* (gen): 1, 2, 3. */
static mt_status gen(mt_call *call, void *user)
{
    (void)user;
    counter *c = malloc(sizeof *c);
    if (!c) return mt_fail(call, "no memory for gen's answers");
    *c = (counter){ 1, 4 };
    return mt_answer_iter(call, (mt_iterator){ c, count_next, free_counter });
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("publish range", mt_def(m, (mt_op){ .name = "range", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = range }));
    require("publish gen", mt_def(m, (mt_op){ .name = "gen", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = gen }));
    mt_space *s1 = mt_space_open(m, "&s1"), *s2 = mt_space_open(m, "&s2");
    require("open &s1 and &s2", s1 && s2);

    mt_each (x, mt_eval(m, E("range", 1, 5))) require("(num x) into &s1", mt_add(s1, E("num", mt_keep(x))));
    require("(num first) into &s2", mt_add(s2, E("num", mt_first(mt_eval(m, E("range", 1, 5))))));

    assert(answers_are(mt_atoms(s1), E(E("num", 1), E("num", 2), E("num", 3), E("num", 4))) && "every answer went to &s1");
    assert(answers_are(mt_atoms(s2), E(E("num", 1))) && "one committed answer to &s2");

    int64_t sum = 0;
    mt_each (v, mt_eval(m, E("gen"))) sum += mt_int(v);
    assert(answers_are(mt_eval(m, E("foldall", E("|->", E(V("x"), V("y")), E("+", V("x"), V("y"))), E("gen"), 0)), E(sum)) && "foldall sums what C sums");
    mt_space_close(s1);
    mt_space_close(s2);
    mt_close(m);
    return 0;
}
