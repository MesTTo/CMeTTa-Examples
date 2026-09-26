/* Purpose: a native structure held and used from MeTTa without becoming
 *   text. The original's vector is an SWI blob built by handle.c; in C it is
 *   a C struct wrapped once by mt_object, released by its own function when
 *   the last owner lets go, and printed by mt_repr as what it is, never its
 *   contents. vector-new, vector-nth, vector-bump and vector-length are C
 *   functions over the struct, published with mt_def; bump-thrice is the
 *   original's equation over them, built as an atom, so three bumps
 *   through three MeTTa calls land on the one C buffer. The handle is an
 *   ordinary grounded value that compares by identity.
 * Guarantees: all five guarded claims of the original hold, each also read
 *   from the C side [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* The metatype get-metatype answers for an atom of this kind: Symbol for a
   symbol and for a space, which the engine names by a symbol; Variable;
   Expression, the empty one included; Grounded for every value. */
static inline const char *metatype(const mt_atom *atom)
{
    switch (mt_kind_of(atom)) {
    case MT_SYMBOL:
    case MT_SPACE: return "Symbol";
    case MT_VARIABLE: return "Variable";
    case MT_EXPR: return "Expression";
    default: return "Grounded";
    }
}

typedef struct vector {
    size_t length;
    int64_t *items;
} vector;

static void release_vector(void *value)
{
    vector *v = value;
    free(v->items);
    free(v);
}

static const char *show_vector(void *value, void *user)
{
    static char text[40];
    (void)user;
    snprintf(text, sizeof text, "<vector %zu>", ((vector *)value)->length);
    return text;
}

/* The vector a call's argument holds, or NULL for any other atom. */
static vector *vector_of(const mt_atom *a)
{
    const char *type = mt_type(a);
    return type && strcmp(type, "vector") == 0 ? mt_value(a) : NULL;
}

/* (vector-new n): 0 .. n-1. */
static mt_status vector_new(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 0)) != MT_INT || mt_int(mt_arg(call, 0)) < 0) return mt_fail(call, "vector-new wants a length");
    const size_t n = (size_t)mt_int(mt_arg(call, 0));
    vector *v = malloc(sizeof *v);
    int64_t *items = v ? malloc((n ? n : 1) * sizeof *items) : NULL;
    if (!items) {
        free(v);
        return mt_error_set(MT_NOMEM, "vector-new has no room");
    }
    for (size_t i = 0; i < n; i++) items[i] = (int64_t)i;
    *v = (vector){ n, items };
    mt_atom *handle = mt_object(v, "vector", release_vector);
    return handle ? mt_answer(call, handle) : mt_error();
}

/* The element an index names, or NULL when the index is out of range. */
static int64_t *element(mt_call *call)
{
    vector *v = vector_of(mt_arg(call, 0));
    if (!v || mt_kind_of(mt_arg(call, 1)) != MT_INT) return NULL;
    const int64_t i = mt_int(mt_arg(call, 1));
    return i >= 0 && (size_t)i < v->length ? &v->items[i] : NULL;
}

static mt_status vector_nth(mt_call *call, void *user)
{
    (void)user;
    const int64_t *slot = element(call);
    return slot ? mt_answer(call, N(*slot)) : mt_fail(call, "vector-nth wants a vector and an index in it");
}

/* Mutates through the handle, so the state is the native one. */
static mt_status vector_bump(mt_call *call, void *user)
{
    (void)user;
    int64_t *slot = element(call);
    return slot ? mt_answer(call, N(++*slot)) : mt_fail(call, "vector-bump wants a vector and an index in it");
}

static mt_status vector_length(mt_call *call, void *user)
{
    (void)user;
    const vector *v = vector_of(mt_arg(call, 0));
    return v ? mt_answer(call, N((int64_t)v->length)) : mt_fail(call, "vector-length wants a vector");
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    /* The original's own imports: its loader needs lib_import and its guard
       lib_file, and C needs neither, but &self holds what they define on
       both sides. */
    static const char *const libraries[] = { "lib_import", "lib_file" };
    for (size_t i = 0; i < sizeof libraries / sizeof *libraries; i++)
        require(libraries[i], mt_one_truth(mt_eval(m, E("import!", "&self", E("library", libraries[i])))));
    require("print a vector as what it is", mt_repr(m, "vector", show_vector, NULL));
    static const struct { const char *name; size_t arity; enum mt_effect_class effect; mt_fn fn; } published[] = {
        { "vector-new", 1, MT_EFFECT_CLASS_WRITES_STATE, vector_new },
        { "vector-nth", 2, MT_EFFECT_CLASS_READ_ONLY_LOOKUP, vector_nth },
        { "vector-bump", 2, MT_EFFECT_CLASS_WRITES_STATE, vector_bump },
        { "vector-length", 1, MT_EFFECT_CLASS_READ_ONLY_LOOKUP, vector_length },
    };
    for (size_t i = 0; i < sizeof published / sizeof *published; i++)
        require(published[i].name, mt_def(m, (mt_op){ .name = published[i].name, .arity = published[i].arity,
                                                       .effect = published[i].effect, .fn = published[i].fn }));
    require("bump-thrice", mt_add(m, E("=", E("bump-thrice"),
                                      E("let", V("v"), E("vector-new", 4),
                                        E("progn", E("vector-bump", V("v"), 0), E("vector-bump", V("v"), 0), E("vector-bump", V("v"), 0))))));

    assert(answers_are(mt_eval(m, E("vector-length", E("vector-new", 1000))), E(1000)) && "a thousand elements are one value");
    assert(answers_are(mt_eval(m, E("vector-nth", E("vector-new", 1000), 700)), E(700)) && "reading one is a call into C");
    assert(answers_are(mt_eval(m, E("bump-thrice")), E(3)) && "three bumps land on one buffer");

    mt_atom *handle = mt_first(mt_eval(m, E("vector-new", 1)));
    require("a handle", handle != NULL && vector_of(handle) != NULL);
    assert(answers_are(mt_eval(m, E("get-metatype", mt_keep(handle))), E(S(metatype(handle)))) && "it is grounded");
    assert(answers_are(mt_eval(m, E("==", mt_keep(handle), mt_keep(handle))), E(B(mt_eq(handle, handle)))) && "and equal to itself");
    mt_drop(handle);
    mt_close(m);
    return 0;
}
