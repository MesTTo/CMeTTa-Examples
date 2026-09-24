/* Purpose: the structural operations compiled Python shares. C keeps the
 *   alternatives as a table and collapse answers them in order, repeats
 *   kept; let* destructures a shape, whose parts C reverses itself; case
 *   selects a branch by pattern and then continues, and an answerless
 *   subject takes the Empty branch, not the first pattern; and metatype()
 *   names a symbol as get-metatype does.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

static const int64_t alternatives[] = { 1, 2, 2 };

int main(void)
{
    metta *m = open_engine();
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

    check_answers("collapse keeps order and repeats", mt_eval(m, E("collected")), mt_exprv(3, collected));
    mt_atom *shape = E(1, E(2, 3));
    const mt_atom *left = mt_at(shape, 0), *middle = mt_at(mt_at(shape, 1), 0), *right = mt_at(mt_at(shape, 1), 1);
    check_answers("let* destructures", mt_eval(m, E("unpack", mt_keep(shape))), E(mt_keep(right), mt_keep(middle), mt_keep(left)));
    mt_drop(shape);
    check_answers("a matching branch, then the rest", mt_eval(m, E("collapse", E("selected", E("Box", 7)))), E(7, "Done"));
    check_answers("the fallback branch, then the rest", mt_eval(m, E("collapse", E("selected", "Other"))), E("Miss", "Done"));
    check_answers("no answer takes the Empty branch", mt_eval(m, E("absent")), S("Missing"));
    mt_atom *tag = S("tag");
    check_answers("a symbol's metatype", mt_eval(m, E("metatype", mt_keep(tag))), S(metatype(tag)));
    mt_drop(tag);
    return done(m);
}
