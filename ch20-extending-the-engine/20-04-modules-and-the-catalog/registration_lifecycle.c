/* Purpose: the extension seam is data. The declared points are listed and
 *   looked up by name, a representation is registered for a C type so its
 *   objects print readably, and withdrawing it leaves the objects alive.
 * Owns resources: the object owns its string; the representation borrows it.
 * Guarantees: the repr point exists, the registration prints the value, and
 *   withdrawal is exact and final [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
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

static const char *display(void *value, void *user)
{
    (void)user;
    return value;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    assert(mt_point_count(m) > 0 && "the seam lists its points");
    assert(mt_point_of(m, "repr") != NULL && "and repr is one of them");

    char *value = strdup("<C value>");
    require("own a value", value != NULL);
    mt_atom *object = mt_object(value, "ExampleValue", free);
    require("box it", object != NULL);
    require("register how it prints", mt_repr(m, "ExampleValue", display, NULL));
    assert(strcmp(mt_show(object), "<C value>") == 0 && "the object prints through the registration");
    assert(answers_are(mt_eval(m, E("get-type", mt_keep(object))), E("ExampleValue"))
           && "and its type is the name it was boxed under");

    require("withdraw the registration", mt_unregister(m, "repr", "ExampleValue"));
    mt_clear();
    assert(!mt_unregister(m, "repr", "ExampleValue") && mt_ok()
           && "withdrawing twice finds nothing, without an error");
    assert(strcmp(mt_value(object), "<C value>") == 0 && "the value itself lives on");
    require("release the object", mt_object_free(object));
    mt_close(m);
    return 0;
}
