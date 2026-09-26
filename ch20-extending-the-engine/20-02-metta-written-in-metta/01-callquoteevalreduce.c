/* Purpose: call, quote, eval and reduce around (fib 5), asked before fib
 *   exists, from inside the definition that installs it, and after. Each
 *   wrapper is a definition C builds from its row of control_forms.h and
 *   the phase it is asked in, and compilefib adds fib's equation, chapter
 *   7's FIB built as an atom, from inside its own body. C's model says what
 *   each answers. Before fib exists every form hands back the bare call,
 *   except call, which raises and so is not asked, as the original's
 *   before-call-errors-ofc says. Inside compilefib the four wrappers run
 *   after its add-atom and answer as the model does once fib exists, C's
 *   fib(5) where they reduce, while the plain (fib 5) beside them was
 *   compiled before fib existed and stays a call. Afterwards the wrappers
 *   written before fib answer as the ones written after it.
 * Guarantees: all four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/fib.h"
#include "_fixtures/control_forms.h"

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

enum { ARG = 5 };

static mt_atom *fib_call(void) { return T_FIB(N(ARG)); }

/* <first>-<second>, the way the original names its definitions and
   wrappers: (= (before-call) (call-before (call (fib 5)))). */
static const char *named(char *buffer, size_t size, const char *first, const char *second)
{
    snprintf(buffer, size, "%s-%s", first, second);
    return buffer;
}

static void define_wrappers(metta *m, const char *phase)
{
    for (size_t i = 0; i < CONTROL_FORMS; i++) {
        char definition[32], wrapper[32];
        named(definition, sizeof definition, phase, control_forms[i].name);
        named(wrapper, sizeof wrapper, control_forms[i].name, phase);
        require(definition, mt_add(m, E("=", E(definition), E(wrapper, E(control_forms[i].name, fib_call())))));
    }
}

/* What FORM's wrapper in PHASE answers, fib existing or not. */
static mt_atom *wrapped(const control_form *form, const char *phase, bool fib_exists)
{
    char wrapper[32];
    return E(named(wrapper, sizeof wrapper, form->name, phase),
             answered(form, fib_call(), fib_exists ? N(fib(ARG)) : NULL));
}

/* The tuple of PHASE's definitions asked together, HEAD first when there is
   one, and the tuple C says it answers. A form that raises for want of fib
   is left out when fib does not exist. */
static void asked_together(metta *m, const char *claim, const char *head, const char *phase, bool fib_exists)
{
    mt_atom *goal[CONTROL_FORMS + 1], *want[CONTROL_FORMS + 1];
    size_t n = 0;
    if (head) goal[n] = S(head), want[n++] = S(head);
    for (size_t i = 0; i < CONTROL_FORMS; i++) {
        if (!fib_exists && control_forms[i].raises_when_undefined) continue;
        char definition[32];
        goal[n] = E(named(definition, sizeof definition, phase, control_forms[i].name));
        want[n++] = wrapped(&control_forms[i], phase, fib_exists);
    }
    assert(answers_are(mt_eval(m, mt_exprv(n, goal)), E(mt_exprv(n, want))) && claim);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    define_wrappers(m, "before");

    mt_atom *inside[CONTROL_FORMS + 1], *answers[CONTROL_FORMS + 1];
    inside[0] = E("within", fib_call());
    answers[0] = E("within", fib_call());
    for (size_t i = 0; i < CONTROL_FORMS; i++) {
        char wrapper[32];
        inside[i + 1] = E(named(wrapper, sizeof wrapper, control_forms[i].name, "within"), E(control_forms[i].name, fib_call()));
        answers[i + 1] = wrapped(&control_forms[i], "within", true);
    }
    require("compilefib", mt_add(m, E("=", E("compilefib"),
                                        E("let", V("temp"), E("add-atom", mt_spaceref("&self"), fib_equation()),
                                          mt_exprv(CONTROL_FORMS + 1, inside)))));

    asked_together(m, "before fib, every form but call hands the call back", "before-call-errors-ofc", "before", false);
    assert(answers_are(mt_eval(m, E("compilefib")), E(mt_exprv(CONTROL_FORMS + 1, answers)))
           && "compilefib installs fib, and only what ran after that reduces");

    define_wrappers(m, "after");
    asked_together(m, "wrappers written before fib now reduce", NULL, "before", true);
    asked_together(m, "as the ones written after it do", NULL, "after", true);
    mt_close(m);
    return 0;
}
