/* Purpose: one payload imported into two spaces, each its own copy. C keeps
 *   a model of how many markers &import-space-a holds and moves it by each
 *   step: an import adds one, the caller's own add one more, an undo of the
 *   import takes back only the import's, a second undo nothing, and a
 *   re-import one. After each step the space must hold what the model says.
 *   Each space evaluates the payload's function, the caller that imported
 *   nothing sees the name as data, the imports view is data a match reads,
 *   and undoing the import in one space leaves the other its program.
 * Guarantees: all eleven claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "modules.h"

#define PAYLOAD MODULES_FIXTURES "imports/overhaul/space_payload"

static const char *const spaces[] = { "&import-space-a", "&import-space-b" };

static void holds_markers(metta *m, const char *claim, const char *space, int64_t markers)
{
    int64_t n = 0;
    mt_each (row, mt_eval(m, E("match", mt_spaceref(space), E("import-space-marker"), "present"))) n++;
    check_int(claim, n, markers);
}

/* (metta (import-space-function) %Undefined% space): the payload's answer
   where the space holds its program, the call itself where it does not. */
static void evaluates(metta *m, const char *claim, const char *space, bool has_program)
{
    check_answers(claim, mt_eval(m, E("metta", E("import-space-function"), "%Undefined%", mt_spaceref(space))),
                  has_program ? S("one-result") : E("import-space-function"));
}

static mt_answers *imports_view(metta *m, const char *space, mt_atom *answer)
{
    return mt_eval(m, E("match", E("imports", mt_spaceref(space)), E("import", V("path")), answer));
}

int main(void)
{
    metta *m = open_engine();
    import_library(m, "lib_import");
    mt_space *opened[2];
    for (size_t i = 0; i < 2; i++) {
        opened[i] = mt_space_open(m, spaces[i]);
        require("a new space", opened[i] != NULL);
    }
    for (size_t i = 0; i < 2; i++) require("import the payload", mt_one_truth(mt_eval(m, E("import!", mt_spaceref(spaces[i]), S(PAYLOAD)))));

    int64_t markers_in_a = 1;
    holds_markers(m, "the import wrote one marker", spaces[0], markers_in_a);
    holds_markers(m, "in each space", spaces[1], 1);
    for (size_t i = 0; i < 2; i++) evaluates(m, "each space runs its own copy", spaces[i], true);
    check_answers("the caller imported nothing", mt_eval(m, E("import-space-function")), E("import-space-function"));
    check_answers("imports are data in a view", imports_view(m, spaces[0], S("imported")), "imported");

    require("the caller's own marker", mt_add(opened[0], E("import-space-marker")));
    markers_in_a++;
    require("undo the import", mt_one_truth(mt_eval(m, E("unimport!", mt_spaceref(spaces[0]), S(PAYLOAD)))));
    markers_in_a--;
    holds_markers(m, "undo takes back only the import's occurrence", spaces[0], markers_in_a);
    check_none("and the view forgets it", imports_view(m, spaces[0], V("path")));

    require("undo again", mt_one_truth(mt_eval(m, E("unimport!", mt_spaceref(spaces[0]), S(PAYLOAD)))));
    evaluates(m, "the other space keeps its program", spaces[1], true);
    evaluates(m, "and this one has none", spaces[0], false);

    require("import again", mt_one_truth(mt_eval(m, E("import!", mt_spaceref(spaces[0]), S(PAYLOAD)))));
    markers_in_a++;
    holds_markers(m, "a re-import restores one occurrence beside the caller's", spaces[0], markers_in_a);
    for (size_t i = 0; i < 2; i++) mt_space_close(opened[i]);
    return done(m);
}
