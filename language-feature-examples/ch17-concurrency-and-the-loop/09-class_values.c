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
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <math.h>

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
    metta *m = open_engine();
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
    check_answers("a method reads the fields", mt_eval(m, E("Point-norm", point_atom(p))), mt_real(norm(p)));
    check_answers("add is fieldwise", mt_eval(m, E("Point-add", point_atom(a), point_atom(b))), point_atom(add(a, b)));
    const point places[] = { { 0, 0 }, { 0, 4 }, { 3, 4 } };
    for (size_t i = 0; i < sizeof places / sizeof *places; i++)
        check_answers("a keyword case", mt_eval(m, E("Point-quadrant", point_atom(places[i]))), S(quadrant(places[i])));
    const point sum = add(a, b);
    check_answers("equal fields are one value", mt_eval(m, E("==", E("Point-add", point_atom(a), point_atom(b)), point_atom(sum))),
                  B(same(add(a, b), sum)));
    mt_space_close(class_space);
    return done(m);
}
