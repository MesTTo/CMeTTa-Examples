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
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "../../ch07-control-flow/07-05-recursion/fib.h"
#include "control_forms.h"

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
    check_answers(claim, mt_eval(m, mt_exprv(n, goal)), mt_exprv(n, want));
}

int main(void)
{
    metta *m = open_engine();
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
    check_answers("compilefib installs fib, and only what ran after that reduces", mt_eval(m, E("compilefib")),
                  mt_exprv(CONTROL_FORMS + 1, answers));

    define_wrappers(m, "after");
    asked_together(m, "wrappers written before fib now reduce", NULL, "before", true);
    asked_together(m, "as the ones written after it do", NULL, "after", true);
    return done(m);
}
