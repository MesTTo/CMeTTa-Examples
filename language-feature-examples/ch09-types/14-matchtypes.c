/* Purpose: comparing types as values. match-types answers its then-branch
 *   when two types are identical, which C decides with mt_eq, and
 *   match-type-or answers True for equal types and its value otherwise.
 * Guarantees: all four claims of the original hold, with its two unasserted
 *   forms checked as well [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_atom *match_types(mt_atom *a, mt_atom *b, mt_atom *then, mt_atom *otherwise)
{
    bool same = mt_eq(a, b);
    mt_drop(a), mt_drop(b), mt_drop(same ? otherwise : then);
    return same ? then : otherwise;
}

int main(void)
{
    metta *m = open_engine();
    require("define match-types", mt_add(m, E("=", E("match-types", V("A"), V("B"), V("Then"), V("Else")), E("if", E("==", V("A"), V("B")), V("Then"), V("Else")))));
    const char *pairs[][2] = { { "Atom", "Atom" }, { "Atom", "Number" } };
    for (size_t i = 0; i < 2; i++)
        check_answers(i ? "different types" : "the same type", mt_eval(m, E("match-types", pairs[i][0], pairs[i][1], T("Matched!"), T("Didn't match"))),
                      match_types(S(pairs[i][0]), S(pairs[i][1]), T("Matched!"), T("Didn't match")));
    require("define match-type-or", mt_add(m, E("=", E("match-type-or", V("value"), V("type1"), V("type2")), E("match-types", V("type1"), V("type2"), B(true), V("value")))));
    const struct { bool value; const char *type1, *type2; } rows[] = {
        { true, "Number", "Number" }, { false, "Number", "Number" }, { true, "Number", "Bool" }, { false, "Number", "Bool" },
    };
    for (size_t i = 0; i < 4; i++)
        check_answers("match-type-or", mt_eval(m, E("match-type-or", B(rows[i].value), rows[i].type1, rows[i].type2)),
                      match_types(S(rows[i].type1), S(rows[i].type2), B(true), B(rows[i].value)));
    return done(m);
}
