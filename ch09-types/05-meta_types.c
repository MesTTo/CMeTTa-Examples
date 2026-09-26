/* Purpose: a metatype is what the engine makes of an atom, and C's mt_kind
 *   is what the atom is; metatype() maps the one onto the other.
 *   They agree for every atom here but one: + is a symbol as C builds it and
 *   Grounded as the engine reads it, because the engine resolves that name
 *   to a built-in operation.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

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

/* The metatype get-metatype answers for an atom of this kind: Symbol for a
   symbol and for a space, which the engine names by a symbol; Variable;
   Expression, the empty one included; Grounded for every value. */
static inline const char *metatype(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_SYMBOL:
    case MT_SPACE: return "Symbol";
    case MT_VARIABLE: return "Variable";
    case MT_EXPR: return "Expression";
    default: return "Grounded";
    }
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *atoms[] = { E("foo", 1, 2), E("a", "b"), N(1), V("x"), S("a") };
    for (size_t i = 0; i < sizeof atoms / sizeof *atoms; i++) {
        char *label = mt_show_dup(atoms[i]);
        assert(answers_are(mt_eval(m, E("get-metatype", mt_keep(atoms[i]))), E(S(metatype(atoms[i])))) && label);
        mt_free(label), mt_drop(atoms[i]);
    }
    mt_atom *plus = S("+");
    assert(mt_kind_of(plus) == MT_SYMBOL && "+ is a symbol as C builds it");
    assert(answers_are(mt_eval(m, E("get-metatype", plus)), E(S("Grounded"))) && "and Grounded as the engine reads it");
    mt_close(m);
    return 0;
}
