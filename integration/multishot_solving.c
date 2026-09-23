/* Purpose: extend a reachability program between solves and toggle external facts.
 * Owns resources: scopes template writes in a transaction and releases its runtime.
 * Guarantees: shortest horizon is three and failed grounding rolls back
 *   [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
typedef struct part { int horizon; } part;
static mt_status ground(metta *m, void *user)
{
    int t = ((part *)user)->horizon;
    mt_atom *head = mt_expr("reach", mt_var("x"), t);
    mt_atom *body = mt_expr("match", mt_spaceref("&self"), mt_parse("(edge $y $x)"),
        mt_expr("once", mt_expr("reach", mt_var("y"), t-1)));
    return mt_add(m, mt_expr("=", head, body)) ? MT_OK : mt_error();
}
static mt_status abort_part(metta *m, void *user)
{ (void)user; if (!mt_add(m, mt_expr("transient", 1))) return mt_error(); return MT_FAIL; }
int main(void)
{
    metta *m = open_engine();
    check("base program", mt_do(m,
      "!(add-atom &metta (dispatch-policy reach NoMatchEnum NoMatchFail)) "
      "(edge a b) (edge b c) (edge c d) (= (reach a 0) True)"));
    part p = {0};
    for (;;) {
        mt_list rows = mt_all(mt_eval(m, mt_expr("reach", "d", p.horizon)));
        check("solve succeeds as an operation", mt_ok()); bool reached = rows.len != 0; mt_list_free(rows);
        if (reached) break;
        ++p.horizon; check("graph horizon", p.horizon <= 3);
        check("ground next part atomically", mt_transaction(m, ground, &p) == MT_OK);
    }
    check("shortest horizon", p.horizon == 3);
    check("assign external", mt_add(m, mt_expr("blocked", "c")));
    check_answers("external visible", mt_match(m, mt_parse("(blocked $x)")), "(blocked c)");
    check("withdraw external", mt_del(m, mt_expr("blocked", "c")));
    check_answers("external gone", mt_match(m, mt_parse("(blocked $x)")), "");
    check("aborted template", mt_transaction(m, abort_part, NULL) == MT_FAIL);
    check_answers("aborted grounding leaves no fact", mt_match(m, mt_parse("(transient $x)")), "");
    return done(m, "multishot_solving");
}
