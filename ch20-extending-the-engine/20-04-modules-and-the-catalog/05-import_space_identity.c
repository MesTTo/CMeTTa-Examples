/* Purpose: one payload imported into two spaces, each its own copy. C keeps
 *   a model of how many markers &import-space-a holds and moves it by each
 *   step: an import adds one, the caller's own add one more, an undo of the
 *   import takes back only the import's, a second undo nothing, and a
 *   re-import one. After each step the space must hold what the model says.
 *   Each space evaluates the payload's function, the caller that imported
 *   nothing sees the name as data, the imports view is data a match reads,
 *   and undoing the import in one space leaves the other its program.
 * Guarantees: all eleven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include "_fixtures/modules.h"

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

#define PAYLOAD MODULES_FIXTURES "imports/overhaul/space_payload"

static const char *const spaces[] = { "&import-space-a", "&import-space-b" };

static void holds_markers(metta *m, const char *claim, const char *space, int64_t markers)
{
    int64_t n = 0;
    mt_each (row, mt_eval(m, E("match", mt_spaceref(space), E("import-space-marker"), "present"))) n++;
    assert(n == markers && claim);
}

/* (metta (import-space-function) %Undefined% space): the payload's answer
   where the space holds its program, the call itself where it does not. */
static void evaluates(metta *m, const char *claim, const char *space, bool has_program)
{
    assert(answers_are(mt_eval(m, E("metta", E("import-space-function"), "%Undefined%", mt_spaceref(space))), E(has_program ? S("one-result") : E("import-space-function")))
           && claim);
}

static mt_answers *imports_view(metta *m, const char *space, mt_atom *answer)
{
    return mt_eval(m, E("match", E("imports", mt_spaceref(space)), E("import", V("path")), answer));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
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
    assert(answers_are(mt_eval(m, E("import-space-function")), E(E("import-space-function"))) && "the caller imported nothing");
    assert(answers_are(imports_view(m, spaces[0], S("imported")), E("imported")) && "imports are data in a view");

    require("the caller's own marker", mt_add(opened[0], E("import-space-marker")));
    markers_in_a++;
    require("undo the import", mt_one_truth(mt_eval(m, E("unimport!", mt_spaceref(spaces[0]), S(PAYLOAD)))));
    markers_in_a--;
    holds_markers(m, "undo takes back only the import's occurrence", spaces[0], markers_in_a);
    assert(!mt_first(imports_view(m, spaces[0], V("path"))) && mt_ok() && "and the view forgets it");

    require("undo again", mt_one_truth(mt_eval(m, E("unimport!", mt_spaceref(spaces[0]), S(PAYLOAD)))));
    evaluates(m, "the other space keeps its program", spaces[1], true);
    evaluates(m, "and this one has none", spaces[0], false);

    require("import again", mt_one_truth(mt_eval(m, E("import!", mt_spaceref(spaces[0]), S(PAYLOAD)))));
    markers_in_a++;
    holds_markers(m, "a re-import restores one occurrence beside the caller's", spaces[0], markers_in_a);
    for (size_t i = 0; i < 2; i++) mt_space_close(opened[i]);
    mt_close(m);
    return 0;
}
