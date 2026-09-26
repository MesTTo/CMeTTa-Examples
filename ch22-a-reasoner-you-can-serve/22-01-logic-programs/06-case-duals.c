/* Purpose: not-provable over nested case arms. case-band answers True for
 *   90 and False for 40 through a nested case, and nothing for any other
 *   key; case-default answers False for every key but 90. C keeps each as a
 *   table of arms and decides each negation itself: (not-provable X) holds
 *   exactly when X has no True answer, so a False answer and no answer at
 *   all both make it hold.
 * Guarantees: all seven claims of the original hold
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

typedef enum { NONE, TRUE_, FALSE_ } answer;

/* case-band: 90 is True, and 40, inside the nested case, False. */
static answer band(int64_t key) { return key == 90 ? TRUE_ : key == 40 ? FALSE_ : NONE; }
/* case-default: the nested case gains an arm answering False for the rest. */
static answer fallback(int64_t key) { return key == 90 ? TRUE_ : FALSE_; }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("case-band", mt_add(m, E("=", E("case-band", V("key")),
                                     E("case", V("key"), E(E(90, B(true)),
                                                           E(V("remaining"), E("case", V("remaining"), E(E(40, B(false))))))))));
    require("case-default",
            mt_add(m, E("=", E("case-default", V("key")),
                        E("case", V("key"), E(E(90, B(true)),
                                              E(V("remaining"), E("case", V("remaining"),
                                                                   E(E(40, B(false)), E(V("other"), B(false))))))))));
    static const int64_t keys[] = { 90, 40, 55 };
    for (size_t i = 0; i < 3; i++)
        assert(answers_are(mt_eval(m, E("not-provable", E("case-band", keys[i]))), E(B(band(keys[i]) != TRUE_))) && "case-band's negation");
    assert(!mt_first(mt_eval(m, E("case-band", 55))) && mt_ok() && "an arm no key matches answers nothing");
    require("C agrees 55 has no arm", band(55) == NONE);
    for (size_t i = 0; i < 3; i++)
        assert(answers_are(mt_eval(m, E("not-provable", E("case-default", keys[i]))), E(B(fallback(keys[i]) != TRUE_)))
               && "case-default's negation");
    mt_close(m);
    return 0;
}
