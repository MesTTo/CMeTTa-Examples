/* Purpose: borrow C storage instead of copying it. mt_text_ref() makes a text
 *   atom over the program's own bytes and takes one release obligation, which
 *   runs exactly once, when the last reference to the atom goes.
 * Owns resources: the storage block, released by its callback.
 * Guarantees: the storage outlives every reference and is released once
 *   [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct storage { char text[8]; unsigned *released; } storage;

static void release_storage(void *owner)
{
    storage *s = owner;
    ++*s->released;
    free(s);
}

int main(void)
{
    unsigned released = 0;
    storage *s = malloc(sizeof *s);
    require("allocate the storage", s != NULL);
    memcpy(s->text, "payload", 8);
    s->released = &released;

    mt_atom *text = mt_text_ref(s->text, 7, s, release_storage);
    if (!text) free(s);        /* on failure the obligation stays here */
    require("borrow the bytes", text != NULL);
    mt_atom *held = mt_keep(text);
    mt_drop(text);
    check("a kept reference keeps the storage", released == 0 &&
                                                strcmp(mt_name(held), "payload") == 0);
    mt_drop(held);
    check_int("the last reference releases it once", released, 1);
    return done(NULL);
}
