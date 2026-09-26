/* Purpose: reading source without running it. mt_forms() answers every
 *   top-level form of a text as atoms, a `!` form's body included, and writes
 *   nothing; mt_parse() refuses text that is not a whole form.
 * text: the program's subject is MeTTa source text as a reader receives it.
 * Guarantees: two forms read, nothing is stored, a broken form is refused
 *   with a status [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>

/* The same question as a truth value, for a condition with more in it:
   borrows the atom the program holds and takes the expectation. */
static inline bool alpha_equal(const mt_atom *got, mt_atom *want)
{
    bool equal = got && want && mt_alpha_eq(got, want);
    mt_drop(want);
    return equal;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_list forms = mt_forms("(fact 7) !(+ 20 22)");
    assert((int64_t)forms.len == 2 && "the reader answers two forms");
    assert(forms.len == 2 && alpha_equal(forms.items[0], E("fact", 7)) && "the first is the fact");
    assert(forms.len == 2 && alpha_equal(forms.items[1], E("+", 20, 22))
           && "the directive's body is data, not 42");
    mt_list_free(forms);
    assert((int64_t)mt_count(m) == 0 && "reading wrote nothing");

    mt_clear();
    mt_atom *broken = mt_parse("(unfinished");
    assert(broken == NULL && mt_error() == MT_ERROR && "a broken form is refused with a status");
    mt_clear();
    mt_close(m);
    return 0;
}
