/* Purpose: at most k answers. In C that is a loop over a lazy cursor that
 *   stops after k and abandons the rest uncomputed, so it ends a producer
 *   that counts up forever. The same bound reaches a match and a join, and
 *   the engine's own take refuses a count that is not a number, which C
 *   sees as MT_ERROR on the cursor.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

/* The first k answers, the cursor abandoned after them. */
static mt_list take(mt_answers *answers, size_t k)
{
    mt_list out = { mt_alloc((k ? k : 1) * sizeof *out.items), 0 };
    const mt_atom *answer;
    while (out.items && out.len < k && (answer = mt_next(answers)) != NULL)
        out.items[out.len++] = mt_keep(answer);
    mt_answers_free(answers);
    return out;
}

static const char *const EDGES[][2] = { { "a", "b" }, { "b", "c" }, { "c", "d" } };

int main(void)
{
    metta *m = open_engine();
    check_list("three of five", take(mt_eval(m, E("superpose", E("a", "b", "c", "d", "e"))), 3), "a", "b", "c");
    check_list("nine of two is two", take(mt_eval(m, E("superpose", E("a", "b"))), 9), "a", "b");
    check_list_("zero is none", take(mt_eval(m, E("superpose", E("a", "b"))), 0), 0, NULL);

    require("from", mt_add(m, E("=", E("from", V("n")), E("superpose", E(V("n"), E("from", E("+", V("n"), 1)))))));
    check_list("four of an endless count", take(mt_eval(m, E("from", 0)), 4), 0, 1, 2, 3);

    mt_clear();
    mt_list refused = mt_all(mt_eval(m, E("take", "foo", E("superpose", E("a", "b")))));
    check("a count that is not a number is refused", refused.len == 0 && mt_error() == MT_ERROR);
    mt_list_free(refused);
    mt_clear();

    for (size_t i = 0; i < sizeof EDGES / sizeof *EDGES; i++)
        require("(edge x y)", mt_add(m, E("edge", EDGES[i][0], EDGES[i][1])));
    check_list("two edges", take(mt_match(m, E("edge", V("x"), V("y"))), 2),
               E("edge", "a", "b"), E("edge", "b", "c"));

    /* The bound belongs to the joined rows, so C counts rows of the join. */
    mt_list paths = { mt_alloc(2 * sizeof *paths.items), 0 };
    require("room for two paths", paths.items != NULL);
    mt_rows (row, mt_query(m, E(",", E("edge", V("x"), V("y")), E("edge", V("y"), V("z"))), NULL)) {
        paths.items[paths.len++] = E(mt_keep(mt_bound(row, "x")), mt_keep(mt_bound(row, "z")));
        if (paths.len == 2) break;
    }
    check_list("two joined paths", paths, E("a", "c"), E("b", "d"));
    return done(m);
}
