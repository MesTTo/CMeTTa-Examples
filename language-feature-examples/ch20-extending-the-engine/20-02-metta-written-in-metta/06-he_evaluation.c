/* Purpose: evaluation from lib_he's side. double is one body over lowering.h's
 *   operators, lowered for the engine and compiled for C; eval, evalc in
 *   &self and chain each answer the sum or product C computes, and
 *   for-each-in-atom maps println! over C's six items, answering the true
 *   println! answers once per item.
 * Guarantees: all four claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define DOUBLE(ADD, x) ADD(x, x)

static const int64_t items[] = { 1, 3, 5, 62, 2, 5 };
#define ITEMS (sizeof items / sizeof *items)

int main(void)
{
    metta *m = open_engine();
    require("import lib_he", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_he")))));
    require("double", mt_lower(m, (double $x), DOUBLE(M_ADD, $x)));
    check_int("eval of a call", mt_one_int(mt_eval(m, E("eval", E("double", 5)))), DOUBLE(C_ADD, 5));
    check_int("evalc in &self", mt_one_int(mt_eval(m, E("evalc", T_ADD(5, 5), mt_spaceref("&self")))), C_ADD(5, 5));
    check_int("chain binds and continues", mt_one_int(mt_eval(m, E("chain", T_ADD(2, 3), V("x"), T_MUL(V("x"), 2)))),
              C_MUL(C_ADD(2, 3), 2));

    mt_atom *list[ITEMS], *printed[ITEMS];
    for (size_t i = 0; i < ITEMS; i++) list[i] = N(items[i]), printed[i] = B(true);
    check_answers("println! answers true once per item",
                  mt_eval(m, E("for-each-in-atom", mt_exprv(ITEMS, list), "println!")), mt_exprv(ITEMS, printed));
    return done(m);
}
