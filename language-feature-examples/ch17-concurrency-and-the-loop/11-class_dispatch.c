/* Purpose: inheritance is a head pattern, and in C it is a function table.
 *   Each class is a row of a C table naming its constructor, its base, its
 *   area, and its describe, NULL for a class that keeps Shape's. The public
 *   heads area and describe are C functions dispatching on the receiver's
 *   constructor through that table, the C spelling of MeTTa's one equation
 *   per defining class. Shape-describe is the base's method, (area-of
 *   <area>), and Circle's describe wraps it, which is super().describe().
 *   Each class's space holds its constructor's type and its :< edge, which
 *   &self reads through from, so get-type widens a Circle to a Shape.
 * Guarantees: all six claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct shape_class shape_class;
struct shape_class {
    const char *name, *base;
    int64_t (*area)(int64_t size);
    mt_atom *(*describe)(const shape_class *c, int64_t size); /* NULL: the base's */
    char area_head[32], describe_head[32];
};

static int64_t circle_area(int64_t r) { return 3 * (r * r); }
static int64_t square_area(int64_t s) { return s * s; }
static mt_atom *shape_describe(const shape_class *c, int64_t size) { return E("area-of", c->area(size)); }
static mt_atom *circle_describe(const shape_class *c, int64_t size) { return E("circle", shape_describe(c, size)); }

static shape_class classes[] = {
    { .name = "Circle", .base = "Shape", .area = circle_area, .describe = circle_describe },
    { .name = "Square", .base = "Shape", .area = square_area },
};
#define CLASSES (sizeof classes / sizeof *classes)

static mt_atom *describe(const shape_class *c, int64_t size) { return (c->describe ? c->describe : shape_describe)(c, size); }

/* The class a receiver (Class size) was built by, or NULL. */
static const shape_class *class_of(const mt_atom *receiver, int64_t *size)
{
    if (mt_kind_of(receiver) != MT_EXPR || mt_len(receiver) != 2 || mt_kind_of(mt_at(receiver, 0)) != MT_SYMBOL) return NULL;
    for (size_t i = 0; i < CLASSES; i++)
        if (strcmp(classes[i].name, mt_name(mt_at(receiver, 0))) == 0) return *size = mt_int(mt_at(receiver, 1)), &classes[i];
    return NULL;
}

typedef enum { AREA, DESCRIBE, BASE_DESCRIBE, OWN_AREA, OWN_DESCRIBE } method;

/* A method's receiver, dispatched through the table. The class-qualified
   heads answer only a receiver of their own class. */
static mt_status call_method(mt_call *call, method which, const shape_class *own)
{
    int64_t size;
    const shape_class *c = class_of(mt_arg(call, 0), &size);
    if (!c || (own && c != own)) return MT_FAIL;
    switch (which) {
    case AREA:
    case OWN_AREA: return mt_answer(call, N(c->area(size)));
    case DESCRIBE: return mt_answer(call, describe(c, size));
    case BASE_DESCRIBE: return mt_answer(call, shape_describe(c, size));
    case OWN_DESCRIBE: break;
    }
    return mt_answer(call, c->describe(c, size));
}

static mt_status area_op(mt_call *call, void *user) { (void)user; return call_method(call, AREA, NULL); }
static mt_status describe_op(mt_call *call, void *user) { (void)user; return call_method(call, DESCRIBE, NULL); }
static mt_status base_describe_op(mt_call *call, void *user) { (void)user; return call_method(call, BASE_DESCRIBE, NULL); }
static mt_status own_area_op(mt_call *call, void *user) { return call_method(call, OWN_AREA, user); }
static mt_status own_describe_op(mt_call *call, void *user) { return call_method(call, OWN_DESCRIBE, user); }

static void publish(metta *m, const char *name, mt_fn fn, void *user)
{
    require(name, mt_def(m, (mt_op){ .name = name, .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = fn, .user = user }));
}

/* A class's space, holding the rows get-type reads, and its from in &self. */
static mt_space *class_space(metta *m, const char *name, mt_atom *type, mt_atom *edge)
{
    char space_name[40];
    snprintf(space_name, sizeof space_name, "&%s", name);
    mt_space *s = mt_space_open(m, space_name);
    require("open a class space", s != NULL);
    require("its type", mt_add(s, type));
    if (edge) require("its base", mt_add(s, edge));
    require("(from &Class)", mt_add(m, E("from", mt_spaceref(space_name))));
    return s;
}

int main(void)
{
    metta *m = open_engine();
    publish(m, "area", area_op, NULL);
    publish(m, "describe", describe_op, NULL);
    publish(m, "Shape-describe", base_describe_op, NULL);
    mt_space *spaces[CLASSES + 1];
    spaces[0] = class_space(m, "Shape", E(":", "Shape", "Type"), NULL);
    for (size_t i = 0; i < CLASSES; i++) {
        shape_class *c = &classes[i];
        snprintf(c->area_head, sizeof c->area_head, "%s-area", c->name);
        publish(m, c->area_head, own_area_op, c);
        if (c->describe) {
            snprintf(c->describe_head, sizeof c->describe_head, "%s-describe", c->name);
            publish(m, c->describe_head, own_describe_op, c);
        }
        spaces[i + 1] = class_space(m, c->name, E(":", c->name, E("->", "Number", c->name)), E(":<", c->name, c->base));
    }

    const shape_class *circle = &classes[0], *square = &classes[1];
    const int64_t r = 2, s = 3;
    check_answers("a circle's area", mt_eval(m, E("area", E(circle->name, r))), N(circle->area(r)));
    check_answers("a square's", mt_eval(m, E("area", E(square->name, s))), N(square->area(s)));
    check_answers("a square keeps Shape's describe", mt_eval(m, E("describe", E(square->name, s))), describe(square, s));
    check_answers("a circle wraps it", mt_eval(m, E("describe", E(circle->name, r))), describe(circle, r));
    check_answers("a Circle is a Circle and a Shape", mt_eval(m, E("collapse", E("get-type", E(circle->name, r)))),
                  E(circle->name, circle->base));
    check_answers("through its one :< edge", mt_match(spaces[1], E(":<", circle->name, V("base"))), E(":<", circle->name, circle->base));

    for (size_t i = 0; i < CLASSES + 1; i++) mt_space_close(spaces[i]);
    return done(m);
}
