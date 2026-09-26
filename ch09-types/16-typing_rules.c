/* Purpose: a user typing rule joins the checker ordinary typed calls use.
 *   Gradual typing admits an undeclared argument, whose type is %Undefined%,
 *   where DemoPayload is asked; C keeps one rule as a struct, adds it, and
 *   derives from that struct the error the refused call answers: the call,
 *   and a BadArgType naming the argument's position, the parameter's type,
 *   the argument's and the rule's refusal, spelled as every
 *   judge's is. Removing the rule restores the admission.
 * Guarantees: all three claims of the original hold, with its two
 *   unasserted forms checked as well [tested 2026-09-27T00:35:58+10:00:
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

/* The verdict that raises with the words; takes them. */
static inline mt_atom *refusing(mt_atom *words) { return mt_expr("Refuse", words); }

/* A typing rule: its name and phase, the argument type it judges, the
   parameter type it judges against, and the reason it refuses. */
static const struct { const char *name, *phase, *argument, *parameter, *reason; } rule = {
    "deny-unknown-demo", "ordinary", "%Undefined%", "DemoPayload", "unknown-demo-is-not-a-payload",
};

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("(: typing-rule-demo (-> DemoPayload Atom))", mt_add(m, E(":", "typing-rule-demo", E("->", rule.parameter, "Atom"))));
    require("(= (typing-rule-demo $value) (seen $value))", mt_add(m, E("=", E("typing-rule-demo", V("value")), E("seen", V("value")))));
    mt_atom *call = E("typing-rule-demo", "unknown-demo"), *seen = E("seen", mt_keep(mt_at(call, 1)));
    assert(answers_are(mt_eval(m, mt_keep(call)), E(mt_keep(seen))) && "gradual typing admits it");
    assert(answers_are(mt_eval(m, E("add-typing-rule!", rule.name, rule.phase, rule.argument, rule.parameter, refusing(S(rule.reason)))), E(B(true))) && "the rule is added");
    assert(answers_are(mt_eval(m, mt_keep(call)), E(E("Error", mt_keep(call), E("BadArgType", 1, rule.parameter, rule.argument, E("TypingRuleRefusal", rule.name, rule.reason)))))
           && "and refuses the same call");
    assert(answers_are(mt_eval(m, E("remove-typing-rule!", rule.name)), E(B(true))) && "the rule is removed");
    assert(answers_are(mt_eval(m, mt_keep(call)), E(mt_keep(seen))) && "and the admission is back");
    mt_drop(call), mt_drop(seen);
    mt_close(m);
    return 0;
}
