/* Purpose: a restricted space keeps its own computation and gains a
 *   capability only when it is granted. Both spaces are the engine's
 *   (new-space &name (restricted ...)) built as terms, the capability named
 *   from vocabularies.h's SpaceCapability words. &locked's double is a body
 *   lowering.h's operators compile to C and lower to the equation. Its file
 *   read is refused, and a refusal reaches C as the error state every door
 *   reports through, naming the capability it wanted; &reader, granted file,
 *   answers the read, and C decides the answer by asking the file system
 *   itself.
 * Guarantees: all three claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"
#include "spaces.h"
#include <unistd.h>

#define DOUBLE(MUL, x) MUL(x, 2)

static const char *const program =
    "examples/ch19-spaces-backed-by-anything/19-01-spaces-of-your-own/02-restricted_spaces.metta";

int main(void)
{
    metta *m = open_engine();
    const char *file = mt_space_capability_names[MT_SPACE_CAPABILITY_FILE];

    mt_space *locked = new_space(m, "&locked", E("restricted"));
    require("its own double", mt_lower(locked, (double $x), DOUBLE(M_MUL, $x)));
    check_answers("computation is kept", mt_eval(locked, E("double", 21)), DOUBLE(C_MUL, 21));

    mt_clear();
    mt_atom *answered = mt_first(mt_eval(locked, E("exists_file", mt_text(program))));
    check("a file read is refused, naming the capability",
          answered == NULL && mt_error() == MT_ERROR && mt_errmsg() && strstr(mt_errmsg(), file) != NULL);
    mt_drop(answered);
    mt_clear();

    mt_space *reader = new_space(m, "&reader", E("restricted", E("grants", file)));
    check_answers("granted, the read answers what the file system says", mt_eval(reader, E("exists_file", mt_text(program))),
                  B(access(program, F_OK) == 0));
    mt_space_close(reader);
    mt_space_close(locked);
    return done(m);
}
