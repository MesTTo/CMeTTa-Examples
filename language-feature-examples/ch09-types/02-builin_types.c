/* Purpose: the built-in operations' declared types, read from C's own
 *   prototypes. Each operation has a C counterpart, libm's function where
 *   libm has one and a one-line C function where it has not, and _Generic
 *   reads that counterpart's signature as the arrow: double is Number, bool
 *   is Bool, and a parameter taking an atom of any type, const mt_atom *, is
 *   a type variable of its own, which is why == and != answer two
 *   independent variables. The C compiler states each arrow and the engine's
 *   lib_builtin_types agrees.
 * Guarantees: all thirty-six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

/* A C signature read as a MeTTa arrow. */
#define ARROW(f)                                                                         \
    _Generic((f),                                                                        \
        double (*)(double): E("->", "Number", "Number"),                                 \
        double (*)(double, double): E("->", "Number", "Number", "Number"),              \
        bool (*)(double): E("->", "Number", "Bool"),                                     \
        bool (*)(double, double): E("->", "Number", "Number", "Bool"),                   \
        bool (*)(bool): E("->", "Bool", "Bool"),                                         \
        bool (*)(bool, bool): E("->", "Bool", "Bool", "Bool"),                           \
        double (*)(const mt_atom *): E("->", V("a"), "Number"),                          \
        bool (*)(const mt_atom *, const mt_atom *): E("->", V("a"), V("b"), "Bool"))

/* The counterparts libm does not have. */
static double sum(double a, double b) { return a + b; }
static double difference(double a, double b) { return a - b; }
static double product(double a, double b) { return a * b; }
static double quotient(double a, double b) { return a / b; }
static bool less(double a, double b) { return a < b; }
static bool at_most(double a, double b) { return a <= b; }
static bool greater(double a, double b) { return a > b; }
static bool at_least(double a, double b) { return a >= b; }
static bool unequal(const mt_atom *a, const mt_atom *b) { return !mt_eq(a, b); }
static double log_base(double base, double x) { return log(x) / log(base); }
static bool not_a_number(double x) { return isnan(x); }
static bool infinite(double x) { return isinf(x); }
static bool both(bool a, bool b) { return a && b; }
static bool either(bool a, bool b) { return a || b; }
static bool negation(bool a) { return !a; }
static bool exactly_one(bool a, bool b) { return a != b; }

/* min-atom and max-atom: the least and greatest number of an expression. */
static double extreme(const mt_atom *numbers, bool greatest)
{
    double out = greatest ? -INFINITY : INFINITY;
    for (size_t i = 0; i < mt_len(numbers); i++) {
        double x = mt_float(mt_at(numbers, i));
        out = greatest ? fmax(out, x) : fmin(out, x);
    }
    return out;
}
static double least(const mt_atom *numbers) { return extreme(numbers, false); }
static double greatest(const mt_atom *numbers) { return extreme(numbers, true); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_builtin_types", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_builtin_types")))));
    const struct { const char *name; mt_atom *arrow; } builtins[] = {
        { "+", ARROW(&sum) },          { "-", ARROW(&difference) },  { "*", ARROW(&product) },       { "/", ARROW(&quotient) },
        { "%", ARROW(&fmod) },         { "<", ARROW(&less) },        { "<=", ARROW(&at_most) },      { ">", ARROW(&greater) },
        { ">=", ARROW(&at_least) },    { "==", ARROW(&mt_eq) },      { "!=", ARROW(&unequal) },      { "pow-math", ARROW(&pow) },
        { "sqrt-math", ARROW(&sqrt) }, { "abs-math", ARROW(&fabs) }, { "log-math", ARROW(&log_base) }, { "trunc-math", ARROW(&trunc) },
        { "ceil-math", ARROW(&ceil) }, { "floor-math", ARROW(&floor) }, { "round-math", ARROW(&round) }, { "sin-math", ARROW(&sin) },
        { "asin-math", ARROW(&asin) }, { "cos-math", ARROW(&cos) },   { "acos-math", ARROW(&acos) },  { "tan-math", ARROW(&tan) },
        { "atan-math", ARROW(&atan) }, { "min-atom", ARROW(&least) }, { "max-atom", ARROW(&greatest) }, { "min", ARROW(&fmin) },
        { "max", ARROW(&fmax) },       { "exp", ARROW(&exp) },       { "isnan-math", ARROW(&not_a_number) }, { "isinf-math", ARROW(&infinite) },
        { "and", ARROW(&both) },       { "or", ARROW(&either) },     { "not", ARROW(&negation) },    { "xor", ARROW(&exactly_one) },
    };
    for (size_t i = 0; i < sizeof builtins / sizeof *builtins; i++)
        check_answers(builtins[i].name, mt_eval(m, E("get-type", builtins[i].name)), builtins[i].arrow);
    return done(m);
}
