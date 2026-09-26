/* Purpose: borrow C storage instead of copying it. mt_text_ref() makes a text
 *   atom over the program's own bytes and takes one release obligation, which
 *   runs exactly once, when the last reference to the atom goes.
 * Owns resources: the storage block, released by its callback.
 * Guarantees: the storage outlives every reference and is released once
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
    assert(released == 0 &&
           strcmp(mt_name(held), "payload") == 0
           && "a kept reference keeps the storage");
    mt_drop(held);
    assert(released == 1 && "the last reference releases it once");
    return 0;
}
