/* Purpose: the finger tree from lib_datastructures, held against a C deque.
 *   Every sequence of pushes, pops and concatenations the original performs
 *   on the tree, C performs on a ring buffer of atoms, and the tree must
 *   read back as the buffer does: from both ends, drained, and joined.
 * Guarantees: all fifteen claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_datastructures",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datastructures")))));
    mt_atom *one = N(1), *two = N(2), *three = N(3), *ten = E("a", "b", "c", "d", "e", "f", "g", "h", "i", "j");
    mt_atom *abc = E("a", "b", "c"), *fifteen = E(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    mt_atom *deq = E(4, 5, 6), *zero = N(0), *nine = N(9), *left = E(1, 2, 3, 4, 5), *right = E(6, 7, 8, 9, 10);
    mt_atom *xy = E("x", "y"), *a = S("a"), *seven = E("b", "c", "d", "e", "f", "g", "h"), *pairs = E(E("nested", "pair"), "plain");

    deque d = { .n = 0 };
    push_front(&d, two); push_back(&d, three); push_front(&d, one);
    assert(answers_are(mt_eval(m, to_list(E("ft-push-front", 1, E("ft-push-back", 3, E("ft-push-front", 2, E("ft-empty")))))), E(as_list(&d)))
           && "built from both ends");
    deque t = of(ten);
    assert(answers_are(mt_eval(m, to_list(tree(mt_keep(ten)))), E(as_list(&t))) && "from a list and back");
    deque three_letters = of(abc);
    assert(answers_are(mt_eval(m, E("ft-front", tree(mt_keep(abc)))), E(mt_keep(front(&three_letters)))) && "the front");
    assert(answers_are(mt_eval(m, E("ft-back", tree(mt_keep(abc)))), E(mt_keep(back(&three_letters)))) && "the back");
    deque popped = of(abc);
    pop_front(&popped);
    assert(answers_are(mt_eval(m, E("let", E(V("x"), V("rest")), E("ft-pop-front", tree(mt_keep(abc))),
                                   E(V("x"), to_list(V("rest"))))), E(E(mt_keep(front(&three_letters)), as_list(&popped))))
           && "a pop from the front");
    popped = of(abc);
    pop_back(&popped);
    assert(answers_are(mt_eval(m, E("let", E(V("x"), V("rest")), E("ft-pop-back", tree(mt_keep(abc))),
                                   E(V("x"), to_list(V("rest"))))), E(E(mt_keep(back(&three_letters)), as_list(&popped))))
           && "and from the back");
    deque deep = of(fifteen);
    assert(answers_are(mt_eval(m, to_list(tree(mt_keep(fifteen)))), E(as_list(&deep))) && "a deep tree drains in order");
    deque workout = of(deq);
    push_front(&workout, zero); push_back(&workout, nine);
    assert(answers_are(mt_eval(m, to_list(E("ft-push-back", 9, E("ft-push-front", 0, tree(mt_keep(deq)))))), E(as_list(&workout)))
           && "both ends at once");
    deque joined = of(left), rest = of(right);
    append(&joined, &rest);
    assert(answers_are(mt_eval(m, to_list(E("ft-concat", tree(mt_keep(left)), tree(mt_keep(right))))), E(as_list(&joined))) && "concatenation");
    deque empty = { .n = 0 }, pair = of(xy);
    append(&empty, &pair);
    assert(answers_are(mt_eval(m, to_list(E("ft-concat", E("ft-empty"), tree(mt_keep(xy))))), E(as_list(&empty))) && "onto the empty tree");
    pair = of(xy);
    assert(answers_are(mt_eval(m, to_list(E("ft-concat", tree(mt_keep(xy)), E("ft-empty")))), E(as_list(&pair))) && "with the empty tree");
    deque single = { .n = 0 }, others = of(seven);
    push_front(&single, a);
    append(&single, &others);
    assert(answers_are(mt_eval(m, to_list(E("ft-concat", E("ft-push-front", "a", E("ft-empty")), tree(mt_keep(seven))))), E(as_list(&single)))
           && "a singleton and seven");
    assert(answers_are(mt_eval(m, E("ft-is-empty", E("ft-empty"))), E(B(true))) && "the empty tree is empty");
    assert(answers_are(mt_eval(m, E("ft-is-empty", E("ft-push-front", 1, E("ft-empty")))), E(B(false))) && "one element is not");
    deque nested = of(pairs);
    assert(answers_are(mt_eval(m, E("ft-front", tree(mt_keep(pairs)))), E(mt_keep(front(&nested)))) && "an expression element");

    mt_atom *all[] = { one, two, three, ten, abc, fifteen, deq, zero, nine, left, right, xy, a, seven, pairs };
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) mt_drop(all[i]);
    mt_close(m);
    return 0;
}
