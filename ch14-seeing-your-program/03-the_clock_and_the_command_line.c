/* Purpose: the clock, its written form and the process's arguments. C
 *   reads its own clock with time(), which is past 1700000000 too, and
 *   writes each format with strftime into its own buffer, whose length is the
 *   length the engine's rendering has; a format's answer is a name, not a
 *   text. Reading an argument past the end answers nothing, and C, holding
 *   argc of at least one, knows argument 0 is there.
 * Guarantees: all fifteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_string", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_string")))));
    const double epoch = 1700000000.0;
    assert(answers_are(mt_eval(m, E(">", E("current-time"), epoch)), E(B((double)time(NULL) > epoch))) && "the clock is past the epoch C reads past");
    assert(answers_are(mt_eval(m, E("<=", E("current-time"), E("current-time"))), E(B(true))) && "and never goes back");
    assert(answers_are(mt_eval(m, E("format-time", T("abc"))), E(S("abc"))) && "a format with no directive");
    assert(answers_are(mt_eval(m, E("==", E("format-time", T("abc")), T("abc"))), E(B(false))) && "answers a name, not a text");
    const char *formats[] = { "a literal", "", "%%", "%Y", "%Y-%m-%d", "%H:%M:%S" };
    for (size_t i = 0; i < sizeof formats / sizeof *formats; i++)
        assert(answers_are(mt_eval(m, E("string-length", E("format-time", T(formats[i])))), E(N(rendered_length(formats[i])))) && formats[i]);
    for (int64_t index = 999; index >= -1; index -= 1000)
        assert(answers_are(mt_eval(m, E("collapse", E("argv", index))), E(mt_unit())) && "past the end, nothing");
    assert(answers_are(mt_eval(m, E("==", E("argv", 0), E("argv", 0))), E(B(argc > 0))) && "an argument equals itself");
    require("define argument-or", mt_add(m, E("=", E("argument-or", V("index"), V("default")),
        E("let", V("found"), E("collapse", E("argv", V("index"))), E("if", E("==", V("found"), mt_unit()), V("default"), E("car-atom", V("found")))))));
    assert(answers_are(mt_eval(m, E("argument-or", 999, "no-such-argument")), E(S("no-such-argument"))) && "a default for an absent argument");
    assert(answers_are(mt_eval(m, E("==", E("argument-or", 0, "no-such-argument"), "no-such-argument")), E(B(!(argc > 0)))) && "and none for a present one");
    mt_close(m);
    return 0;
}
