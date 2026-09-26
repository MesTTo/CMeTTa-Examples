/* Purpose: the same algebra over answers and over arrays. unique, union,
 *   intersection and subtraction fold superposed answer streams; C computes
 *   each over arrays of names, as multisets kept in the left side's order,
 *   and the engine's collapsed stream must be that array.
 * Guarantees: all four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>

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

enum { MOST = 16 };
typedef struct { const char *name[MOST]; size_t n; } bag;

static mt_atom *fan(const bag *b)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < b->n; i++) kids[i] = mt_sym(b->name[i]);
    return E("superpose", mt_exprv(b->n, kids));
}

/* Whether `name` is still in `b` among the members not yet `used`, using it. */
static bool take(const bag *b, bool *used, const char *name)
{
    for (size_t i = 0; i < b->n; i++)
        if (!used[i] && strcmp(b->name[i], name) == 0) return used[i] = true;
    return false;
}

static bag unique(const bag *a)
{
    bag out = { .n = 0 };
    for (size_t i = 0; i < a->n; i++) {
        bool seen = false;
        for (size_t j = 0; j < out.n && !seen; j++) seen = strcmp(out.name[j], a->name[i]) == 0;
        if (!seen) out.name[out.n++] = a->name[i];
    }
    return out;
}

static bag union_of(const bag *a, const bag *b)
{
    bag out = *a;
    for (size_t i = 0; i < b->n; i++) out.name[out.n++] = b->name[i];
    return out;
}

/* a's members in order, kept when (intersection) or unless (subtraction) b
   still holds a copy of them. */
static bag against(const bag *a, const bag *b, bool keep_shared)
{
    bag out = { .n = 0 };
    bool used[MOST] = { false };
    for (size_t i = 0; i < a->n; i++)
        if (take(b, used, a->name[i]) == keep_shared) out.name[out.n++] = a->name[i];
    return out;
}

static void check_bag(const char *claim, mt_answers *answers, bag want)
{
    mt_atom *atoms[MOST];
    for (size_t i = 0; i < want.n; i++) atoms[i] = mt_sym(want.name[i]);
    assert(list_is(mt_all(answers), mt_exprv(want.n, atoms)) && claim);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const bag letters = { { "a", "b", "c", "d", "d" }, 5 };
    const bag left = { { "a", "b", "b", "c" }, 4 }, right = { { "b", "c", "c", "d" }, 4 };
    const bag wide = { { "a", "b", "c", "c" }, 4 }, wider = { { "b", "c", "c", "c", "d" }, 5 };

    check_bag("unique keeps first occurrences", mt_eval(m, E("unique", fan(&letters))), unique(&letters));
    check_bag("union keeps every copy", mt_eval(m, E("union", fan(&left), fan(&right))), union_of(&left, &right));
    check_bag("intersection keeps as many as both hold",
              mt_eval(m, E("intersection", fan(&wide), fan(&wider))), against(&wide, &wider, true));
    check_bag("subtraction cancels copy for copy",
              mt_eval(m, E("subtraction", fan(&left), fan(&right))), against(&left, &right, false));
    mt_close(m);
    return 0;
}
