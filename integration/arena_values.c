/* Purpose: Reclaim C atom allocations together after every reference is released.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
#include <stddef.h>
typedef union block block;
union block { max_align_t alignment; struct { block *next; } links; };
typedef struct { block *blocks; size_t live; } arena;
/* Lua's allocator protocol preserves the old block when growth fails:
 * https://github.com/lua/lua/blob/0b29f408433e92953cc72b1d3e06c7ac8139e439/lmem.c
 * Time: O(old_size) per resize; space: sum of allocation sizes until reset.
 */
static void *resize(void *user, void *pointer, size_t old_size, size_t new_size)
{
    arena *a = user;
    if (!new_size) { if (pointer) --a->live; return NULL; }
    if (new_size > SIZE_MAX - sizeof(block)) return NULL;
    block *b = malloc(sizeof(*b) + new_size);
    if (!b) return NULL;
    b->links.next = a->blocks; a->blocks = b;
    if (pointer) memcpy(b + 1, pointer, old_size < new_size ? old_size : new_size);
    else ++a->live;
    return b + 1;
}
int main(void)
{
    metta *m = open_engine();
    arena a = {0};
    mt_allocator previous = mt_allocator_set((mt_allocator){resize, &a});
    mt_atom *value = mt_expr("request", 7, mt_text("owned by arena"));
    mt_allocator_set(previous); /* Existing blocks keep their allocator. */
    check("arena owns atom storage", value != NULL && a.live > 0);
    mt_atom *returned = mt_one(mt_eval(m, mt_expr("quote", mt_keep(value))));
    check("arena term crosses the engine", returned && mt_eq(returned, value)); mt_drop(returned);
    mt_atom *held = mt_keep(value);
    mt_drop(value);
    check("retained value prevents reclamation", a.live > 0);
    mt_drop(held);
    check("every atom allocation released", a.live == 0);
    while (a.blocks) { block *next = a.blocks->links.next; free(a.blocks); a.blocks = next; }
    return done(m, "arena_values");
}
