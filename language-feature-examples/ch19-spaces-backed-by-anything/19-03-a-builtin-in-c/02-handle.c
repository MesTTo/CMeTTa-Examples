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
 *   from the C side [tested: make twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
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

    check_answers("a thousand elements are one value", mt_eval(m, E("vector-length", E("vector-new", 1000))), 1000);
    check_answers("reading one is a call into C", mt_eval(m, E("vector-nth", E("vector-new", 1000), 700)), 700);
    check_answers("three bumps land on one buffer", mt_eval(m, E("bump-thrice")), 3);

    mt_atom *handle = mt_first(mt_eval(m, E("vector-new", 1)));
    require("a handle", handle != NULL && vector_of(handle) != NULL);
    check_answers("it is grounded", mt_eval(m, E("get-metatype", mt_keep(handle))), S(metatype(handle)));
    check_answers("and equal to itself", mt_eval(m, E("==", mt_keep(handle), mt_keep(handle))), B(mt_eq(handle, handle)));
    mt_drop(handle);
    return done(m);
}
