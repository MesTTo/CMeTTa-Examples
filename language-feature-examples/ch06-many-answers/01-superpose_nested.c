/* Purpose: a superposition flattens. progme collapses four nestings of two
 *   alternative sets that C holds as arrays of names; however they nest,
 *   each collapse answers the sets' concatenation, which C builds by
 *   appending the arrays and compares with the engine's one answer.
 * Guarantees: the original's claim holds [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

#define COUNT(array) (sizeof (array) / sizeof *(array))
static const char *const ABC[] = { "a", "b", "c" }, *const XYZ[] = { "x", "y", "z" };

static mt_atom *names(const char *const *list, size_t n)
{
    mt_atom *kids[COUNT(ABC) + COUNT(XYZ)];
    for (size_t i = 0; i < n; i++) kids[i] = mt_sym(list[i]);
    return mt_exprv(n, kids);
}

static mt_atom *superpose(mt_atom *alternatives) { return E("superpose", alternatives); }
static mt_atom *collapse(mt_atom *answers) { return E("collapse", answers); }

int main(void)
{
    metta *m = open_engine();

    /* ((superpose (a b c)) x y z): one set nested, the other's members bare. */
    mt_atom *spliced[1 + COUNT(XYZ)] = { superpose(names(ABC, COUNT(ABC))) };
    for (size_t i = 0; i < COUNT(XYZ); i++) spliced[1 + i] = mt_sym(XYZ[i]);

    require("(= (progme) ...)",
            mt_add(m, E("=", E("progme"),
                        E(collapse(superpose(E(superpose(names(ABC, COUNT(ABC))),
                                               superpose(names(XYZ, COUNT(XYZ)))))),
                          collapse(superpose(names(ABC, COUNT(ABC)))),
                          collapse(superpose(E(superpose(names(ABC, COUNT(ABC)))))),
                          collapse(superpose(mt_exprv(COUNT(spliced), spliced)))))));

    const char *appended[COUNT(ABC) + COUNT(XYZ)];
    memcpy(appended, ABC, sizeof ABC);
    memcpy(appended + COUNT(ABC), XYZ, sizeof XYZ);
    check_answers("every nesting collapses to the concatenation", mt_eval(m, E("progme")),
                  E(names(appended, COUNT(appended)), names(ABC, COUNT(ABC)),
                    names(ABC, COUNT(ABC)), names(appended, COUNT(appended))));
    return done(m);
}
