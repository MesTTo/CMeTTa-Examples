/* Purpose: lib_he's equation storage and space queries. addnormal's
 *   equation is added as C built it, so the space stores (+ 1 3) as
 *   written; lib_he's add-reduct evaluates the body list before storing it,
 *   so addreduct's body is the one-element list of C's sum. C reads each
 *   stored body back through the space's lookup and compares the atoms. A
 *   declared type is an atom C adds from its own table, and get-type-space
 *   answers it; unify against &self answers Yes exactly for the facts C
 *   added.
 * Guarantees: all five claims of the original hold, and its unasserted
 *   (get-type 1) as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)

/* The one body &self stores for (head), read back through a match. */
static void stored_body(metta *m, const char *claim, const char *head, mt_atom *want)
{
    mt_list bodies = { NULL, 0 };
    mt_rows (row, mt_match(m, E("=", E(head), V("X")))) {
        mt_atom **grown = mt_resize(bodies.items, (bodies.len + 1) * sizeof *grown);
        require("room", grown != NULL);
        bodies.items = grown;
        bodies.items[bodies.len++] = mt_keep(mt_bound(row, "X"));
    }
    assert(list_is(bodies, mt_exprv(1, &want)) && claim);
}

static const struct { const char *atom, *type; } declared[] = { { "a", "A" } };
static const char *const facts[][2] = { { "hello", "world" } };

static bool is_fact(const char *head, const char *arg)
{
    for (size_t i = 0; i < sizeof facts / sizeof *facts; i++)
        if (strcmp(facts[i][0], head) == 0 && strcmp(facts[i][1], arg) == 0) return true;
    return false;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("addnormal as written", mt_add(m, E("=", E("addnormal"), T_ADD(1, 3))));
    require("addreduct reduced",
            mt_one_truth(mt_eval(m, E("add-reduct", mt_spaceref("&self"), E("=", E("addreduct"), T_ADD(1, 3))))));
    stored_body(m, "add-atom stores the body as written", "addnormal", T_ADD(1, 3));
    stored_body(m, "add-reduct stores the evaluated body list", "addreduct", E(C_ADD(1, 3)));
    assert(answers_are(mt_eval(m, E("get-type", 1)), E("Number")) && "a number's type");

    for (size_t i = 0; i < sizeof declared / sizeof *declared; i++) {
        require("a declaration", mt_add(m, E(":", declared[i].atom, declared[i].type)));
        assert(answers_are(mt_eval(m, E("get-type-space", mt_spaceref("&self"), declared[i].atom)), E(declared[i].type))
               && "get-type-space answers the declaration");
    }
    for (size_t i = 0; i < sizeof facts / sizeof *facts; i++) require("a fact", mt_add(m, E(facts[i][0], facts[i][1])));
    static const char *const asked[][2] = { { "hello", "world" }, { "hello", "dream" } };
    for (size_t i = 0; i < sizeof asked / sizeof *asked; i++)
        assert(answers_are(mt_eval(m, E("unify", mt_spaceref("&self"), E(asked[i][0], asked[i][1]), "Yes", "No")), E(is_fact(asked[i][0], asked[i][1]) ? "Yes" : "No"))
               && "unify finds exactly C's facts");
    mt_close(m);
    return 0;
}
