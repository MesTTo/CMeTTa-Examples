/* Purpose: a type decides what a C function receives. Two C functions answer
 *   their argument unchanged; one is declared (-> Atom Atom), so it receives
 *   its argument as written, and the other (-> Number Number), so the engine
 *   reduces the argument first. The declarations are atoms like any other.
 * Guarantees: (written-term (+ 20 22)) is (+ 20 22) and
 *   (reduced-value (+ 20 22)) is 42 [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

static mt_status same(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, mt_keep(mt_arg(call, 0)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const struct { const char *name, *type; } functions[] = {
        { "written-term", "Atom" }, { "reduced-value", "Number" },
    };
    for (size_t i = 0; i < 2; i++) {
        require("publish", mt_def(m, (mt_op){ .name = functions[i].name, .arity = 1,
                                              .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = same }));
        require("declare its type",   /* (: name (-> T T)) */
                mt_add(m, E(":", functions[i].name, E("->", functions[i].type, functions[i].type))));
    }
    assert(answers_are(mt_eval(m, E("written-term", E("+", 20, 22))), E(E("+", 20, 22)))
           && "an Atom argument arrives as written");
    assert(answers_are(mt_eval(m, E("reduced-value", E("+", 20, 22))), E(42))
           && "a Number argument arrives reduced");
    for (size_t i = 0; i < 2; i++) require("withdraw", mt_undef(m, functions[i].name));
    mt_close(m);
    return 0;
}
