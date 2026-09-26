/* Purpose: a string is a value, not structure. mt_text() carries the bytes,
 *   parentheses included, and evaluating the text atom answers the same text:
 *   nothing inside it is read as a form.
 * Guarantees: the text evaluates to itself, byte for byte
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char words[] = "a test (with newlines and parentheses)";
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

    mt_atom *same = mt_one(mt_eval(m, T(words)));
    assert(mt_kind_of(same) == MT_TEXT && mt_name_len(same) == strlen(words) &&
           memcmp(mt_name(same), words, strlen(words)) == 0
           && "the text answers itself");
    mt_drop(same);
    mt_close(m);
    return 0;
}
