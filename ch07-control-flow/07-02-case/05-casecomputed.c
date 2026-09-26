/* Purpose: cases as a value. switch hands its cases to case as an argument,
 *   so C can build them: the case lists here come from C tables of keys and
 *   answers. Written out or handed over, the same cases answer the same; a
 *   key with no answers takes Empty on both paths, and a key that answers
 *   but matches nothing answers nothing on both. A value that is not a list
 *   of pairs is refused, which C sees as MT_ERROR on the cursor, while the
 *   same form written out is data and reduces to itself.
 * Guarantees: all eleven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("switch", mt_add(m, E("=", E("switch", V("value"), V("cases")), E("case", V("value"), V("cases")))));
    require("numbered-cases", mt_add(m, E("=", E("numbered-cases"), E("cons-atom", E(1, "one"), E(E(2, "two"))))));
    require("key-of-nothing", mt_add(m, E("=", E("key-of-nothing", V("cases")), E("case", E("empty"), V("cases")))));
    require("one-case", mt_add(m, E("=", E("one-case", V("pair")), E("case", 1, E(V("pair"))))));

    assert(answers_are(mt_eval(m, E("switch", 2, CASES(NUMBERS))), E("two")) && "handed over");
    assert(answers_are(mt_eval(m, E("case", 2, CASES(NUMBERS))), E("two")) && "written out");
    assert(answers_are(mt_eval(m, E("switch", 1, E("numbered-cases"))), E("one")) && "built by the program, first key");
    assert(answers_are(mt_eval(m, E("switch", 2, E("numbered-cases"))), E("two")) && "and second");
    assert(answers_are(mt_eval(m, E("key-of-nothing", CASES(WITH_DEFAULT))), E("none")) && "no answers takes Empty, handed over");
    assert(answers_are(mt_eval(m, E("case", E("empty"), CASES(WITH_DEFAULT))), E("none")) && "and written out");
    assert(!mt_first(mt_eval(m, E("switch", 9, CASES(WITH_DEFAULT)))) && mt_ok() && "an unmatched key answers nothing, handed over");
    assert(!mt_first(mt_eval(m, E("case", 9, CASES(WITH_DEFAULT)))) && mt_ok() && "and written out");
    assert(answers_are(mt_eval(m, E("one-case", E(1, "hit"))), E("hit")) && "one pair on its own");

    mt_clear();
    mt_list refused = mt_all(mt_eval(m, E("switch", 1, "foo")));
    assert(refused.len == 0 && mt_error() == MT_ERROR && "cases that are not pairs are refused");
    mt_list_free(refused);
    mt_clear();
    assert(answers_are(mt_eval(m, E("case", 1, "foo")), E(E("case", 1, "foo"))) && "written out, the name is data");
    mt_close(m);
    return 0;
}
