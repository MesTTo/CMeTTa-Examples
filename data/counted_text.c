/* Purpose: Round-trip UTF-8 text containing an embedded NUL.
 * Owns resources: local C handles are released before exit; a failed check
 *   terminates the example process.
 * Guarantees: the assertions below hold [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    const char bytes[] = {'a', '\0', 'b', '\0'};
    mt_atom *text = mt_textn(bytes, 3);
    mt_string written = mt_write_dup(text);
    check("owned counted source", written.data != NULL);
    mt_atom *copy = mt_parsen(written.data, written.len);
    check("all bytes survive", mt_eq(text, copy) && mt_name_len(copy) == 3 &&
          memcmp(mt_name(copy), bytes, 3) == 0);
    mt_free(written.data); mt_drop(copy); mt_drop(text);
    return done(m, "counted_text");
}

