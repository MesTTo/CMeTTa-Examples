/* Purpose: a restricted space keeps its own computation and gains a
 *   capability only when it is granted. Both spaces are the engine's
 *   (new-space &name (restricted ...)) built as terms, the capability named
 *   from vocabularies.h's SpaceCapability words. &locked's double is a body
 *   the C_ and T_ operators compile to C and build as the equation. Its file
 *   read is refused, and a refusal reaches C as the error state every door
 *   reports through, naming the capability it wanted; &reader, granted file,
 *   answers the read, and C decides the answer by asking the file system
 *   itself.
 * Guarantees: all three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>
#include "_fixtures/spaces.h"
#include <unistd.h>

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

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_MUL(a, b) ((a) * (b))
#define T_MUL(a, b) mt_expr("*", a, b)

#define DOUBLE(MUL, x) MUL(x, 2)

static const char *const program =
    "examples/ch19-spaces-backed-by-anything/19-01-spaces-of-your-own/02-restricted_spaces.metta";

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const char *file = mt_space_capability_names[MT_SPACE_CAPABILITY_FILE];

    mt_space *locked = new_space(m, "&locked", E("restricted"));
    require("its own double", mt_add(locked, E("=", E("double", V("x")), DOUBLE(T_MUL, V("x")))));
    assert(answers_are(mt_eval(locked, E("double", 21)), E(DOUBLE(C_MUL, 21))) && "computation is kept");

    mt_clear();
    mt_atom *answered = mt_first(mt_eval(locked, E("exists_file", mt_text(program))));
    assert(answered == NULL && mt_error() == MT_ERROR && mt_errmsg() && strstr(mt_errmsg(), file) != NULL
           && "a file read is refused, naming the capability");
    mt_drop(answered);
    mt_clear();

    mt_space *reader = new_space(m, "&reader", E("restricted", E("grants", file)));
    assert(answers_are(mt_eval(reader, E("exists_file", mt_text(program))), E(B(access(program, F_OK) == 0)))
           && "granted, the read answers what the file system says");
    mt_space_close(reader);
    mt_space_close(locked);
    mt_close(m);
    return 0;
}
