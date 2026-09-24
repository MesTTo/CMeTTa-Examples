/* Purpose: what a gap query costs, and why it is not the size of the space.
 *   nodes is a C function storing (node n tag) for n from its count down to
 *   1 through one mt_add_all batch, where the original's recursion adds one
 *   a step. A gap is the language matcher's, which C reaches through
 *   mt_query; mt_match is the primitive stored lookup and reads ... as a
 *   symbol like any other. (inferences 1000 ...) is mt_limit's inference
 *   bound in C, which covers a lazy cursor's whole walk and reports MT_LIMIT
 *   when the walk outgrows it: the gap-free (edge a $y) and the gap query
 *   (edge ... $y)
 *   each fit under 1,000 inferences with two hundred node atoms stored and
 *   still fit with two thousand more, so neither reads the node relation.
 *   The gap query over (node ... $y) answers all 2,200 rows, more than
 *   1,000 inferences could have read, which is what makes the bound a proof.
 *   sort-atom is qsort with mt_order over the values each match binds.
 * Guarantees: all nine claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* The original counts down to 0, which a negative count never reaches: its
   recursion runs out of stack, and this refuses by name instead. */
static mt_status nodes(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 0)) != MT_INT) return MT_FAIL;
    const int64_t count = mt_int(mt_arg(call, 0));
    if (count < 0) return mt_fail(call, "nodes counts down to 0, which a negative count never reaches");
    mt_list batch = { mt_alloc((size_t)count * sizeof *batch.items), (size_t)count };
    if (count > 0 && !batch.items) return mt_error_set(MT_NOMEM, "nodes has no room for its batch");
    for (int64_t n = count; n > 0; n--) batch.items[count - n] = E("node", n, "tag");
    return mt_add_all(mt_of(call), batch) ? mt_answer(call, S("done")) : mt_error();
}

/* How many rows a query finds within an inference budget, or -1 when its
   walk outgrows the budget, which is MT_LIMIT. TAKES pattern. */
static int64_t within(metta *m, uint64_t budget, mt_atom *pattern)
{
    require("bound the ask", mt_limit(m, (mt_limits){ .inferences = budget }));
    int64_t found = 0;
    mt_clear();
    mt_each (row, mt_query(m, pattern, NULL)) found++;
    const bool fitted = mt_ok();
    require("lift the bound", mt_limit(m, (mt_limits){0}));
    return fitted ? found : -1;
}

/* The values a query binds `name` to, in the standard order, which is
   sort-atom's. TAKES pattern. */
static mt_list sorted_bindings(metta *m, mt_atom *pattern, const char *name)
{
    mt_list values = { NULL, 0 };
    mt_rows (row, mt_query(m, pattern, NULL)) {
        mt_atom **grown = mt_resize(values.items, (values.len + 1) * sizeof *values.items);
        require("room for the bindings", grown != NULL);
        values.items = grown;
        values.items[values.len++] = mt_keep(mt_bound(row, name));
    }
    qsort(values.items, values.len, sizeof *values.items, mt_order);
    return values;
}

int main(void)
{
    metta *m = open_engine();
    require("nodes", mt_def(m, (mt_op){ .name = "nodes", .arity = 1, .effect = MT_EFFECT_CLASS_WRITES_STATE, .fn = nodes }));
    require("(edge a b)", mt_add(m, E("edge", "a", "b")));
    require("(edge a c)", mt_add(m, E("edge", "a", "c")));
    require("(edge a d e)", mt_add(m, E("edge", "a", "d", "e")));

    const uint64_t budget = 1000;
    static const int64_t stored[] = { 200, 2000 };
    for (size_t i = 0; i < 2; i++) {
        check_answers("the node relation grows", mt_eval(m, E("nodes", stored[i])), S("done"));
        check_int("the gap-free ask fits the budget and reads two rows",
                  within(m, budget, E("edge", "a", V("y"))), 2);
        check_int("the gap ask fits it too and reads three",
                  within(m, budget, E("edge", "...", V("y"))), 3);
    }
    int64_t rows = 0;
    mt_each (row, mt_query(m, E("node", "...", V("y")), NULL)) rows++;
    check_int("a gap over the node relation reads every row, more than the budget could",
              rows, stored[0] + stored[1]);
    check_list("the fixed pattern reads the two rows of its own length",
               sorted_bindings(m, E("edge", "a", V("y")), "y"), "b", "c");
    check_list("a segment reads all three", sorted_bindings(m, E("edge", "a", E(":seg", V("rest"))), "rest"),
               E("b"), E("c"), E("d", "e"));
    return done(m);
}
