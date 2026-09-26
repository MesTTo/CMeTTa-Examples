/* Purpose: the structural operations compiled Python shares. C keeps the
 *   alternatives as a table and collapse answers them in order, repeats
 *   kept; let* destructures a shape, whose parts C reverses itself; case
 *   selects a branch by pattern and then continues, and an answerless
 *   subject takes the Empty branch, not the first pattern; and metatype()
 *   names a symbol as get-metatype does.
 * Guarantees: all six claims of the original hold
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

/* The metatype get-metatype answers for an atom of this kind: Symbol for a
   symbol and for a space, which the engine names by a symbol; Variable;
   Expression, the empty one included; Grounded for every value. */
static inline const char *metatype(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_SYMBOL:
    case MT_SPACE: return "Symbol";
    case MT_VARIABLE: return "Variable";
    case MT_EXPR: return "Expression";
    default: return "Grounded";
    }
}

static const int64_t alternatives[] = { 1, 2, 2 };

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *collected[3];
    for (size_t i = 0; i < 3; i++) {
        require("an alternative", mt_add(m, E("=", E("alternatives"), alternatives[i])));
        collected[i] = N(alternatives[i]);
    }
    require("collected", mt_add(m, E("=", E("collected"), E("collapse", E("alternatives")))));
    require("unpack", mt_add(m, E("=", E("unpack", V("value")), E("let*", E(E(E(V("left"), E(V("middle"), V("right"))), V("value"))), E(V("right"), V("middle"), V("left"))))));
    require("selected--after-yield-1", mt_add(m, E("=", E("selected--after-yield-1"), E("superpose", E("Done")))));
    require("selected", mt_add(m, E("=", E("selected", V("value")),
        E("superpose", E(E("case", V("value"), E(E(E("Box", V("item")), E("superpose", E(V("item"), E("selected--after-yield-1")))),
                                                 E(V("_"), E("superpose", E("Miss", E("selected--after-yield-1")))))))))));
    require("absent", mt_add(m, E("=", E("absent"), E("case", E("superpose", mt_unit()), E(E(1, "Unexpected"), E("Empty", "Missing"))))));
    require("metatype", mt_add(m, E("=", E("metatype", V("value")), E("get-metatype", V("value")))));

    assert(answers_are(mt_eval(m, E("collected")), E(mt_exprv(3, collected))) && "collapse keeps order and repeats");
    mt_atom *shape = E(1, E(2, 3));
    const mt_atom *left = mt_at(shape, 0), *middle = mt_at(mt_at(shape, 1), 0), *right = mt_at(mt_at(shape, 1), 1);
    assert(answers_are(mt_eval(m, E("unpack", mt_keep(shape))), E(E(mt_keep(right), mt_keep(middle), mt_keep(left)))) && "let* destructures");
    mt_drop(shape);
    assert(answers_are(mt_eval(m, E("collapse", E("selected", E("Box", 7)))), E(E(7, "Done"))) && "a matching branch, then the rest");
    assert(answers_are(mt_eval(m, E("collapse", E("selected", "Other"))), E(E("Miss", "Done"))) && "the fallback branch, then the rest");
    assert(answers_are(mt_eval(m, E("absent")), E(S("Missing"))) && "no answer takes the Empty branch");
    mt_atom *tag = S("tag");
    assert(answers_are(mt_eval(m, E("metatype", mt_keep(tag))), E(S(metatype(tag)))) && "a symbol's metatype");
    mt_drop(tag);
    mt_close(m);
    return 0;
}
