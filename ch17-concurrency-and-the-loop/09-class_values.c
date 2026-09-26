/* Purpose: a C struct is a value, as a frozen Python dataclass is. A point is
 *   the constructor term (Point x y), so C marshals its struct into that term
 *   and back, and each method is a C function over the struct published
 *   under the class's prefix, which is the only namespace C has: Point-x and
 *   Point-y read a field, Point-norm is the square root of the squared
 *   fields, Point-add adds fieldwise, and Point-quadrant is a C conditional
 *   over which fields are zero. The class's space &Point holds the
 *   constructor's type, which &self reads through (from &Point). Equality is
 *   the term's own, so the engine's == on two points agrees with C comparing
 *   their fields.
 * Guarantees: all six claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 09-class_values.c $(pkg-config --cflags --libs cmetta) -lm
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

typedef struct point {
    int64_t x, y;
} point;

static mt_atom *point_atom(point p) { return E("Point", p.x, p.y); }

/* The struct a (Point x y) term holds, or false for any other atom. */
static bool as_point(const mt_atom *a, point *p)
{
    if (mt_kind_of(a) != MT_EXPR || mt_len(a) != 3 || mt_kind_of(mt_at(a, 0)) != MT_SYMBOL || strcmp(mt_name(mt_at(a, 0)), "Point") != 0 ||
        mt_kind_of(mt_at(a, 1)) != MT_INT || mt_kind_of(mt_at(a, 2)) != MT_INT)
        return false;
    *p = (point){ mt_int(mt_at(a, 1)), mt_int(mt_at(a, 2)) };
    return true;
}

static int64_t point_x(point p) { return p.x; }
static int64_t point_y(point p) { return p.y; }
static double norm(point p) { return sqrt((double)(p.x * p.x + p.y * p.y)); }
static point add(point a, point b) { return (point){ a.x + b.x, a.y + b.y }; }
static const char *quadrant(point p) { return p.x == 0 && p.y == 0 ? "origin" : p.x == 0 ? "axis" : "plane"; }
static bool same(point a, point b) { return a.x == b.x && a.y == b.y; }

/* The published methods: each reads its receiver, and a point argument, as
   structs. An argument that is not a point has no answer. */
typedef enum { FIELD_X, FIELD_Y, NORM, ADD, QUADRANT } method;

static mt_status call_method(mt_call *call, void *user)
{
    point self, other;
    if (!as_point(mt_arg(call, 0), &self)) return MT_FAIL;
    switch ((method)(intptr_t)user) {
    case FIELD_X: return mt_answer(call, N(point_x(self)));
    case FIELD_Y: return mt_answer(call, N(point_y(self)));
    case NORM: return mt_answer(call, mt_real(norm(self)));
    case ADD: return as_point(mt_arg(call, 1), &other) ? mt_answer(call, point_atom(add(self, other))) : MT_FAIL;
    case QUADRANT: break;
    }
    return mt_answer(call, S(quadrant(self)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_space *class_space = mt_space_open(m, "&Point");
    require("open &Point", class_space != NULL);
    require("(: Point (-> Number Number Point))", mt_add(class_space, E(":", "Point", E("->", "Number", "Number", "Point"))));
    const struct { const char *name; size_t arity; method method; } methods[] = {
        { "Point-x", 1, FIELD_X }, { "Point-y", 1, FIELD_Y }, { "Point-norm", 1, NORM },
        { "Point-add", 2, ADD },   { "Point-quadrant", 1, QUADRANT },
    };
    for (size_t i = 0; i < sizeof methods / sizeof *methods; i++)
        require(methods[i].name, mt_def(m, (mt_op){ .name = methods[i].name, .arity = methods[i].arity, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL,
                                                    .fn = call_method, .user = (void *)(intptr_t)methods[i].method }));
    require("(from &Point)", mt_add(m, E("from", mt_spaceref(mt_space_name(class_space)))));

    const point p = { 3, 4 }, a = { 1, 2 }, b = { 3, 4 };
    assert(answers_are(mt_eval(m, E("Point-norm", point_atom(p))), E(mt_real(norm(p)))) && "a method reads the fields");
    assert(answers_are(mt_eval(m, E("Point-add", point_atom(a), point_atom(b))), E(point_atom(add(a, b)))) && "add is fieldwise");
    const point places[] = { { 0, 0 }, { 0, 4 }, { 3, 4 } };
    for (size_t i = 0; i < sizeof places / sizeof *places; i++)
        assert(answers_are(mt_eval(m, E("Point-quadrant", point_atom(places[i]))), E(S(quadrant(places[i])))) && "a keyword case");
    const point sum = add(a, b);
    assert(answers_are(mt_eval(m, E("==", E("Point-add", point_atom(a), point_atom(b)), point_atom(sum))), E(B(same(add(a, b), sum))))
           && "equal fields are one value");
    mt_space_close(class_space);
    mt_close(m);
    return 0;
}
