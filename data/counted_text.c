/* Purpose: text is counted bytes, not a C string. mt_textn() carries an
 *   embedded NUL that strlen() would stop at, and mt_write_dup() then
 *   mt_parsen() round-trip all of it through the engine's writer and reader.
 * text: the program's subject is the source spelling of a text atom.
 * Guarantees: all three bytes survive the round trip [tested: make check;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    static const char bytes[] = { 'a', '\0', 'b' };
    mt_atom *text = mt_textn(bytes, sizeof bytes);
    check_int("the length counts the NUL", (int64_t)mt_name_len(text), 3);

    mt_string written = mt_write_dup(text);
    require("write the text", written.data != NULL);
    mt_atom *read = mt_parsen(written.data, written.len);
    mt_free(written.data);
    check("all three bytes survive the round trip",
          read && mt_name_len(read) == sizeof bytes &&
          memcmp(mt_name(read), bytes, sizeof bytes) == 0);
    check_atom("and the atom is the same atom", read, mt_keep(text));

    check_answers("the engine hands the bytes back", mt_eval(m, mt_keep(text)), text);
    return done(m);
}
