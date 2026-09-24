/* Purpose: a result type decides whether the result is evaluated. f's
 *   %Undefined% result runs its body's sum, g's Atom result keeps it as
 *   written, and h holds its Atom argument too, so the sum it builds keeps
 *   (+ 1 1) inside. C computes what is evaluated with its own arithmetic and
 *   keeps what is held as the term it built.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    const struct { const char *name, *parameter, *result; } arrows[] = {
        { "f", "Number", "%Undefined%" }, { "g", "Number", "Atom" }, { "h", "Atom", "Atom" },
    };
    for (size_t i = 0; i < 3; i++) {
        require("declare the arrow", mt_add(m, E(":", arrows[i].name, E("->", arrows[i].parameter, arrows[i].result))));
        require("(= (name $x) (+ $x 42))", mt_add(m, E("=", E(arrows[i].name, V("x")), E("+", V("x"), 42))));
    }
    check_answers("an evaluated result", mt_eval(m, E("f", E("+", 1, 1))), N(1 + 1 + 42));
    check_answers("an Atom result keeps the sum", mt_eval(m, E("g", E("+", 1, 1))), E("+", 1 + 1, 42));
    check_answers("and an Atom argument keeps its own", mt_eval(m, E("h", E("+", 1, 1))), E("+", E("+", 1, 1), 42));
    return done(m);
}
