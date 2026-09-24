/* Purpose: an override that delegates. &self's store wraps an atom as
 *   (stored atom); &guarded redefines store to refuse bad and let anything
 *   else through with super, the next definition up the space's chain, and
 *   &wrapping redefines the engine's own car-atom around its original. The
 *   equations are the original's, lowered into each space from C tokens.
 *   C writes what each space must answer as C: the guard is a C conditional
 *   over the atom's name, the wrapper C's own first element of its array,
 *   and every other space keeps the definition it had. evalc is mt_eval with
 *   the space's handle as its target, and &self's handle is the runtime's.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* What a guarded store answers: the guard's refusal for bad, and what the
   definition above it answers for anything else. */
static mt_atom *guarded_store(const char *atom)
{
    return strcmp(atom, "bad") == 0 ? S("refused") : E("stored", atom);
}

int main(void)
{
    metta *m = open_engine();
    require("&self's store", mt_lower(m, (store $atom), (stored $atom)));
    mt_space *guarded = mt_space_open(m, "&guarded");
    require("open &guarded", guarded != NULL);
    require("the guard", mt_lower(guarded, (store $atom), (if (== $atom bad) refused (super (store $atom)))));

    check_answers("the guard lets an ordinary atom through", mt_eval(guarded, E("store", "good")), guarded_store("good"));
    check_answers("and refuses the one it was written for", mt_eval(guarded, E("store", "bad")), guarded_store("bad"));
    check_answers("the space above is untouched", mt_eval(m, E("store", "bad")), E("stored", "bad"));

    mt_space *wrapping = mt_space_open(m, "&wrapping");
    require("open &wrapping", wrapping != NULL);
    require("the wrapper", mt_lower(wrapping, (car-atom $expr), (wrapped (super (car-atom $expr)))));
    static const int64_t items[] = { 1, 2, 3 };
    check_answers("super reaches the engine's own car-atom", mt_eval(wrapping, E("car-atom", E(items[0], items[1], items[2]))),
                  E("wrapped", items[0]));
    check_answers("every other space keeps the builtin", mt_eval(m, E("car-atom", E(items[0], items[1], items[2]))), items[0]);
    check_answers("and naming &self is evaluating there", mt_eval(mt_self(m), E("store", "good")), E("stored", "good"));
    mt_space_close(wrapping);
    mt_space_close(guarded);
    return done(m);
}
