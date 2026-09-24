/* Purpose: the clock, its written form and the process's arguments. C
 *   reads its own clock with time(), which is past 1700000000 too, and
 *   writes each format with strftime into its own buffer, whose length is the
 *   length the engine's rendering has; a format's answer is a name, not a
 *   text. Reading an argument past the end answers nothing, and C, holding
 *   argc of at least one, knows argument 0 is there.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#include "common.h"
#include <time.h>

/* The length strftime writes for a format at the current local time. */
static int64_t rendered_length(const char *format)
{
    char out[128];
    time_t now = time(NULL);
    struct tm local;
    require("the local time", localtime_r(&now, &local) != NULL);
    return (int64_t)strftime(out, sizeof out, format, &local);
}

int main(int argc, char **argv)
{
    (void)argv;
    metta *m = open_engine();
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));
    const double epoch = 1700000000.0;
    check_answers("the clock is past the epoch C reads past", mt_eval(m, E(">", E("current-time"), epoch)), B((double)time(NULL) > epoch));
    check_answers("and never goes back", mt_eval(m, E("<=", E("current-time"), E("current-time"))), B(true));
    check_answers("a format with no directive", mt_eval(m, E("format-time", T("abc"))), S("abc"));
    check_answers("answers a name, not a text", mt_eval(m, E("==", E("format-time", T("abc")), T("abc"))), B(false));
    const char *formats[] = { "a literal", "", "%%", "%Y", "%Y-%m-%d", "%H:%M:%S" };
    for (size_t i = 0; i < sizeof formats / sizeof *formats; i++)
        check_answers(formats[i], mt_eval(m, E("string-length", E("format-time", T(formats[i])))), N(rendered_length(formats[i])));
    for (int64_t index = 999; index >= -1; index -= 1000)
        check_answers("past the end, nothing", mt_eval(m, E("collapse", E("argv", index))), mt_unit());
    check_answers("an argument equals itself", mt_eval(m, E("==", E("argv", 0), E("argv", 0))), B(argc > 0));
    require("define argument-or", mt_add(m, E("=", E("argument-or", V("index"), V("default")),
        E("let", V("found"), E("collapse", E("argv", V("index"))), E("if", E("==", V("found"), mt_unit()), V("default"), E("car-atom", V("found")))))));
    check_answers("a default for an absent argument", mt_eval(m, E("argument-or", 999, "no-such-argument")), S("no-such-argument"));
    check_answers("and none for a present one", mt_eval(m, E("==", E("argument-or", 0, "no-such-argument"), "no-such-argument")), B(!(argc > 0)));
    return done(m);
}
