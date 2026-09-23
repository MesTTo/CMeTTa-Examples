/* Purpose: the extension seam is data. The declared points are listed and
 *   looked up by name, a representation is registered for a C type so its
 *   objects print readably, and withdrawing it leaves the objects alive.
 * Owns resources: the object owns its string; the representation borrows it.
 * Guarantees: the repr point exists, the registration prints the value, and
 *   withdrawal is exact and final [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

static const char *display(void *value, void *user)
{
    (void)user;
    return value;
}

int main(void)
{
    metta *m = open_engine();
    check("the seam lists its points", mt_point_count(m) > 0);
    check("and repr is one of them", mt_point_of(m, "repr") != NULL);

    char *value = strdup("<C value>");
    require("own a value", value != NULL);
    mt_atom *object = mt_object(value, "ExampleValue", free);
    require("box it", object != NULL);
    require("register how it prints", mt_repr(m, "ExampleValue", display, NULL));
    check_text("the object prints through the registration", mt_show(object), "<C value>");
    check_answers("and its type is the name it was boxed under",
                  mt_eval(m, E("get-type", mt_keep(object))), "ExampleValue");

    require("withdraw the registration", mt_unregister(m, "repr", "ExampleValue"));
    mt_clear();
    check("withdrawing twice finds nothing, without an error",
          !mt_unregister(m, "repr", "ExampleValue") && mt_ok());
    check_text("the value itself lives on", mt_value(object), "<C value>");
    require("release the object", mt_object_free(object));
    return done(m);
}
