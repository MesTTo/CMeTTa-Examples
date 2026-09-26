/* Purpose: Python as a notation reached from C. The equations wrap py-call
 *   and C builds them as terms; each answer Python gives is held against C's
 *   own: math's pi against M_PI, a string's upper against toupper, a method
 *   sum against C's +, a rounding to two digits against C's round over the
 *   scaled value, a dict lookup against the C table the dict's source is
 *   written from, and a class's name against the constructor path C asked
 *   for. bind! names a value for the reader, and C, building terms, holds
 *   the value in a variable instead.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 01-python.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

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

static const char constructor[] = "types.SimpleNamespace";

/* The preferences the dict is written from, as C holds them. */
static const struct { const char *key; bool number; const char *text; int64_t value; } prefs[] = {
    { "colour", false, "green", 0 }, { "size", true, NULL, 7 },
};

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("make-object", mt_add(m, E("=", E("make-object"), E("py-call", E(constructor)))));
    require("get-attribute", mt_add(m, E("=", E("get-attribute", V("obj"), V("name")), E("py-call", E("getattr", V("obj"), V("name"))))));
    require("set-attribute", mt_add(m, E("=", E("set-attribute", V("obj"), V("name"), V("value")), E("py-call", E("setattr", V("obj"), V("name"), V("value"))))));
    require("import", mt_add(m, E("=", E("import", V("name")), E("py-call", E("importlib.import_module", V("name"))))));
    require("math.pi", mt_add(m, E("=", E("math.pi"), E("get-attribute", E("import", "math"), "pi"))));

    assert(answers_are(mt_eval(m, E("let*", E(E(V("obj"), E("make-object")), E(V("temp"), E("set-attribute", V("obj"), "foo", E("math.pi")))),
                                  E("get-attribute", V("obj"), "foo"))), E(mt_real(M_PI)))
           && "pi through an object's attribute");
    const char word[] = "abc";
    char upper[sizeof word];
    for (size_t i = 0; i < sizeof word; i++) upper[i] = (char)toupper((unsigned char)word[i]);
    assert(answers_are(mt_eval(m, E("py-call", E(".upper", T(word)))), E(S(upper))) && "a string method");
    assert(answers_are(mt_eval(m, E("py-call", E(".__add__", 5, 3))), E(N(5 + 3))) && "a number method");

    mt_atom *py_round = mt_one(mt_eval(m, E("py-atom", "round")));
    require("round as a Python callable", py_round != NULL);
    const double x = 3.14159;
    const int64_t digits = 2;
    assert(answers_are(mt_eval(m, E(mt_keep(py_round), x, E("Kwargs", E("ndigits", digits)))), E(mt_real(round(x * pow(10, (double)digits)) / pow(10, (double)digits)))) && "keyword arguments");

    char source[128];
    int n = snprintf(source, sizeof source, "dict(%s='%s', %s=%lld)", prefs[0].key, prefs[0].text, prefs[1].key, (long long)prefs[1].value);
    require("the dict's source fits", n > 0 && (size_t)n < sizeof source);
    mt_atom *dict = mt_one(mt_eval(m, E("py-atom", T(source))));
    require("the dict held whole", dict != NULL);
    assert(answers_are(mt_eval(m, E("py-call", E(".get", mt_keep(dict), prefs[1].key))), E(N(prefs[1].value))) && "a dict asked for a key");
    assert(answers_are(mt_eval(m, E("py-dot", E("py-call", E("type", E("make-object"))), "__name__")), E(T(strrchr(constructor, '.') + 1))) && "a class's name");
    mt_drop(py_round), mt_drop(dict);
    mt_close(m);
    return 0;
}
