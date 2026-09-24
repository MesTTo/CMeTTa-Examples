/* Purpose: cases as a value. switch hands its cases to case as an argument,
 *   so C can build them: the case lists here come from C tables of keys and
 *   answers. Written out or handed over, the same cases answer the same; a
 *   key with no answers takes Empty on both paths, and a key that answers
 *   but matches nothing answers nothing on both. A value that is not a list
 *   of pairs is refused, which C sees as MT_ERROR on the cursor, while the
 *   same form written out is data and reduces to itself.
 * Guarantees: all eleven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct { mt_atom *(*key)(void); const char *answer; } branch;
static mt_atom *one_(void) { return N(1); }
static mt_atom *two_(void) { return N(2); }
static mt_atom *empty_(void) { return S("Empty"); }

static const branch NUMBERS[] = { { one_, "one" }, { two_, "two" } };
static const branch WITH_DEFAULT[] = { { one_, "one" }, { empty_, "none" } };

/* ((key answer) ...) from a table. */
static mt_atom *cases(const branch *table, size_t n)
{
    mt_atom *pairs[4];
    for (size_t i = 0; i < n; i++) pairs[i] = E(table[i].key(), table[i].answer);
    return mt_exprv(n, pairs);
}
#define CASES(table) cases((table), sizeof (table) / sizeof *(table))

int main(void)
{
    metta *m = open_engine();
    require("switch", mt_lower(m, (switch $value $cases), (case $value $cases)));
    require("numbered-cases", mt_lower(m, (numbered-cases), (cons-atom (1 one) ((2 two)))));
    require("key-of-nothing", mt_lower(m, (key-of-nothing $cases), (case (empty) $cases)));
    require("one-case", mt_lower(m, (one-case $pair), (case 1 ($pair))));

    check_answers("handed over", mt_eval(m, E("switch", 2, CASES(NUMBERS))), "two");
    check_answers("written out", mt_eval(m, E("case", 2, CASES(NUMBERS))), "two");
    check_answers("built by the program, first key", mt_eval(m, E("switch", 1, E("numbered-cases"))), "one");
    check_answers("and second", mt_eval(m, E("switch", 2, E("numbered-cases"))), "two");
    check_answers("no answers takes Empty, handed over", mt_eval(m, E("key-of-nothing", CASES(WITH_DEFAULT))), "none");
    check_answers("and written out", mt_eval(m, E("case", E("empty"), CASES(WITH_DEFAULT))), "none");
    check_none("an unmatched key answers nothing, handed over", mt_eval(m, E("switch", 9, CASES(WITH_DEFAULT))));
    check_none("and written out", mt_eval(m, E("case", 9, CASES(WITH_DEFAULT))));
    check_answers("one pair on its own", mt_eval(m, E("one-case", E(1, "hit"))), "hit");

    mt_clear();
    mt_list refused = mt_all(mt_eval(m, E("switch", 1, "foo")));
    check("cases that are not pairs are refused", refused.len == 0 && mt_error() == MT_ERROR);
    mt_list_free(refused);
    mt_clear();
    check_answers("written out, the name is data", mt_eval(m, E("case", 1, "foo")), E("case", 1, "foo"));
    return done(m);
}
