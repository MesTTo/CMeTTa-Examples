/* Purpose: lib_he's equation storage and space queries. addnormal's
 *   equation is added as C built it, so the space stores (+ 1 3) as
 *   written; lib_he's add-reduct evaluates the body list before storing it,
 *   so addreduct's body is the one-element list of C's sum. C reads each
 *   stored body back through the space's lookup and compares the atoms. A
 *   declared type is an atom C adds from its own table, and get-type-space
 *   answers it; unify against &self answers Yes exactly for the facts C
 *   added.
 * Guarantees: all five claims of the original hold, and its unasserted
 *   (get-type 1) as well [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

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
    check_list_(claim, bodies, 1, &want);
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
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("addnormal as written", mt_add(m, E("=", E("addnormal"), T_ADD(1, 3))));
    require("addreduct reduced",
            mt_one_truth(mt_eval(m, E("add-reduct", mt_spaceref("&self"), E("=", E("addreduct"), T_ADD(1, 3))))));
    stored_body(m, "add-atom stores the body as written", "addnormal", T_ADD(1, 3));
    stored_body(m, "add-reduct stores the evaluated body list", "addreduct", E(C_ADD(1, 3)));
    check_answers("a number's type", mt_eval(m, E("get-type", 1)), "Number");

    for (size_t i = 0; i < sizeof declared / sizeof *declared; i++) {
        require("a declaration", mt_add(m, E(":", declared[i].atom, declared[i].type)));
        check_answers("get-type-space answers the declaration", mt_eval(m, E("get-type-space", mt_spaceref("&self"), declared[i].atom)),
                      declared[i].type);
    }
    for (size_t i = 0; i < sizeof facts / sizeof *facts; i++) require("a fact", mt_add(m, E(facts[i][0], facts[i][1])));
    static const char *const asked[][2] = { { "hello", "world" }, { "hello", "dream" } };
    for (size_t i = 0; i < sizeof asked / sizeof *asked; i++)
        check_answers("unify finds exactly C's facts",
                      mt_eval(m, E("unify", mt_spaceref("&self"), E(asked[i][0], asked[i][1]), "Yes", "No")),
                      is_fact(asked[i][0], asked[i][1]) ? "Yes" : "No");
    return done(m);
}
