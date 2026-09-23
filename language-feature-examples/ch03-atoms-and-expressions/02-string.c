/* Purpose: a string is a value, not structure. mt_text() carries the bytes,
 *   parentheses included, and evaluating the text atom answers the same text:
 *   nothing inside it is read as a form.
 * Guarantees: the text evaluates to itself, byte for byte
 *   [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    static const char words[] = "a test (with newlines and parentheses)";
    metta *m = open_engine();

    mt_atom *same = mt_one(mt_eval(m, T(words)));
    check("the text answers itself",
          mt_kind_of(same) == MT_TEXT && mt_name_len(same) == strlen(words) &&
          memcmp(mt_name(same), words, strlen(words)) == 0);
    mt_drop(same);
    return done(m);
}
