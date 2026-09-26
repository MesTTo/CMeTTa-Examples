/* Purpose: comparing types as values. match-types answers its then-branch
 *   when two types are identical, which C decides with mt_eq, and
 *   match-type-or answers True for equal types and its value otherwise.
 * Guarantees: all four claims of the original hold, with its two unasserted
 *   forms checked as well [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

static mt_atom *match_types(mt_atom *a, mt_atom *b, mt_atom *then, mt_atom *otherwise)
{
    bool same = mt_eq(a, b);
    mt_drop(a), mt_drop(b), mt_drop(same ? otherwise : then);
    return same ? then : otherwise;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("define match-types", mt_add(m, E("=", E("match-types", V("A"), V("B"), V("Then"), V("Else")), E("if", E("==", V("A"), V("B")), V("Then"), V("Else")))));
    const char *pairs[][2] = { { "Atom", "Atom" }, { "Atom", "Number" } };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, E("match-types", pairs[i][0], pairs[i][1], T("Matched!"), T("Didn't match"))), E(match_types(S(pairs[i][0]), S(pairs[i][1]), T("Matched!"), T("Didn't match"))))
               && (i ? "different types" : "the same type"));
    require("define match-type-or", mt_add(m, E("=", E("match-type-or", V("value"), V("type1"), V("type2")), E("match-types", V("type1"), V("type2"), B(true), V("value")))));
    const struct { bool value; const char *type1, *type2; } rows[] = {
        { true, "Number", "Number" }, { false, "Number", "Number" }, { true, "Number", "Bool" }, { false, "Number", "Bool" },
    };
    for (size_t i = 0; i < 4; i++)
        assert(answers_are(mt_eval(m, E("match-type-or", B(rows[i].value), rows[i].type1, rows[i].type2)), E(match_types(S(rows[i].type1), S(rows[i].type2), B(true), B(rows[i].value))))
               && "match-type-or");
    mt_close(m);
    return 0;
}
