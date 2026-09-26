/* Purpose: lib_pln2's check that operands rest on disjoint evidence, asked
 *   of the library and held to C's model of its walk: the support groups in
 *   order and each group's IDs in order, where the first ID already met in
 *   its own group is a duplicate and the first met in an earlier group makes
 *   the operands dependent, each refusal the ball naming that ID and the
 *   remedy it ends with, C's table of the library's refusals [source:
 *   lib/lib_pln2/lib_pln2.pl, pln2_support_ids/5;
 *   commit=8d651070dedaa190e25cc388c029172a63e967be]. Groups C finds
 *   independent answer True; the others are caught, and C compares the ball
 *   and the remedy it builds with the ones the engine raised.
 * Guarantees: all eight claims of the original hold
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

typedef enum { INDEPENDENT, DUPLICATE, DEPENDENT } walked;

static const struct {
    const char *ball, *remedy;
} refusals[] = {
    [DUPLICATE] = { "pln2_duplicate_support_identity", "list each evidence ID once within an operand support" },
    [DEPENDENT] = { "pln2_dependent_supports",
                    "factor shared support in an owned reasoner with stable signed evidence IDs, then combine only disjoint residual supports" },
};

/* The first ID the walk refuses in GROUPS, and why; INDEPENDENT with no ID
   where it refuses none. */
static walked walk(const mt_atom *groups, const mt_atom **refused)
{
    for (size_t g = 0; g < mt_len(groups); g++) {
        const mt_atom *group = mt_at(groups, g);
        for (size_t i = 0; i < mt_len(group); i++) {
            const mt_atom *id = mt_at(group, i);
            *refused = id;
            for (size_t j = 0; j < i; j++)
                if (mt_eq(mt_at(group, j), id)) return DUPLICATE;
            for (size_t h = 0; h < g; h++)
                for (size_t k = 0; k < mt_len(mt_at(groups, h)); k++)
                    if (mt_eq(mt_at(mt_at(groups, h), k), id)) return DEPENDENT;
        }
    }
    *refused = NULL;
    return INDEPENDENT;
}

/* The engine's (Error BALL CONTEXT) for GROUPS, caught. */
static mt_atom *caught(metta *m, const mt_atom *groups)
{
    mt_atom *error = mt_one(mt_eval(m, E("catch", E("pln2-require-independent-supports", mt_keep(groups)))));
    require("a refusal", error && mt_kind_of(error) == MT_EXPR && mt_len(error) == 3);
    return error;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("lib_pln2", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln2")))));

    mt_atom *independent[] = {
        E(E("sensor-a"), E("sensor-b")), E(E("sensor-a", "sensor-b"), E("sensor-c")), mt_unit(), E(E("sensor-a")), E(E("sensor-a"), mt_unit()),
    };
    for (size_t i = 0; i < sizeof independent / sizeof *independent; i++) {
        const mt_atom *refused;
        require("C's walk refuses nothing", walk(independent[i], &refused) == INDEPENDENT);
        assert(answers_are(mt_eval(m, E("pln2-require-independent-supports", mt_keep(independent[i]))), E(B(true))) && "disjoint supports are independent");
        mt_drop(independent[i]);
    }

    /* A shared ID refused by name, and the remedy the refusal ends with. */
    mt_atom *shared[] = { E(E("sensor-a"), E("sensor-a")), E(E("sensor-a", "sensor-b"), E("sensor-b")) };
    for (size_t i = 0; i < sizeof shared / sizeof *shared; i++) {
        const mt_atom *refused;
        walked why = walk(shared[i], &refused);
        require("C's walk refuses an ID", why != INDEPENDENT);
        mt_atom *error = caught(m, shared[i]);
        assert(atom_is(mt_keep(mt_at(error, 1)), E(refusals[why].ball, mt_keep(refused))) && "the refusal names the shared ID");
        mt_drop(error), mt_drop(shared[i]);
    }
    mt_atom *same = E(E("s"), E("s"));
    const mt_atom *refused;
    walked why = walk(same, &refused);
    require("C's walk refuses an ID", why != INDEPENDENT);
    mt_atom *error = caught(m, same);
    assert(atom_is(mt_keep(mt_at(mt_at(error, 2), 2)), T(refusals[why].remedy)) && "the refusal ends in the remedy");
    mt_drop(error), mt_drop(same);
    mt_close(m);
    return 0;
}
