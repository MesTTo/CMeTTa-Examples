/* Purpose: Ask a family relation in every direction under five carriers.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check("family program", mt_do(m, "(Parent Tom Bob) (Parent Bob Ann) "
        "(= (ancestor $x $y) (match &self (Parent $x $y) True)) "
        "(= (ancestor $x $y) (match &self (Parent $x $z) (ancestor $z $y)))"));
    const char *carriers[] = {"counting", "tropical", "prov", "ranked", "prob"};
    const char *identities[] = {"1", "0", "one", "1", "1"};
    const char *queries[] = {"(ancestor Tom Ann)", "(ancestor $x Ann)", "(ancestor Tom $y)", "(ancestor $x $y)"};
    const size_t counts[] = {1,2,2,3};
    for (size_t a = 0; a < sizeof(carriers)/sizeof(carriers[0]); ++a) {
        for (size_t q = 0; q < sizeof(queries)/sizeof(queries[0]); ++q) {
            mt_list rows = mt_all(mt_eval_under(m, mt_sym(carriers[a]), mt_parse(queries[q])));
            check("each direction retains its answer multiplicity", mt_ok() && rows.len == counts[q]);
            for (size_t i = 0; i < rows.len; ++i) {
                check("answer carries coefficient", mt_len(rows.items[i]) == 2);
                check_atom("ancestor proof", mt_at(rows.items[i], 0), "True");
                check_atom("carrier identity", mt_at(rows.items[i], 1), identities[a]);
            }
            mt_list_free(rows);
        }
    }
    return done(m, "family_algebras");
}
