/* Purpose: discover extension points, install a representation and withdraw it.
 * Owns resources: the object owns its pointer; representation callback borrows it.
 * Guarantees: withdrawal removes the hook while the value remains live
 *   [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
static const char *display(void *value, void *user)
{ (void)user; return value; }
int main(void)
{
    metta *m = open_engine();
    check("declared points are inspectable", mt_point_count(m) > 0);
    check("representation point exists", mt_point_of(m, "repr") != NULL);
    char *value = strdup("<C value>"); check("owned value", value != NULL);
    mt_atom *object = mt_object(value, "ExampleValue", free); check("boxed object", object != NULL);
    check("publish representation", mt_repr(m, "ExampleValue", display, NULL));
    check("representation is active", strcmp(mt_show(object), "<C value>") == 0);
    check_answers("native type reaches engine", mt_eval(m, mt_expr("get-type", mt_keep(object))), "ExampleValue");
    check("withdraw exact registration", mt_unregister(m, "repr", "ExampleValue"));
    check("registration is gone", !mt_unregister(m, "repr", "ExampleValue") && mt_ok());
    check("value remains owned", strcmp(mt_value(object), "<C value>") == 0);
    check("release object", mt_object_free(object));
    return done(m, "registration_lifecycle");
}
