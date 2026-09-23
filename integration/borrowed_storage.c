/* Purpose: Borrow immutable C storage with an explicit last-owner callback.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct { char text[8]; unsigned *released; } storage;
static void release_storage(void *user)
{
    storage *s = user; ++*s->released; free(s);
}
int main(void)
{
    unsigned released = 0;
    storage *s = malloc(sizeof(*s));
    check("allocate backing", s != NULL);
    memcpy(s->text, "payload", 8); s->released = &released;
    mt_atom *text = mt_text_ref(s->text, 7, s, release_storage);
    if (!text) { free(s); check("construct borrowed text", false); }
    mt_atom *held = mt_keep(text);
    mt_drop(text);
    check("owner retained", released == 0 && strcmp(mt_name(held), "payload") == 0);
    mt_drop(held);
    check("owner released once", released == 1);
    return done(NULL, "borrowed_storage");
}
