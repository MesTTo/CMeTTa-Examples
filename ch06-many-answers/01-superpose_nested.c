/* Purpose: a superposition flattens. progme collapses four nestings of two
 *   alternative sets that C holds as arrays of names; however they nest,
 *   each collapse answers the sets' concatenation, which C builds by
 *   appending the arrays and compares with the engine's one answer.
 * Guarantees: the original's claim holds [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;

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
    assert(answers_are(mt_eval(m, E("progme")), E(E(names(appended, COUNT(appended)), names(ABC, COUNT(ABC)),
                                                    names(ABC, COUNT(ABC)), names(appended, COUNT(appended)))))
           && "every nesting collapses to the concatenation");
    mt_close(m);
    return 0;
}
