/* Purpose: a million atoms and five questions of different shapes. addK is a
 *   C loop building (r K (mod K 10)) for K from the count down to 1, in the
 *   order the original's recursion writes them, stored by one mt_add_all
 *   batch, the bulk door a C loader takes where the original adds one atom a
 *   step. The five questions stay the original's equations, built as atoms:
 *   a collapse over a match is the engine's own work, and a C
 *   function walking the match's cursor to collect it costs 4.4 seconds for
 *   the million where the engine's collapse costs 0.5. indexing-demo is a C
 *   function asking the engine for each question's length and building the
 *   report. The counts C expects are C's own, counted over the loader's
 *   definition: each question is a predicate on K, and C counts the Ks that
 *   satisfy it.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

/* The relation q-rel's pattern fixes the rest of: ($r 643 3). Macros, so the
   equation and the C predicate read one number. */
#define REL_FIRST 643
#define REL_SECOND 3

/* The original counts down to 0, which a negative count never reaches: its
   recursion runs out of stack, and this refuses by name instead. */
static mt_status add_k(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 0)) != MT_INT) return MT_FAIL;
    const int64_t count = mt_int(mt_arg(call, 0));
    if (count < 0) return mt_fail(call, "addK counts down to 0, which a negative count never reaches");
    mt_list batch = { mt_alloc((size_t)count * sizeof *batch.items), (size_t)count };
    if (count > 0 && !batch.items) return mt_error_set(MT_NOMEM, "addK has no room for its batch");
    for (int64_t k = count; k > 0; k--) batch.items[count - k] = E("r", k, floor_mod(k, 10));
    return mt_add_all(mt_of(call), batch) ? mt_answer(call, S("done")) : mt_error();
}

/* What the demo asks, in the order its report names them: the label, the
   call, and what the call fixes of (r K (mod K 10)), ANY fixing nothing. */
enum { ANY = -1 };
typedef struct asked {
    const char *label;
    mt_atom *call;
    int64_t first, second;
} asked;

static size_t questions(asked out[5])
{
    out[0] = (asked){ "all:", E("q-all"), ANY, ANY };
    out[1] = (asked){ "first:", E("q-first", 7), 7, ANY };
    out[2] = (asked){ "second:", E("q-second", 3), ANY, 3 };
    out[3] = (asked){ "rel:", E("q-rel", "r"), REL_FIRST, REL_SECOND };
    out[4] = (asked){ "both:", E("q-both", 42, 2), 42, 2 };
    return 5;
}

/* The report (all: n first: n second: n rel: n both: n), one count per
   question. */
static mt_atom *report(const asked *q, const int64_t *counts, size_t n)
{
    mt_atom *items[10];
    for (size_t i = 0; i < n; i++) {
        items[2 * i] = S(q[i].label);
        items[2 * i + 1] = N(counts[i]);
    }
    return mt_exprv(2 * n, items);
}

static mt_status indexing_demo(mt_call *call, void *user)
{
    (void)user;
    metta *m = mt_of(call);
    mt_atom *loaded = mt_first(mt_eval(m, E("addK", mt_keep(mt_arg(call, 0)))));
    if (!loaded) return mt_error();
    mt_drop(loaded);
    asked q[5];
    size_t n = questions(q);
    int64_t counts[5];
    bool answered = true;
    for (size_t i = 0; i < n; i++) {
        if (!answered) {
            mt_drop(q[i].call);
            continue;
        }
        mt_atom *length = mt_first(mt_eval(m, E("length", q[i].call)));
        answered = length != NULL && mt_kind_of(length) == MT_INT;
        counts[i] = answered ? mt_int(length) : 0;
        mt_drop(length);
    }
    return answered ? mt_answer(call, report(q, counts, n)) : mt_error();
}

/* How many Ks from 1 to count the question admits, which is what its match
   must find among the loader's atoms. Time: count comparisons. */
static int64_t admitted(const asked *q, int64_t count)
{
    int64_t found = 0;
    for (int64_t k = 1; k <= count; k++)
        found += (q->first == ANY || q->first == k) && (q->second == ANY || q->second == floor_mod(k, 10));
    return found;
}

int main(void)
{
    metta *m = open_engine();
    require("addK", mt_def(m, (mt_op){ .name = "addK", .arity = 1, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = add_k }));
    require("q-all", mt_add(m, E("=", E("q-all"), E("collapse", E("match", "&self", E("r", V("x"), V("y")), E("r", V("x"), V("y")))))));
    require("q-first", mt_add(m, E("=", E("q-first", V("a")), E("collapse", E("match", "&self", E("r", V("a"), V("y")), E("r", V("a"), V("y")))))));
    require("q-second", mt_add(m, E("=", E("q-second", V("b")), E("collapse", E("match", "&self", E("r", V("x"), V("b")), E("r", V("x"), V("b")))))));
    require("q-both", mt_add(m, E("=", E("q-both", V("a"), V("b")), E("collapse", E("match", "&self", E("r", V("a"), V("b")), E("r", V("a"), V("b")))))));
    require("q-rel", mt_add(m, E("=", E("q-rel", V("r")),
                                E("collapse", E("match", "&self", E(V("r"), REL_FIRST, REL_SECOND), E(V("r"), REL_FIRST, REL_SECOND))))));
    require("indexing-demo", mt_def(m, (mt_op){ .name = "indexing-demo", .arity = 1,
                                                .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = indexing_demo }));

    const int64_t count = 1000000;
    asked q[5];
    size_t n = questions(q);
    int64_t counts[5];
    for (size_t i = 0; i < n; i++) {
        counts[i] = admitted(&q[i], count);
        mt_drop(q[i].call);
    }
    check_answers("a million atoms, five questions", mt_eval(m, E("indexing-demo", count)), report(q, counts, n));
    return done(m);
}
