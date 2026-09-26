/* Purpose: a space whose atoms live in C, as a provider behind cmetta's
 *   mt_provider_open. c_store holds each atom it is given in a C array under
 *   a pthread mutex and answers each match with a snapshot of the array, so
 *   the engine keeps unification for itself and filters what C enumerates,
 *   and a writer during a walk changes the next match rather than this one.
 *   remove takes one occurrence, the seam's contract, which remove-atom
 *   drains through. c_store_provider() hands the store over with every
 *   callback it has and the rules promise the caller makes for it: a store
 *   that holds equations the engine compiles, or one that holds data only.
 *   Shared, as an #include is, by chapter 19's C space and chapter 20's
 *   space that holds rules.
 * Owns resources: every atom it holds, kept when added and dropped when
 *   removed, cleared or released; release frees the array.
 * Guarded by: the store's own mutex, over its atoms and their count.
 */
#ifndef C_STORE_H
#define C_STORE_H
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct c_store {
    pthread_mutex_t lock;
    mt_atom **atoms;
    size_t n, cap;
} c_store;

#define C_STORE_INIT { .lock = PTHREAD_MUTEX_INITIALIZER }

static inline mt_status c_store_add(void *user, const mt_atom *atom)
{
    c_store *s = user;
    pthread_mutex_lock(&s->lock);
    if (s->n == s->cap) {
        size_t cap = s->cap ? 2 * s->cap : 8;
        mt_atom **grown = realloc(s->atoms, cap * sizeof *grown);
        if (!grown) {
            pthread_mutex_unlock(&s->lock);
            return mt_error_set(MT_NOMEM, "the C store has no room");
        }
        s->atoms = grown;
        s->cap = cap;
    }
    s->atoms[s->n++] = mt_keep(atom);
    pthread_mutex_unlock(&s->lock);
    return MT_OK;
}

/* One occurrence, which is the seam's contract; remove-atom above it drains. */
static inline mt_status c_store_remove(void *user, const mt_atom *atom, bool *removed)
{
    c_store *s = user;
    *removed = false;
    pthread_mutex_lock(&s->lock);
    for (size_t i = 0; i < s->n && !*removed; i++)
        if (mt_eq(s->atoms[i], atom)) {
            mt_drop(s->atoms[i]);
            s->atoms[i] = s->atoms[--s->n];
            *removed = true;
        }
    pthread_mutex_unlock(&s->lock);
    return MT_OK;
}

typedef struct c_store_snapshot {
    mt_atom **atoms;
    size_t n, next;
} c_store_snapshot;

static inline mt_status c_store_step(void *state, mt_atom **answer)
{
    c_store_snapshot *s = state;
    if (s->next == s->n) return MT_DONE;
    *answer = s->atoms[s->next];
    s->atoms[s->next++] = NULL;
    return MT_ROW;
}

static inline void c_store_close_snapshot(void *state)
{
    c_store_snapshot *s = state;
    for (size_t i = s->next; i < s->n; i++) mt_drop(s->atoms[i]);
    free(s->atoms);
    free(s);
}

static inline mt_status c_store_match(void *user, const mt_atom *pattern, size_t limit, mt_iterator *answers)
{
    c_store *s = user;
    (void)pattern;
    (void)limit;
    c_store_snapshot *snap = calloc(1, sizeof *snap);
    if (!snap) return mt_error_set(MT_NOMEM, "the C store has no room for a snapshot");
    pthread_mutex_lock(&s->lock);
    snap->atoms = malloc((s->n ? s->n : 1) * sizeof *snap->atoms);
    if (snap->atoms)
        for (size_t i = 0; i < s->n; i++) snap->atoms[snap->n++] = mt_keep(s->atoms[i]);
    pthread_mutex_unlock(&s->lock);
    if (!snap->atoms) {
        free(snap);
        return mt_error_set(MT_NOMEM, "the C store has no room for a snapshot");
    }
    *answers = (mt_iterator){ snap, c_store_step, c_store_close_snapshot };
    return MT_OK;
}

static inline mt_status c_store_clear(void *user)
{
    c_store *s = user;
    pthread_mutex_lock(&s->lock);
    while (s->n) mt_drop(s->atoms[--s->n]);
    pthread_mutex_unlock(&s->lock);
    return MT_OK;
}

/* What closing the provider hands back: every atom the store still holds. */
static inline void c_store_release(void *user)
{
    c_store *s = user;
    c_store_clear(s);
    free(s->atoms);
    s->atoms = NULL;
    s->cap = 0;
}

static inline size_t c_store_held(c_store *s)
{
    pthread_mutex_lock(&s->lock);
    size_t n = s->n;
    pthread_mutex_unlock(&s->lock);
    return n;
}

/* The provider over S, with every callback and the RULES promise. */
static inline mt_provider c_store_provider(c_store *s, bool rules)
{
    return (mt_provider){ .user = s, .add = c_store_add, .remove = c_store_remove, .match = c_store_match,
                          .clear = c_store_clear, .release = c_store_release, .rules = rules };
}
#endif
