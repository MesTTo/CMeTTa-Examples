/* Purpose: a type decides what a C function receives. Two C functions answer
 *   their argument unchanged; one is declared (-> Atom Atom), so it receives
 *   its argument as written, and the other (-> Number Number), so the engine
 *   reduces the argument first. The declarations are atoms like any other.
 * Guarantees: (written-term (+ 20 22)) is (+ 20 22) and
 *   (reduced-value (+ 20 22)) is 42 [tested: make check; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static mt_status same(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, mt_keep(mt_arg(call, 0)));
}

int main(void)
{
    metta *m = open_engine();
    static const struct { const char *name, *type; } functions[] = {
        { "written-term", "Atom" }, { "reduced-value", "Number" },
    };
    for (size_t i = 0; i < 2; i++) {
        require("publish", mt_def(m, (mt_op){ .name = functions[i].name, .arity = 1,
                                              .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = same }));
        require("declare its type",   /* (: name (-> T T)) */
                mt_add(m, E(":", functions[i].name, E("->", functions[i].type, functions[i].type))));
    }
    check_answers("an Atom argument arrives as written",
                  mt_eval(m, E("written-term", E("+", 20, 22))), E("+", 20, 22));
    check_answers("a Number argument arrives reduced",
                  mt_eval(m, E("reduced-value", E("+", 20, 22))), 42);
    for (size_t i = 0; i < 2; i++) require("withdraw", mt_undef(m, functions[i].name));
    return done(m);
}
