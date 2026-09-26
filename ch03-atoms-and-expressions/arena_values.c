/* Purpose: the caller's allocator. mt_allocator_set() routes every atom
 *   allocation on this thread through an arena of the program's own; each
 *   block remembers the allocator that made it, so atoms built under the
 *   arena are released into it even after the default is restored, and the
 *   arena sees its live count reach zero when the last reference goes.
 * Owns resources: the arena's blocks, freed together at the end.
 * Guarantees: an arena-built atom crosses the engine, a kept reference holds
 *   it, and dropping the last reference releases every block
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    arena a = {0};
    mt_allocator previous = mt_allocator_set((mt_allocator){ resize, &a });
    mt_atom *request = E("request", 7, T("owned by the arena"));
    mt_allocator_set(previous);     /* existing blocks keep their allocator */
    assert(request != NULL && a.live > 0 && "the arena holds the atom's storage");

    assert(answers_are(mt_eval(m, E("quote", mt_keep(request))), E(E("request", 7, T("owned by the arena"))))
           && "an arena atom crosses the engine");

    mt_atom *held = mt_keep(request);
    mt_drop(request);
    assert(a.live > 0 && "a kept reference holds its blocks");
    mt_drop(held);
    assert((int64_t)a.live == 0 && "the last reference releases every block");
    while (a.blocks) {
        block *next = a.blocks->link.next;
        free(a.blocks);
        a.blocks = next;
    }
    mt_close(m);
    return 0;
}
