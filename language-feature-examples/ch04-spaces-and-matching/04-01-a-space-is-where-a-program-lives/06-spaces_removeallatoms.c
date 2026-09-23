/* Purpose: emptying a space. remove-all-atoms takes everything, the library
 *   that defines it included, so a second call and (f 42) both find no
 *   equation left and answer themselves, and mt_count() reads zero.
 * Guarantees: the three claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
    require("import lib_spaces",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_spaces")))));
    require("store a friendship", mt_add(m, E("friend", "tim", "tom")));
    require("(= (f $x) 42)", mt_add(m, E("=", E("f", V("x")), 42)));

    mt_list emptied = mt_all(mt_eval(m, E("remove-all-atoms", "&self")));
    require("empty &self", mt_ok() && emptied.len > 0);
    mt_list_free(emptied);

    /* The answer carries &self as the space it is, so the expectation is
       built with mt_spaceref() rather than as a symbol spelled "&self". */
    check_answers("with its own definition gone, the call answers itself",
                  mt_eval(m, E("remove-all-atoms", "&self")),
                  E("remove-all-atoms", mt_spaceref("&self")));
    check_answers("and so does (f 42)", mt_eval(m, E("f", 42)), E("f", 42));
    check_int("nothing is left", (int64_t)mt_count(m), 0);
    return done(m);
}
