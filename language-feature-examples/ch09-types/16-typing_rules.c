/* Purpose: a user typing rule joins the checker ordinary typed calls use.
 *   Gradual typing admits an undeclared argument, whose type is %Undefined%,
 *   where DemoPayload is asked; C keeps one rule as a struct, adds it, and
 *   derives from that struct the error the refused call answers: the call,
 *   and a BadArgType naming the argument's position, the parameter's type,
 *   the argument's and the rule's refusal, spelled by verdicts.h as every
 *   judge's is. Removing the rule restores the admission.
 * Guarantees: all three claims of the original hold, with its two
 *   unasserted forms checked as well [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "verdicts.h"

/* A typing rule: its name and phase, the argument type it judges, the
   parameter type it judges against, and the reason it refuses. */
static const struct { const char *name, *phase, *argument, *parameter, *reason; } rule = {
    "deny-unknown-demo", "ordinary", "%Undefined%", "DemoPayload", "unknown-demo-is-not-a-payload",
};

int main(void)
{
    metta *m = open_engine();
    require("(: typing-rule-demo (-> DemoPayload Atom))", mt_add(m, E(":", "typing-rule-demo", E("->", rule.parameter, "Atom"))));
    require("(= (typing-rule-demo $value) (seen $value))", mt_add(m, E("=", E("typing-rule-demo", V("value")), E("seen", V("value")))));
    mt_atom *call = E("typing-rule-demo", "unknown-demo"), *seen = E("seen", mt_keep(mt_at(call, 1)));
    check_answers("gradual typing admits it", mt_eval(m, mt_keep(call)), mt_keep(seen));
    check_answers("the rule is added", mt_eval(m, E("add-typing-rule!", rule.name, rule.phase, rule.argument, rule.parameter, refusing(S(rule.reason)))), B(true));
    check_answers("and refuses the same call", mt_eval(m, mt_keep(call)),
                  E("Error", mt_keep(call), E("BadArgType", 1, rule.parameter, rule.argument, E("TypingRuleRefusal", rule.name, rule.reason))));
    check_answers("the rule is removed", mt_eval(m, E("remove-typing-rule!", rule.name)), B(true));
    check_answers("and the admission is back", mt_eval(m, mt_keep(call)), mt_keep(seen));
    mt_drop(call), mt_drop(seen);
    return done(m);
}
