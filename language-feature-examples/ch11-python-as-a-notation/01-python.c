/* Purpose: Python as a notation reached from C. The equations wrap py-call
 *   and C builds them as terms; each answer Python gives is held against C's
 *   own: math's pi against M_PI, a string's upper against toupper, a method
 *   sum against C's +, a rounding to two digits against C's round over the
 *   scaled value, a dict lookup against the C table the dict's source is
 *   written from, and a class's name against the constructor path C asked
 *   for. bind! names a value for the reader, and C, building terms, holds
 *   the value in a variable instead.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#include "common.h"
#include <ctype.h>
#include <math.h>

static const char constructor[] = "types.SimpleNamespace";

/* The preferences the dict is written from, as C holds them. */
static const struct { const char *key; bool number; const char *text; int64_t value; } prefs[] = {
    { "colour", false, "green", 0 }, { "size", true, NULL, 7 },
};

int main(void)
{
    metta *m = open_engine();
    require("make-object", mt_add(m, E("=", E("make-object"), E("py-call", E(constructor)))));
    require("get-attribute", mt_add(m, E("=", E("get-attribute", V("obj"), V("name")), E("py-call", E("getattr", V("obj"), V("name"))))));
    require("set-attribute", mt_add(m, E("=", E("set-attribute", V("obj"), V("name"), V("value")), E("py-call", E("setattr", V("obj"), V("name"), V("value"))))));
    require("import", mt_add(m, E("=", E("import", V("name")), E("py-call", E("importlib.import_module", V("name"))))));
    require("math.pi", mt_add(m, E("=", E("math.pi"), E("get-attribute", E("import", "math"), "pi"))));

    check_answers("pi through an object's attribute", mt_eval(m, E("let*", E(E(V("obj"), E("make-object")), E(V("temp"), E("set-attribute", V("obj"), "foo", E("math.pi")))),
                                                                 E("get-attribute", V("obj"), "foo"))),
                  mt_real(M_PI));
    const char word[] = "abc";
    char upper[sizeof word];
    for (size_t i = 0; i < sizeof word; i++) upper[i] = (char)toupper((unsigned char)word[i]);
    check_answers("a string method", mt_eval(m, E("py-call", E(".upper", T(word)))), S(upper));
    check_answers("a number method", mt_eval(m, E("py-call", E(".__add__", 5, 3))), N(5 + 3));

    mt_atom *py_round = mt_one(mt_eval(m, E("py-atom", "round")));
    require("round as a Python callable", py_round != NULL);
    const double x = 3.14159;
    const int64_t digits = 2;
    check_answers("keyword arguments", mt_eval(m, E(mt_keep(py_round), x, E("Kwargs", E("ndigits", digits)))), mt_real(round(x * pow(10, (double)digits)) / pow(10, (double)digits)));

    char source[128];
    int n = snprintf(source, sizeof source, "dict(%s='%s', %s=%lld)", prefs[0].key, prefs[0].text, prefs[1].key, (long long)prefs[1].value);
    require("the dict's source fits", n > 0 && (size_t)n < sizeof source);
    mt_atom *dict = mt_one(mt_eval(m, E("py-atom", T(source))));
    require("the dict held whole", dict != NULL);
    check_answers("a dict asked for a key", mt_eval(m, E("py-call", E(".get", mt_keep(dict), prefs[1].key))), N(prefs[1].value));
    check_answers("a class's name", mt_eval(m, E("py-dot", E("py-call", E("type", E("make-object"))), "__name__")), T(strrchr(constructor, '.') + 1));
    mt_drop(py_round), mt_drop(dict);
    return done(m);
}
