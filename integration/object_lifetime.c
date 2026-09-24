/* Purpose: a live C value in the space. mt_object() carries a pointer by
 *   reference under a type name the engine's get-type reads; stored and
 *   matched back it is the same pointer, and mt_object_free() releases the
 *   payload once, deterministically, without waiting for blob collection.
 * Owns resources: the payload, released by its callback.
 * Guarantees: identity survives a round trip, its type is Counter, and the
 *   payload is released exactly once [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static unsigned released;
static void release_payload(void *payload)
{
    released++;
    free(payload);
}

int main(void)
{
    metta *m = open_engine();
    int *counter = malloc(sizeof *counter);
    require("allocate the payload", counter != NULL);
    *counter = 42;
    mt_atom *object = mt_object(counter, "Counter", release_payload);
    require("box the payload", object != NULL);

    require("store it", mt_add(m, E("holds", mt_keep(object))));
    mt_atom *row = mt_one(mt_match(m, E("holds", V("x"))));
    check("the same pointer comes back", row && mt_value(mt_at(row, 1)) == counter);
    mt_drop(row);
    check_answers("its type is the name it was boxed under",
                  mt_eval(m, E("get-type", mt_keep(object))), "Counter");
    require("remove the fact", mt_del(m, E("holds", mt_keep(object))));
    require("release the object now", mt_object_free(object));
    check_int("the payload is released once", released, 1);
    return done(m);
}
