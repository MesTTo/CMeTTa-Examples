/* Purpose: the finger tree from lib_datastructures, held against a C deque.
 *   Every sequence of pushes, pops and concatenations the original performs
 *   on the tree, C performs on a ring buffer of atoms, and the tree must
 *   read back as the buffer does: from both ends, drained, and joined.
 * Guarantees: all fifteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

/* A deque of atoms on a ring buffer, the structure the tree must agree with. */
enum { RING = 32 };
typedef struct { const mt_atom *item[RING]; size_t first, n; } deque;

static void push_front(deque *d, const mt_atom *x) { d->first = (d->first + RING - 1) % RING; d->item[d->first] = x; d->n++; }
static void push_back(deque *d, const mt_atom *x) { d->item[(d->first + d->n++) % RING] = x; }
static const mt_atom *front(const deque *d) { return d->item[d->first]; }
static const mt_atom *back(const deque *d) { return d->item[(d->first + d->n - 1) % RING]; }
static void pop_front(deque *d) { d->first = (d->first + 1) % RING; d->n--; }
static void pop_back(deque *d) { d->n--; }
static void append(deque *d, const deque *e) { for (size_t i = 0; i < e->n; i++) push_back(d, e->item[(e->first + i) % RING]); }
static deque of(const mt_atom *list) { deque d = { .n = 0 }; for (size_t i = 0; i < mt_len(list); i++) push_back(&d, mt_at(list, i)); return d; }

/* The deque as an expression, front first. */
static mt_atom *as_list(const deque *d)
{
    mt_atom *kids[RING];
    for (size_t i = 0; i < d->n; i++) kids[i] = mt_keep(d->item[(d->first + i) % RING]);
    return mt_exprv(d->n, kids);
}

static mt_atom *tree(mt_atom *list) { return E("ft-from-list", list); }
static mt_atom *to_list(mt_atom *t) { return E("ft-to-list", t); }

int main(void)
{
    metta *m = open_engine();
    require("import lib_datastructures",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datastructures")))));
    mt_atom *one = N(1), *two = N(2), *three = N(3), *ten = E("a", "b", "c", "d", "e", "f", "g", "h", "i", "j");
    mt_atom *abc = E("a", "b", "c"), *fifteen = E(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    mt_atom *deq = E(4, 5, 6), *zero = N(0), *nine = N(9), *left = E(1, 2, 3, 4, 5), *right = E(6, 7, 8, 9, 10);
    mt_atom *xy = E("x", "y"), *a = S("a"), *seven = E("b", "c", "d", "e", "f", "g", "h"), *pairs = E(E("nested", "pair"), "plain");

    deque d = { .n = 0 };
    push_front(&d, two); push_back(&d, three); push_front(&d, one);
    check_answers("built from both ends", mt_eval(m, to_list(E("ft-push-front", 1, E("ft-push-back", 3, E("ft-push-front", 2, E("ft-empty")))))),
                  as_list(&d));
    deque t = of(ten);
    check_answers("from a list and back", mt_eval(m, to_list(tree(mt_keep(ten)))), as_list(&t));
    deque three_letters = of(abc);
    check_answers("the front", mt_eval(m, E("ft-front", tree(mt_keep(abc)))), mt_keep(front(&three_letters)));
    check_answers("the back", mt_eval(m, E("ft-back", tree(mt_keep(abc)))), mt_keep(back(&three_letters)));
    deque popped = of(abc);
    pop_front(&popped);
    check_answers("a pop from the front", mt_eval(m, E("let", E(V("x"), V("rest")), E("ft-pop-front", tree(mt_keep(abc))),
                                                      E(V("x"), to_list(V("rest"))))),
                  E(mt_keep(front(&three_letters)), as_list(&popped)));
    popped = of(abc);
    pop_back(&popped);
    check_answers("and from the back", mt_eval(m, E("let", E(V("x"), V("rest")), E("ft-pop-back", tree(mt_keep(abc))),
                                                   E(V("x"), to_list(V("rest"))))),
                  E(mt_keep(back(&three_letters)), as_list(&popped)));
    deque deep = of(fifteen);
    check_answers("a deep tree drains in order", mt_eval(m, to_list(tree(mt_keep(fifteen)))), as_list(&deep));
    deque workout = of(deq);
    push_front(&workout, zero); push_back(&workout, nine);
    check_answers("both ends at once", mt_eval(m, to_list(E("ft-push-back", 9, E("ft-push-front", 0, tree(mt_keep(deq)))))),
                  as_list(&workout));
    deque joined = of(left), rest = of(right);
    append(&joined, &rest);
    check_answers("concatenation", mt_eval(m, to_list(E("ft-concat", tree(mt_keep(left)), tree(mt_keep(right))))), as_list(&joined));
    deque empty = { .n = 0 }, pair = of(xy);
    append(&empty, &pair);
    check_answers("onto the empty tree", mt_eval(m, to_list(E("ft-concat", E("ft-empty"), tree(mt_keep(xy))))), as_list(&empty));
    pair = of(xy);
    check_answers("with the empty tree", mt_eval(m, to_list(E("ft-concat", tree(mt_keep(xy)), E("ft-empty")))), as_list(&pair));
    deque single = { .n = 0 }, others = of(seven);
    push_front(&single, a);
    append(&single, &others);
    check_answers("a singleton and seven", mt_eval(m, to_list(E("ft-concat", E("ft-push-front", "a", E("ft-empty")), tree(mt_keep(seven))))),
                  as_list(&single));
    check_answers("the empty tree is empty", mt_eval(m, E("ft-is-empty", E("ft-empty"))), B(true));
    check_answers("one element is not", mt_eval(m, E("ft-is-empty", E("ft-push-front", 1, E("ft-empty")))), B(false));
    deque nested = of(pairs);
    check_answers("an expression element", mt_eval(m, E("ft-front", tree(mt_keep(pairs)))), mt_keep(front(&nested)));

    mt_atom *all[] = { one, two, three, ten, abc, fifteen, deq, zero, nine, left, right, xy, a, seven, pairs };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    return done(m);
}
