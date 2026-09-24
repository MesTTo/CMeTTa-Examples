/* Purpose: not-provable over nested case arms. case-band answers True for
 *   90 and False for 40 through a nested case, and nothing for any other
 *   key; case-default answers False for every key but 90. C keeps each as a
 *   table of arms and decides each negation itself: (not-provable X) holds
 *   exactly when X has no True answer, so a False answer and no answer at
 *   all both make it hold.
 * Guarantees: all seven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

typedef enum { NONE, TRUE_, FALSE_ } answer;

/* case-band: 90 is True, and 40, inside the nested case, False. */
static answer band(int64_t key) { return key == 90 ? TRUE_ : key == 40 ? FALSE_ : NONE; }
/* case-default: the nested case gains an arm answering False for the rest. */
static answer fallback(int64_t key) { return key == 90 ? TRUE_ : FALSE_; }

int main(void)
{
    metta *m = open_engine();
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
        check_answers("case-band's negation", mt_eval(m, E("not-provable", E("case-band", keys[i]))), B(band(keys[i]) != TRUE_));
    check_none("an arm no key matches answers nothing", mt_eval(m, E("case-band", 55)));
    require("C agrees 55 has no arm", band(55) == NONE);
    for (size_t i = 0; i < 3; i++)
        check_answers("case-default's negation", mt_eval(m, E("not-provable", E("case-default", keys[i]))),
                      B(fallback(keys[i]) != TRUE_));
    return done(m);
}
