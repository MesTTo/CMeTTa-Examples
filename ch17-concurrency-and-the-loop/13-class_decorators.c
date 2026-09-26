/* Purpose: a class's decorators and special methods, each as C writes it. A
 *   property is a C accessor over the struct, a class method takes the class
 *   symbol, a static method has no receiver. total_ordering is composition:
 *   C writes Money's eq and lt, and le and gt are derived from those two,
 *   gt being lt with its operands swapped. __add__ is fieldwise, __len__
 *   counts the stack's items, __iter__ is a C generator over them, and
 *   __call__ is a C closure, the adder's n held beside the function. The
 *   abstract Shape-area is an arrow in &Shape with no C function behind it,
 *   which Square supplies. Every method is published under its class's
 *   prefix and each expectation is the C function's own answer. Each class's
 *   space holds its rows, which &self reads through from.
 * Guarantees: all twelve claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
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

/* Whether a term is (ctor f1 ... fn), and its field i. */
static bool fields(const mt_atom *term, const char *ctor, size_t n)
{
    return mt_kind_of(term) == MT_EXPR && mt_len(term) == n + 1 && mt_kind_of(mt_at(term, 0)) == MT_SYMBOL &&
           strcmp(mt_name(mt_at(term, 0)), ctor) == 0;
}
static int64_t field(const mt_atom *term, size_t i) { return mt_int(mt_at(term, i + 1)); }

/* Circle: a property, a class method and a static method. */
static int64_t circle_area(int64_t r) { return 3 * (r * r); }
static const int64_t unit_radius = 1;
static mt_atom *circle_unit(const char *cls) { return E(cls, unit_radius); }
static int64_t circle_grow(int64_t r, int64_t by) { return r + by; }

/* Money: eq and lt written, le and gt derived. */
typedef bool (*money_order)(int64_t, int64_t);
static bool money_eq(int64_t a, int64_t b) { return a == b; }
static bool money_lt(int64_t a, int64_t b) { return a < b; }
static bool money_le(int64_t a, int64_t b) { return money_lt(a, b) || money_eq(a, b); }
static bool money_gt(int64_t a, int64_t b) { return money_lt(b, a); }

enum { EQ, LT, LE, GT };
static const struct money_method {
    const char *head;
    money_order order;
} money_methods[] = {
    [EQ] = { "Money-eq", money_eq }, [LT] = { "Money-lt", money_lt }, [LE] = { "Money-le", money_le }, [GT] = { "Money-gt", money_gt },
};

/* Vector, Stack, Adder and Square. */
typedef struct vector {
    int64_t x, y;
} vector;
static vector vector_add(vector a, vector b) { return (vector){ a.x + b.x, a.y + b.y }; }
static mt_atom *vector_atom(vector v) { return E("Vector", v.x, v.y); }
static int64_t adder_call(int64_t n, int64_t x) { return n + x; }
static int64_t square_area(int64_t s) { return s * s; }

static mt_status circle_op(mt_call *call, void *user)
{
    (void)user;
    return fields(mt_arg(call, 0), "Circle", 1) ? mt_answer(call, N(circle_area(field(mt_arg(call, 0), 0)))) : MT_FAIL;
}

static mt_status unit_op(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *cls = mt_arg(call, 0);
    return mt_kind_of(cls) == MT_SYMBOL ? mt_answer(call, circle_unit(mt_name(cls))) : MT_FAIL;
}

static mt_status grow_op(mt_call *call, void *user)
{
    (void)user;
    return mt_answer(call, N(circle_grow(mt_int(mt_arg(call, 0)), mt_int(mt_arg(call, 1)))));
}

static mt_status money_op(mt_call *call, void *user)
{
    const mt_atom *a = mt_arg(call, 0), *b = mt_arg(call, 1);
    if (!fields(a, "Money", 1) || !fields(b, "Money", 1)) return MT_FAIL;
    return mt_answer(call, B(((const struct money_method *)user)->order(field(a, 0), field(b, 0))));
}

static mt_status vector_op(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *a = mt_arg(call, 0), *b = mt_arg(call, 1);
    if (!fields(a, "Vector", 2) || !fields(b, "Vector", 2)) return MT_FAIL;
    return mt_answer(call, vector_atom(vector_add((vector){ field(a, 0), field(a, 1) }, (vector){ field(b, 0), field(b, 1) })));
}

static mt_status stack_len_op(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *s = mt_arg(call, 0);
    return fields(s, "Stack", 1) ? mt_answer(call, N((int64_t)mt_len(mt_at(s, 1)))) : MT_FAIL;
}

/* __iter__: a generator over the stack's items, holding its own copy. */
typedef struct items {
    mt_atom *stack;
    size_t next;
} items;

static mt_status next_item(void *state, mt_atom **answer)
{
    items *it = state;
    if (it->next == mt_len(it->stack)) return *answer = NULL, MT_DONE;
    *answer = mt_keep(mt_at(it->stack, it->next++));
    return MT_ROW;
}

static void close_items(void *state)
{
    items *it = state;
    mt_drop(it->stack);
    free(it);
}

static mt_status stack_iter_op(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *s = mt_arg(call, 0);
    if (!fields(s, "Stack", 1)) return MT_FAIL;
    items *it = malloc(sizeof *it);
    if (!it) return mt_error_set(MT_NOMEM, "Stack-iter has no room for its cursor");
    *it = (items){ mt_keep(mt_at(s, 1)), 0 };
    return mt_answer_iter(call, (mt_iterator){ it, next_item, close_items });
}

static mt_status adder_op(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *adder = mt_arg(call, 0);
    return fields(adder, "Adder", 1) ? mt_answer(call, N(adder_call(field(adder, 0), mt_int(mt_arg(call, 1))))) : MT_FAIL;
}

static mt_status square_op(mt_call *call, void *user)
{
    (void)user;
    return fields(mt_arg(call, 0), "Square", 1) ? mt_answer(call, N(square_area(field(mt_arg(call, 0), 0)))) : MT_FAIL;
}

/* A class's space: its rows, and its from in &self. TAKES the rows. */
static void class_space(metta *m, const char *name, mt_atom *const *rows, size_t n)
{
    char space_name[40];
    snprintf(space_name, sizeof space_name, "&%s", name);
    mt_space *s = mt_space_open(m, space_name);
    require("open a class space", s != NULL);
    for (size_t i = 0; i < n; i++) require("a class row", mt_add(s, rows[i]));
    mt_space_close(s);
    require("(from &Class)", mt_add(m, E("from", mt_spaceref(space_name))));
}
#define CLASS(m, name, ...) class_space((m), (name), (mt_atom *[]){ __VA_ARGS__ }, MT_NARG(__VA_ARGS__))

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    const struct { const char *name; size_t arity; mt_fn fn; void *user; } methods[] = {
        { "Circle-area", 1, circle_op, NULL },         { "Circle-unit", 1, unit_op, NULL },
        { "Circle-grow", 2, grow_op, NULL },    { "Vector-add", 2, vector_op, NULL },
        { "Stack-len", 1, stack_len_op, NULL }, { "Stack-iter", 1, stack_iter_op, NULL },
        { "Adder-call", 2, adder_op, NULL },    { "Square-area", 1, square_op, NULL },
    };
    for (size_t i = 0; i < sizeof methods / sizeof *methods; i++)
        require(methods[i].name, mt_def(m, (mt_op){ .name = methods[i].name, .arity = methods[i].arity, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL,
                                                    .fn = methods[i].fn, .user = methods[i].user }));
    for (size_t i = 0; i < sizeof money_methods / sizeof *money_methods; i++)
        require(money_methods[i].head, mt_def(m, (mt_op){ .name = money_methods[i].head, .arity = 2, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL,
                                                          .fn = money_op, .user = (void *)&money_methods[i] }));
    mt_atom *abstract = E(":", "Shape-area", E("->", "Shape", "Number"));
    CLASS(m, "Circle", E(":", "Circle", E("->", "Number", "Circle")));
    CLASS(m, "Money", E(":", "Money", E("->", "Number", "Money")));
    CLASS(m, "Vector", E(":", "Vector", E("->", "Number", "Number", "Vector")));
    CLASS(m, "Stack", E(":", "Stack", E("->", "Expression", "Stack")));
    CLASS(m, "Adder", E(":", "Adder", E("->", "Number", "Adder")));
    CLASS(m, "Shape", E(":", "Shape", "Type"), mt_keep(abstract));
    CLASS(m, "Square", E(":", "Square", E("->", "Number", "Square")), E(":<", "Square", "Shape"));

    const int64_t r = 2, grown = 3;
    assert(answers_are(mt_eval(m, E("Circle-area", E("Circle", r))), E(N(circle_area(r)))) && "a property");
    assert(answers_are(mt_eval(m, E("Circle-unit", "Circle")), E(circle_unit("Circle"))) && "a class method takes the class");
    assert(answers_are(mt_eval(m, E("Circle-grow", r, grown)), E(N(circle_grow(r, grown)))) && "a static method takes no receiver");
    const struct { const struct money_method *method; int64_t a, b; } comparisons[] = {
        { &money_methods[LE], 5, 5 }, { &money_methods[GT], 7, 5 }, { &money_methods[GT], 5, 5 },
    };
    for (size_t i = 0; i < sizeof comparisons / sizeof *comparisons; i++)
        assert(answers_are(mt_eval(m, E(comparisons[i].method->head, E("Money", comparisons[i].a), E("Money", comparisons[i].b))), E(B(comparisons[i].method->order(comparisons[i].a, comparisons[i].b))))
               && "a derived comparison");
    const vector a = { 1, 2 }, b = { 3, 4 };
    assert(answers_are(mt_eval(m, E("Vector-add", vector_atom(a), vector_atom(b))), E(vector_atom(vector_add(a, b)))) && "__add__");
    mt_atom *stack = E("Stack", E(1, 2, 3));
    assert(answers_are(mt_eval(m, E("Stack-len", mt_keep(stack))), E(N((int64_t)mt_len(mt_at(stack, 1))))) && "__len__");
    assert(answers_are(mt_eval(m, E("collapse", E("Stack-iter", mt_keep(stack)))), E(mt_keep(mt_at(stack, 1)))) && "__iter__");
    mt_drop(stack);
    const int64_t n = 10, x = 4, side = 3;
    assert(answers_are(mt_eval(m, E("Adder-call", E("Adder", n), x)), E(N(adder_call(n, x)))) && "__call__");
    assert(answers_are(mt_eval(m, E("Square-area", E("Square", side))), E(N(square_area(side)))) && "the concrete class supplies the abstract method");
    mt_space *shape = mt_space_open(m, "&Shape");
    require("open &Shape", shape != NULL);
    assert(answers_are(mt_match(shape, E(":", "Shape-area", V("arrow"))), E(mt_keep(abstract))) && "which Shape declares as an arrow alone");
    mt_space_close(shape);
    mt_drop(abstract);
    mt_close(m);
    return 0;
}
