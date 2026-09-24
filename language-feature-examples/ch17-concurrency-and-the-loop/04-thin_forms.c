/* Purpose: the special forms almost nothing uses, each for the property that
 *   makes it a special form, with C computing what each should answer. The
 *   original's three definitions are C functions: tx-three a generator over a
 *   C array, spin a C countdown, and tx-body a term C builds, which the
 *   engine takes as data because a C function's answer is not evaluated
 *   again, what the original's noeval asks of its equation [measured
 *   2026-09-24: a published function answering (+ 1 2) answers (+ 1 2)].
 *   atomically therefore runs the computed body to its two sums, and
 *   transaction hands the term back unrun. elapsed is held against C's own
 *   bracket on the clock it reads, get_time's realtime clock
 *   [source: engine/metta/runtime.pl, metta_elapsed/3;
 *   commit=e165f80baec1f9d1127f04606c6bbd2283bbc781], so its seconds lie
 *   between zero and what C measured around the call. What call and
 *   translatePredicate sort, C sorts with qsort, and a hyperpose's answers,
 *   which arrive in completion order, are sorted by C too.
 * Guarantees: all twenty-five claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <time.h>

static const int64_t three[] = { 1, 2, 3 };
#define THREE (sizeof three / sizeof *three)

/* tx-three's answers, one per element of three. */
typedef struct cursor {
    size_t next;
} cursor;

static mt_status next_of_three(void *state, mt_atom **answer)
{
    cursor *c = state;
    if (c->next == THREE) return *answer = NULL, MT_DONE;
    *answer = N(three[c->next++]);
    return MT_ROW;
}

static mt_status tx_three(mt_call *call, void *user)
{
    (void)user;
    cursor *c = calloc(1, sizeof *c);
    if (!c) return mt_error_set(MT_NOMEM, "tx-three has no room for its cursor");
    return mt_answer_iter(call, (mt_iterator){ c, next_of_three, free });
}

/* The sums tx-body's superpose holds, as C pairs. */
static const int64_t addends[][2] = { { 1, 1 }, { 2, 2 } };
#define SUMS (sizeof addends / sizeof *addends)

static mt_atom *body_term(void)
{
    mt_atom *sums[SUMS];
    for (size_t i = 0; i < SUMS; i++) sums[i] = E("+", addends[i][0], addends[i][1]);
    return E("superpose", mt_exprv(SUMS, sums));
}

static mt_status tx_body(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, body_term());
}

static mt_status spin(mt_call *call, void *user)
{
    (void)user;
    for (volatile int64_t n = mt_int(mt_arg(call, 0)); n > 0; n--) {}
    return mt_answer(call, S("done"));
}

static int by_value(const void *a, const void *b) { return (*(const int64_t *)a > *(const int64_t *)b) - (*(const int64_t *)a < *(const int64_t *)b); }

/* The expression of the numbers, in order. */
static mt_atom *numbers(const int64_t *xs, size_t n)
{
    mt_atom **items = malloc((n + 1) * sizeof *items);
    require("room for the numbers", items != NULL);
    for (size_t i = 0; i < n; i++) items[i] = N(xs[i]);
    mt_atom *out = mt_exprv(n, items);
    free(items);
    return out;
}

/* (head item...), keeping the items. */
static mt_atom *headed(const char *head, mt_atom *const *items, size_t n)
{
    mt_atom **all = malloc((n + 1) * sizeof *all);
    require("room for the form", all != NULL);
    all[0] = S(head);
    for (size_t i = 0; i < n; i++) all[i + 1] = mt_keep(items[i]);
    mt_atom *out = mt_exprv(n + 1, all);
    free(all);
    return out;
}

static double now(void)
{
    struct timespec t;
    require("the realtime clock", clock_gettime(CLOCK_REALTIME, &t) == 0);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int main(void)
{
    metta *m = open_engine();
    const mt_op ops[] = {
        { .name = "tx-three", .arity = 0, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = tx_three },
        { .name = "tx-body", .arity = 0, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = tx_body },
        { .name = "spin", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = spin },
    };
    for (size_t i = 0; i < sizeof ops / sizeof *ops; i++) require(ops[i].name, mt_def(m, ops[i]));

    /* test-no-answer: no answer is not one answer that is (). */
    check_none("superpose of nothing answers nothing", mt_eval(m, E("superpose", mt_unit())));
    check_answers("which collapse gathers into ()", mt_eval(m, E("collapse", E("superpose", mt_unit()))), mt_unit());
    check_answers("while () is one answer", mt_eval(m, E("collapse", mt_unit())), E(mt_unit()));

    /* prog1 answers its first form and progn its last. */
    const int64_t forms[][2] = { { 1, 1 }, { 2, 2 }, { 3, 3 } };
    const size_t n_forms = sizeof forms / sizeof *forms;
    mt_atom *sequence[sizeof forms / sizeof *forms];
    for (size_t i = 0; i < n_forms; i++) sequence[i] = E("+", forms[i][0], forms[i][1]);
    check_answers("prog1", mt_eval(m, headed("prog1", sequence, n_forms)), N(forms[0][0] + forms[0][1]));
    check_answers("progn", mt_eval(m, headed("progn", sequence, n_forms)), N(forms[n_forms - 1][0] + forms[n_forms - 1][1]));
    for (size_t i = 0; i < n_forms; i++) mt_drop(sequence[i]);

    /* transaction: a failed body undoes its writes, a good one keeps them,
       and every answer comes back. */
    check_answers("a failing transaction answers nothing",
                  mt_eval(m, E("collapse", E("transaction", E("progn", E("add-atom", "&self", E("tx-rolled", "a")), E("superpose", mt_unit()))))),
                  mt_unit());
    check_none("and its write is undone", mt_match(m, E("tx-rolled", V("x"))));
    check_answers("a good one answers its body's True", mt_eval(m, E("collapse", E("transaction", E("add-atom", "&self", E("tx-kept", "a"))))),
                  E(B(true)));
    check_answers("and keeps its write", mt_match(m, E("tx-kept", V("x"))), E("tx-kept", "a"));
    check_answers("every answer of the body", mt_eval(m, E("collapse", E("transaction", E("tx-three")))), numbers(three, THREE));
    const int64_t each[] = { 1, 2 };
    const size_t n_each = sizeof each / sizeof *each;
    mt_atom *writes[sizeof each / sizeof *each], *trues[sizeof each / sizeof *each];
    for (size_t i = 0; i < n_each; i++) writes[i] = E("add-atom", "&self", E("tx-each", each[i])), trues[i] = B(true);
    check_answers("one True per write", mt_eval(m, E("collapse", E("transaction", E("superpose", mt_exprv(n_each, writes))))),
                  mt_exprv(n_each, trues));
    mt_atom *landed[sizeof each / sizeof *each];
    for (size_t i = 0; i < n_each; i++) landed[i] = E("tx-each", each[i]);
    check_answers_("and every write lands", mt_match(m, E("tx-each", V("x"))), n_each, landed);

    /* atomically: the same guarantees over a body the program computed. */
    check_answers("atomically answers every answer too", mt_eval(m, E("collapse", E("atomically", E("tx-three")))), numbers(three, THREE));
    int64_t sums[SUMS];
    for (size_t i = 0; i < SUMS; i++) sums[i] = addends[i][0] + addends[i][1];
    check_answers("atomically runs a computed body", mt_eval(m, E("collapse", E("let", V("b"), E("tx-body"), E("atomically", V("b"))))),
                  numbers(sums, SUMS));
    mt_list ran = mt_all(mt_eval(m, E("let", V("b"), E("tx-body"), E("atomically", V("b")))));
    check_int("one answer per sum", (int64_t)ran.len, (int64_t)SUMS);
    mt_list_free(ran);
    mt_list held = mt_all(mt_eval(m, E("let", V("b"), E("tx-body"), E("transaction", V("b")))));
    mt_atom *term = body_term();
    check("transaction answers the term unrun", held.len == 1 && mt_alpha_eq(held.items[0], term));
    mt_drop(term);
    mt_list_free(held);

    /* elapsed: the value and the seconds it took. */
    double before = now();
    mt_atom *timed = mt_first(mt_eval(m, E("elapsed", E("+", 1, 2))));
    double bracket = now() - before;
    require("elapsed answers a pair", timed && mt_kind_of(timed) == MT_EXPR && mt_len(timed) == 2);
    check_atom("elapsed keeps the value", mt_keep(mt_at(timed, 0)), N(1 + 2));
    double seconds = mt_float(mt_at(timed, 1));
    check("and its seconds lie inside C's bracket", seconds >= 0 && seconds <= bracket);
    mt_drop(timed);

    /* A bound that does not fire leaves the answer alone. */
    check_answers("timeout", mt_eval(m, E("timeout", 5, E("spin", 10))), S("done"));

    /* Named locks, and hyperpose's concurrency. */
    check_answers("with_mutex", mt_eval(m, E("with_mutex", "thin-lock-a", E("+", 1, 2))), N(1 + 2));
    check_answers("under another name", mt_eval(m, E("with_mutex", "thin-lock-b", E("+", 2, 2))), N(2 + 2));
    check_answers("once takes the first branch done", mt_eval(m, E("once", E("hyperpose", E(E("spin", 3000000), E("spin", 3))))),
                  S("done"));
    mt_atom *branches[SUMS], *sorted_sums[SUMS];
    for (size_t i = 0; i < SUMS; i++) branches[i] = E("+", addends[i][0], addends[i][1]), sorted_sums[i] = N(sums[i]);
    mt_list both = mt_all(mt_eval(m, E("hyperpose", mt_exprv(SUMS, branches))));
    qsort(both.items, both.len, sizeof *both.items, mt_order);
    qsort(sorted_sums, SUMS, sizeof *sorted_sums, mt_order);
    check_list_("every branch answers", both, SUMS, sorted_sums);

    /* A Prolog predicate with no registration, by call and inline. */
    int64_t unsorted[] = { 3, 1, 2 }, in_order[sizeof unsorted / sizeof *unsorted];
    const size_t n_unsorted = sizeof unsorted / sizeof *unsorted;
    memcpy(in_order, unsorted, sizeof unsorted);
    qsort(in_order, n_unsorted, sizeof *in_order, by_value);
    check_answers("call reaches msort", mt_eval(m, E("call", E("msort", numbers(unsorted, n_unsorted)))), numbers(in_order, n_unsorted));
    check_answers("translatePredicate compiles it inline",
                  mt_eval(m, E("progn", E("translatePredicate", E("msort", numbers(unsorted, n_unsorted), V("s"))), V("s"))),
                  numbers(in_order, n_unsorted));
    return done(m);
}
