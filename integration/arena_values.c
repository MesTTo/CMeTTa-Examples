/* Purpose: the caller's allocator. mt_allocator_set() routes every atom
 *   allocation on this thread through an arena of the program's own; each
 *   block remembers the allocator that made it, so atoms built under the
 *   arena are released into it even after the default is restored, and the
 *   arena sees its live count reach zero when the last reference goes.
 * Owns resources: the arena's blocks, freed together at the end.
 * Guarantees: an arena-built atom crosses the engine, a kept reference holds
 *   it, and dropping the last reference releases every block [tested: make
 *   check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <stddef.h>

typedef union block block;
union block {
    max_align_t alignment;
    struct { block *next; } link;
};
typedef struct arena { block *blocks; size_t live; } arena;

/* realloc's contract, as mt_allocator asks: NULL allocates, zero frees, and
   a failed growth keeps the old block. Blocks are only reclaimed with the
   whole arena. Time O(old_size) per resize.
   [source: https://github.com/lua/lua/blob/0b29f408433e92953cc72b1d3e06c7ac8139e439/lmem.c] */
static void *resize(void *user, void *pointer, size_t old_size, size_t new_size)
{
    arena *a = user;
    if (new_size == 0) {
        if (pointer) a->live--;
        return NULL;
    }
    if (new_size > SIZE_MAX - sizeof(block)) return NULL;
    block *b = malloc(sizeof *b + new_size);
    if (!b) return NULL;
    b->link.next = a->blocks;
    a->blocks = b;
    if (pointer) memcpy(b + 1, pointer, old_size < new_size ? old_size : new_size);
    else a->live++;
    return b + 1;
}

int main(void)
{
    metta *m = open_engine();
    arena a = {0};
    mt_allocator previous = mt_allocator_set((mt_allocator){ resize, &a });
    mt_atom *request = E("request", 7, T("owned by the arena"));
    mt_allocator_set(previous);     /* existing blocks keep their allocator */
    check("the arena holds the atom's storage", request != NULL && a.live > 0);

    check_answers("an arena atom crosses the engine",
                  mt_eval(m, E("quote", mt_keep(request))),
                  E("request", 7, T("owned by the arena")));

    mt_atom *held = mt_keep(request);
    mt_drop(request);
    check("a kept reference holds its blocks", a.live > 0);
    mt_drop(held);
    check_int("the last reference releases every block", (int64_t)a.live, 0);
    while (a.blocks) {
        block *next = a.blocks->link.next;
        free(a.blocks);
        a.blocks = next;
    }
    return done(m);
}
