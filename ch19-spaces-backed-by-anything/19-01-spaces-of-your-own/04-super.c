/* Purpose: an override that delegates. &self's store wraps an atom as
 *   (stored atom); &guarded redefines store to refuse bad and let anything
 *   else through with super, the next definition up the space's chain, and
 *   &wrapping redefines the engine's own car-atom around its original. The
 *   equations are the original's, built as atoms and added to each space.
 *   C writes what each space must answer as C: the guard is a C conditional
 *   over the atom's name, the wrapper C's own first element of its array,
 *   and every other space keeps the definition it had. evalc is mt_eval with
 *   the space's handle as its target, and &self's handle is the runtime's.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* What a guarded store answers: the guard's refusal for bad, and what the
   definition above it answers for anything else. */
static mt_atom *guarded_store(const char *atom)
{
    return strcmp(atom, "bad") == 0 ? S("refused") : E("stored", atom);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("&self's store", mt_add(m, E("=", E("store", V("atom")), E("stored", V("atom")))));
    mt_space *guarded = mt_space_open(m, "&guarded");
    require("open &guarded", guarded != NULL);
    require("the guard", mt_add(guarded, E("=", E("store", V("atom")),
                                           E("if", E("==", V("atom"), "bad"), "refused", E("super", E("store", V("atom")))))));

    assert(answers_are(mt_eval(guarded, E("store", "good")), E(guarded_store("good"))) && "the guard lets an ordinary atom through");
    assert(answers_are(mt_eval(guarded, E("store", "bad")), E(guarded_store("bad"))) && "and refuses the one it was written for");
    assert(answers_are(mt_eval(m, E("store", "bad")), E(E("stored", "bad"))) && "the space above is untouched");

    mt_space *wrapping = mt_space_open(m, "&wrapping");
    require("open &wrapping", wrapping != NULL);
    require("the wrapper", mt_add(wrapping, E("=", E("car-atom", V("expr")), E("wrapped", E("super", E("car-atom", V("expr")))))));
    static const int64_t items[] = { 1, 2, 3 };
    assert(answers_are(mt_eval(wrapping, E("car-atom", E(items[0], items[1], items[2]))), E(E("wrapped", items[0])))
           && "super reaches the engine's own car-atom");
    assert(answers_are(mt_eval(m, E("car-atom", E(items[0], items[1], items[2]))), E(items[0])) && "every other space keeps the builtin");
    assert(answers_are(mt_eval(mt_self(m), E("store", "good")), E(E("stored", "good"))) && "and naming &self is evaluating there");
    mt_space_close(wrapping);
    mt_space_close(guarded);
    mt_close(m);
    return 0;
}
