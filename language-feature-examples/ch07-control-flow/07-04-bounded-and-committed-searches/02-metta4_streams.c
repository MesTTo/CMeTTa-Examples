/* Purpose: answers as a stream. range is a C generator counting from K to
 *   N; C loops over its lazy cursor to write every answer into &s1, and
 *   mt_first commits to one answer for &s2, as once does. gen answers three
 *   values from a C array, and foldall's sum is C's sum over the same
 *   cursor.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    require("publish range", mt_def(m, (mt_op){ .name = "range", .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = range }));
    require("publish gen", mt_def(m, (mt_op){ .name = "gen", .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = gen }));
    mt_space *s1 = mt_space_open(m, "&s1"), *s2 = mt_space_open(m, "&s2");
    require("open &s1 and &s2", s1 && s2);

    mt_each (x, mt_eval(m, E("range", 1, 5))) require("(num x) into &s1", mt_add(s1, E("num", mt_keep(x))));
    require("(num first) into &s2", mt_add(s2, E("num", mt_first(mt_eval(m, E("range", 1, 5))))));

    check_answers("every answer went to &s1", mt_atoms(s1), E("num", 1), E("num", 2), E("num", 3), E("num", 4));
    check_answers("one committed answer to &s2", mt_atoms(s2), E("num", 1));

    int64_t sum = 0;
    mt_each (v, mt_eval(m, E("gen"))) sum += mt_int(v);
    check_answers("foldall sums what C sums", mt_eval(m, E("foldall", E("|->", E(V("x"), V("y")), E("+", V("x"), V("y"))), E("gen"), 0)), sum);
    mt_space_close(s1);
    mt_space_close(s2);
    return done(m);
}
