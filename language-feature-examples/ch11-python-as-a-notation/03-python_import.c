/* Purpose: a Python file imported as a module of functions. C imports the
 *   original's own fixture and holds each function's answer against its C
 *   counterpart: greet's sentence through snprintf and add's sum through +.
 * Guarantees: both claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("import the fixture", mt_one_truth(mt_eval(m, E("import!", "&self", T("examples/ch11-python-as-a-notation/_fixtures/python_import_file.py")))));
    const char name[] = "MeTTa User";
    char greeting[64];
    int n = snprintf(greeting, sizeof greeting, "Hello, %s from Python!", name);
    require("the greeting fits", n > 0 && (size_t)n < sizeof greeting);
    check_answers("greet", mt_eval(m, E("repr", E("py-call", E("python_import_file.greet", T(name))))), T(greeting));
    check_answers("add", mt_eval(m, E("py-call", E("python_import_file.add", 10, 20))), N(10 + 20));
    return done(m);
}
