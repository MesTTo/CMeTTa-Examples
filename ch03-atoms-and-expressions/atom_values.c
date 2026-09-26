/* Purpose: an atom is a C value. Symbols, text, numbers and booleans are
 *   distinct kinds, children are borrowed from their parent, and a child kept
 *   with mt_keep() outlives the parent it came from.
 * Guarantees: each kind and payload reads back as built
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *record = E("record", T("Ada"), 42, B(true));
    assert(mt_kind_of(record) == MT_EXPR && "an expression");
    assert((int64_t)mt_len(record) == 4 && "of four children");
    assert(mt_kind_of(mt_at(record, 0)) == MT_SYMBOL && "the head is a symbol");
    assert(mt_kind_of(mt_at(record, 1)) == MT_TEXT && "the name is text, not a symbol");
    assert(mt_int(mt_at(record, 2)) == 42 && "the number is an integer");
    assert(mt_kind_of(mt_at(record, 3)) == MT_BOOL &&
           mt_truth(mt_at(record, 3))
           && "the flag is a boolean");

    mt_atom *name = mt_keep(mt_at(record, 1));
    mt_drop(record);
    assert(atom_is(name, T("Ada")) && "a kept child outlives its parent");

    /* The engine hands the same kinds back. */
    assert(answers_are(mt_eval(m, E("record", T("Ada"), 42, B(true))), E(E("record", T("Ada"), 42, B(true))))
           && "a record evaluates to itself");
    mt_close(m);
    return 0;
}
