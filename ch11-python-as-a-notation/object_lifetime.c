/* Purpose: a live C value in the space. mt_object() carries a pointer by
 *   reference under a type name the engine's get-type reads; stored and
 *   matched back it is the same pointer, and mt_object_free() releases the
 *   payload once, deterministically, without waiting for blob collection.
 * Owns resources: the payload, released by its callback.
 * Guarantees: identity survives a round trip, its type is Counter, and the
 *   payload is released exactly once [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

static unsigned released;
static void release_payload(void *payload)
{
    released++;
    free(payload);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    int *counter = malloc(sizeof *counter);
    require("allocate the payload", counter != NULL);
    *counter = 42;
    mt_atom *object = mt_object(counter, "Counter", release_payload);
    require("box the payload", object != NULL);

    require("store it", mt_add(m, E("holds", mt_keep(object))));
    mt_atom *row = mt_one(mt_match(m, E("holds", V("x"))));
    assert(row && mt_value(mt_at(row, 1)) == counter && "the same pointer comes back");
    mt_drop(row);
    assert(answers_are(mt_eval(m, E("get-type", mt_keep(object))), E("Counter"))
           && "its type is the name it was boxed under");
    require("remove the fact", mt_del(m, E("holds", mt_keep(object))));
    require("release the object now", mt_object_free(object));
    assert(released == 1 && "the payload is released once");
    mt_close(m);
    return 0;
}
