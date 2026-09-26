/* Purpose: text is counted bytes, not a C string. mt_textn() carries an
 *   embedded NUL that strlen() would stop at, and mt_write_dup() then
 *   mt_parsen() round-trip all of it through the engine's writer and reader.
 * text: the program's subject is the source spelling of a text atom.
 * Guarantees: all three bytes survive the round trip
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    static const char bytes[] = { 'a', '\0', 'b' };
    mt_atom *text = mt_textn(bytes, sizeof bytes);
    assert((int64_t)mt_name_len(text) == 3 && "the length counts the NUL");

    mt_string written = mt_write_dup(text);
    require("write the text", written.data != NULL);
    mt_atom *read = mt_parsen(written.data, written.len);
    mt_free(written.data);
    assert(read && mt_name_len(read) == sizeof bytes &&
           memcmp(mt_name(read), bytes, sizeof bytes) == 0
           && "all three bytes survive the round trip");
    assert(atom_is(read, mt_keep(text)) && "and the atom is the same atom");

    assert(answers_are(mt_eval(m, mt_keep(text)), E(text)) && "the engine hands the bytes back");
    mt_close(m);
    return 0;
}
