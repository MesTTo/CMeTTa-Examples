/* Purpose: at most k answers. In C that is a loop over a lazy cursor that
 *   stops after k and abandons the rest uncomputed, so it ends a producer
 *   that counts up forever. The same bound reaches a match and a join, and
 *   the engine's own take refuses a count that is not a number, which C
 *   sees as MT_ERROR on the cursor.
 * Guarantees: all seven claims of the original hold
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(list_is(take(mt_eval(m, E("superpose", E("a", "b", "c", "d", "e"))), 3), E("a", "b", "c")) && "three of five");
    assert(list_is(take(mt_eval(m, E("superpose", E("a", "b"))), 9), E("a", "b")) && "nine of two is two");
    assert(list_is(take(mt_eval(m, E("superpose", E("a", "b"))), 0), mt_exprv(0, NULL)) && "zero is none");

    require("from", mt_add(m, E("=", E("from", V("n")), E("superpose", E(V("n"), E("from", E("+", V("n"), 1)))))));
    assert(list_is(take(mt_eval(m, E("from", 0)), 4), E(0, 1, 2, 3)) && "four of an endless count");

    mt_clear();
    mt_list refused = mt_all(mt_eval(m, E("take", "foo", E("superpose", E("a", "b")))));
    assert(refused.len == 0 && mt_error() == MT_ERROR && "a count that is not a number is refused");
    mt_list_free(refused);
    mt_clear();

    for (size_t i = 0; i < sizeof EDGES / sizeof *EDGES; i++)
        require("(edge x y)", mt_add(m, E("edge", EDGES[i][0], EDGES[i][1])));
    assert(list_is(take(mt_match(m, E("edge", V("x"), V("y"))), 2), E(E("edge", "a", "b"), E("edge", "b", "c")))
           && "two edges");

    /* The bound belongs to the joined rows, so C counts rows of the join. */
    mt_list paths = { mt_alloc(2 * sizeof *paths.items), 0 };
    require("room for two paths", paths.items != NULL);
    mt_rows (row, mt_query(m, E(",", E("edge", V("x"), V("y")), E("edge", V("y"), V("z"))), NULL)) {
        paths.items[paths.len++] = E(mt_keep(mt_bound(row, "x")), mt_keep(mt_bound(row, "z")));
        if (paths.len == 2) break;
    }
    assert(list_is(paths, E(E("a", "c"), E("b", "d"))) && "two joined paths");
    mt_close(m);
    return 0;
}
