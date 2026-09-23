/* Purpose: lib_sets, held against sets written in C over the children of an
 *   expression. A set is canonical: its children in the standard order of
 *   terms, which mt_compare and its qsort-shaped mt_order give C, each term
 *   once. set-of is qsort and a pass dropping repeats; membership is bsearch
 *   over the child vector, so it compares terms and never unifies; and the
 *   four combinations are one merge of two sorted arrays keeping some of the
 *   three regions a Venn diagram has: union all three, intersection the
 *   middle, difference the left, symmetric difference the outer two. A merge
 *   refuses an argument that is not a set, as the library does, and the
 *   variadic forms fold it, intersection refusing zero sets for want of a
 *   universe.
 * Guarantees: all fifty-four claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

enum { MOST = 16 };

static const mt_atom *const *kids(const mt_atom *x) { return mt_children(x); }

static mt_atom *kept(const mt_atom *const *at, size_t n)
{
    mt_atom *out[MOST];
    for (size_t i = 0; i < n; i++) out[i] = mt_keep(at[i]);
    return mt_exprv(n, out);
}

/* Each term once, in the standard order. */
static mt_atom *set_of(const mt_atom *xs)
{
    const mt_atom *at[MOST];
    size_t n = mt_len(xs), unique = 0;
    require("room for the set", n <= MOST);
    memcpy(at, kids(xs), n * sizeof *at);
    qsort(at, n, sizeof *at, mt_order);
    for (size_t i = 0; i < n; i++)
        if (!unique || mt_compare(at[unique - 1], at[i]) != 0) at[unique++] = at[i];
    return kept(at, unique);
}

/* An expression whose children strictly increase. */
static bool is_set(const mt_atom *x)
{
    if (mt_kind_of(x) != MT_EXPR) return false;
    for (size_t i = 1; i < mt_len(x); i++)
        if (mt_compare(mt_at(x, i - 1), mt_at(x, i)) >= 0) return false;
    return true;
}

/* The regions of two sets a merge keeps. */
enum { LEFT = 1, BOTH = 2, RIGHT = 4, ALL = LEFT | BOTH | RIGHT };

/* One pass over two sorted arrays; NULL when either is not a set.
   Time: len(a) + len(b) comparisons at most. */
static mt_atom *merged(const mt_atom *a, const mt_atom *b, unsigned keep)
{
    if (!is_set(a) || !is_set(b)) return NULL;
    const mt_atom *out[2 * MOST];
    size_t i = 0, j = 0, n = 0, na = mt_len(a), nb = mt_len(b);
    while (i < na || j < nb) {
        int c = i == na ? 1 : j == nb ? -1 : mt_compare(mt_at(a, i), mt_at(b, j));
        if (c < 0) {
            if (keep & LEFT) out[n++] = mt_at(a, i);
            i++;
        } else if (c > 0) {
            if (keep & RIGHT) out[n++] = mt_at(b, j);
            j++;
        } else {
            if (keep & BOTH) out[n++] = mt_at(a, i);
            i++, j++;
        }
    }
    return kept(out, n);
}

/* The variadic forms: union folds from the empty set, intersection from its
   first set and refuses none. NULL on a refusal. */
static mt_atom *folded(const mt_atom *const *sets, size_t n, unsigned keep)
{
    mt_atom *acc = keep == ALL ? mt_unit() : n && is_set(sets[0]) ? mt_keep(sets[0]) : NULL;
    for (size_t i = keep == ALL ? 0 : 1; acc && i < n; i++) {
        mt_atom *next = merged(acc, sets[i], keep);
        mt_drop(acc);
        acc = next;
    }
    return acc;
}

/* Membership compares terms: bsearch over the child vector. */
static bool member(const mt_atom *set, const mt_atom *x)
{
    require("a set", is_set(set));
    return bsearch(&x, kids(set), mt_len(set), sizeof x, mt_order) != NULL;
}

/* Whether a value C computed is empty; TAKES it. */
static bool emptied(mt_atom *s)
{
    bool none = s && mt_len(s) == 0;
    mt_drop(s);
    return none;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_sets", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_sets")))));
    require("import lib_functional", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_functional")))));

    /* set-of sorts and drops repeats once. */
    mt_atom *none = mt_unit();
    mt_atom *loose = E(3, 1, 2, 1), *bab = E("b", "a", "b"), *mixed = E("b", E("x"), 2, "a", 1), *two_one = E(2, 1), *one_two_two = E(1, 2, 2);
    check_answers("set-of", mt_eval(m, E("set-of", mt_keep(loose))), set_of(loose));
    check_answers("the empty set", mt_eval(m, E("set-of", mt_unit())), set_of(none));
    mt_atom *ab = set_of(bab);
    check_answers("a set is an ordinary expression", mt_eval(m, E("size-atom", E("set-of", mt_keep(bab)))), (int64_t)mt_len(ab));
    mt_atom *x = set_of(two_one), *y = set_of(one_two_two);
    check_answers("== compares sets", mt_eval(m, E("==", E("set-of", mt_keep(two_one)), E("set-of", mt_keep(one_two_two)))), B(mt_eq(x, y)));
    check_answers("numbers, then symbols, then expressions", mt_eval(m, E("set-of", mt_keep(mixed))), set_of(mixed));

    /* set-is asks whether a value already is one. */
    mt_atom *ordered = E(1, 2, 3), *twice = E(1, 1), *three = mt_num(3);
    check_answers("ordered and unique", mt_eval(m, E("set-is", mt_keep(ordered))), B(is_set(ordered)));
    check_answers("out of order", mt_eval(m, E("set-is", mt_keep(two_one))), B(is_set(two_one)));
    check_answers("a repeat", mt_eval(m, E("set-is", mt_keep(twice))), B(is_set(twice)));
    check_answers("not an expression", mt_eval(m, E("set-is", mt_keep(three))), B(is_set(three)));

    /* Membership compares terms, so a variable is no member of numbers. */
    mt_atom *two = mt_num(2), *four = mt_num(4), *var = V("x");
    check_answers("a member", mt_eval(m, E("set-member", mt_keep(ordered), mt_keep(two))), B(member(ordered, two)));
    check_answers("not a member", mt_eval(m, E("set-member", mt_keep(ordered), mt_keep(four))), B(member(ordered, four)));
    check_answers("a variable is not a member of numbers", mt_eval(m, E("set-member", mt_keep(ordered), mt_keep(var))), B(member(ordered, var)));

    /* Insert and remove answer new sets. */
    mt_atom *one_three = E(1, 3), *nine = mt_num(9), *just_two = E(2), *just_three = E(3), *just_seven = E(7), *just_nine = E(9);
    check_answers("insert", mt_eval(m, E("set-insert", mt_keep(one_three), mt_keep(two))), merged(one_three, just_two, ALL));
    check_answers("inserting what is there", mt_eval(m, E("set-insert", mt_keep(one_three), 3)), merged(one_three, just_three, ALL));
    check_answers("remove", mt_eval(m, E("set-remove", mt_keep(ordered), mt_keep(two))), merged(ordered, just_two, LEFT));
    check_answers("removing what is not there", mt_eval(m, E("set-remove", mt_keep(ordered), mt_keep(nine))), merged(ordered, just_nine, LEFT));
    mt_atom *small_primes = E(2, 3, 5), *primes_c = set_of(small_primes);
    mt_atom *primes = mt_one(mt_eval(m, E("set-of", mt_keep(small_primes))));
    require("&primes", primes != NULL);
    check_answers("insert into a held set", mt_eval(m, E("set-insert", mt_keep(primes), 7)), merged(primes_c, just_seven, ALL));
    check_atom("which it leaves alone", mt_keep(primes), mt_keep(primes_c));

    /* The combinations. */
    mt_atom *two_three = E(2, 3), *two_four = E(2, 3, 4), *three_four = E(3, 4), *five = E(5), *three_five = E(3, 4, 5);
    check_answers("union", mt_eval(m, E("set-union", mt_keep(one_three), mt_keep(two_three))), merged(one_three, two_three, ALL));
    check_answers("intersection", mt_eval(m, E("set-intersection", mt_keep(ordered), mt_keep(two_four))), merged(ordered, two_four, BOTH));
    check_answers("difference", mt_eval(m, E("set-difference", mt_keep(ordered), mt_keep(two_four))), merged(ordered, two_four, LEFT));
    check_answers("difference the other way", mt_eval(m, E("set-difference", mt_keep(two_four), mt_keep(ordered))), merged(two_four, ordered, LEFT));
    check_answers("symmetric difference", mt_eval(m, E("set-symmetric-difference", mt_keep(ordered), mt_keep(two_four))),
                  merged(ordered, two_four, LEFT | RIGHT));
    check_answers("the union of nothing", mt_eval(m, E("set-union", mt_unit(), mt_unit())), merged(none, none, ALL));
    mt_atom *pair = E(1, 2);
    check_answers("disjoint sets share nothing", mt_eval(m, E("set-intersection", mt_keep(pair), mt_keep(three_four))),
                  merged(pair, three_four, BOTH));

    /* Any number of sets. */
    const mt_atom *three_sets[] = { pair, two_three, five }, *overlapping[] = { ordered, two_four, three_five };
    check_answers("a union of three", mt_eval(m, E("set-union", mt_keep(pair), mt_keep(two_three), mt_keep(five))), folded(three_sets, 3, ALL));
    check_answers("a union of none", mt_eval(m, E("set-union")), folded(NULL, 0, ALL));
    check_answers("an intersection of three", mt_eval(m, E("set-intersection", mt_keep(ordered), mt_keep(two_four), mt_keep(three_five))),
                  folded(overlapping, 3, BOTH));
    check_answers("an intersection needs a universe", mt_eval(m, guarded(E("set-intersection"))), verdict(computed(folded(NULL, 0, BOTH))));

    /* Subset and disjoint. */
    mt_atom *lone = E(1);
    check_answers("a subset", mt_eval(m, E("set-subset", mt_keep(pair), mt_keep(ordered))), B(emptied(merged(pair, ordered, LEFT))));
    check_answers("not a subset", mt_eval(m, E("set-subset", mt_keep(ordered), mt_keep(pair))), B(emptied(merged(ordered, pair, LEFT))));
    check_answers("the empty set is a subset", mt_eval(m, E("set-subset", mt_unit(), mt_keep(lone))), B(emptied(merged(none, lone, LEFT))));
    check_answers("of itself too", mt_eval(m, E("set-subset", mt_keep(pair), mt_keep(pair))), B(emptied(merged(pair, pair, LEFT))));
    check_answers("disjoint", mt_eval(m, E("set-disjoint", mt_keep(pair), mt_keep(three_four))), B(emptied(merged(pair, three_four, BOTH))));
    check_answers("not disjoint", mt_eval(m, E("set-disjoint", mt_keep(pair), mt_keep(two_three))), B(emptied(merged(pair, two_three, BOTH))));
    check_answers("the empty set is disjoint from itself", mt_eval(m, E("set-disjoint", mt_unit(), mt_unit())),
                  B(emptied(merged(none, none, BOTH))));

    /* The laws hold because the representation is canonical. */
    mt_atom *upto_four = E(1, 2, 3, 4);
    mt_atom *ab_ = merged(pair, just_three, ALL), *ba_ = merged(just_three, pair, ALL);
    check_answers("union commutes", mt_eval(m, E("==", E("set-union", mt_keep(pair), mt_keep(just_three)), E("set-union", mt_keep(just_three), mt_keep(pair)))),
                  B(mt_eq(ab_, ba_)));
    mt_atom *either = merged(lone, just_two, ALL), *left_side = merged(upto_four, either, LEFT);
    mt_atom *without_one = merged(upto_four, lone, LEFT), *without_two = merged(upto_four, just_two, LEFT);
    mt_atom *right_side = merged(without_one, without_two, BOTH);
    check_answers("De Morgan's law",
                  mt_eval(m, E("==", E("set-difference", mt_keep(upto_four), E("set-union", mt_keep(lone), mt_keep(just_two))),
                               E("set-intersection", E("set-difference", mt_keep(upto_four), mt_keep(lone)),
                                 E("set-difference", mt_keep(upto_four), mt_keep(just_two))))),
                  B(mt_eq(left_side, right_side)));

    /* A value that is no set is refused rather than merged. */
    check_answers("the first argument must be a set", mt_eval(m, guarded(E("set-union", mt_keep(two_one), mt_keep(lone)))),
                  verdict(computed(merged(two_one, lone, ALL))));
    check_answers("so must a member's set", mt_eval(m, guarded(E("set-member", mt_keep(twice), 1))), verdict(is_set(twice)));
    check_answers("and every argument", mt_eval(m, guarded(E("set-union", mt_keep(lone), mt_keep(two_one)))),
                  verdict(computed(merged(lone, two_one, ALL))));

    /* Runnable data, variables and distinct numeric kinds are terms too. */
    mt_atom *arith = E("+", 1, 2), *error = E("Error", "a", "b"), *repeated = E(mt_keep(arith), mt_keep(error), mt_keep(arith)),
            *ac = E("a", "c"), *error_a = E("Error", "a"), *vars = E(mt_keep(var)), *exact_and_float = E(1, 1.0);
    check_answers("runnable data stays data", mt_eval(m, E("set-of", E("quote", mt_keep(repeated)))), set_of(repeated));
    check_answers("an Error expression can be a set", mt_eval(m, E("set-is", E("quote", mt_keep(error)))), B(is_set(error)));
    check_answers("and merge", mt_eval(m, E("set-union", E("quote", mt_keep(error)), E("quote", mt_keep(ac)))), merged(error, ac, ALL));
    check_answers("and intersect", mt_eval(m, E("set-intersection", E("quote", mt_keep(error)), E("quote", mt_keep(error_a)))),
                  merged(error, error_a, BOTH));
    check_answers("a variable is a member of its own set", mt_eval(m, E("set-member", E("quote", mt_keep(vars)), mt_keep(var))), B(member(vars, var)));
    mt_atom *kinds = set_of(exact_and_float);
    check_answers("1 and 1.0 are two terms", mt_eval(m, E("size-atom", E("set-of", mt_keep(exact_and_float)))), (int64_t)mt_len(kinds));
    check_answers("a variable is no set", mt_eval(m, E("set-is", mt_keep(var))), B(is_set(var)));
    const mt_atom *unordered[] = { two_one };
    check_answers("one unordered argument is refused", mt_eval(m, guarded(E("set-intersection", mt_keep(two_one)))),
                  verdict(computed(folded(unordered, 1, BOTH))));

    /* One variadic arrow: one set, nine sets, a runtime collection. */
    const mt_atom *just_pair[] = { pair };
    check_answers("the union of one set", mt_eval(m, E("set-union", mt_keep(pair))), folded(just_pair, 1, ALL));
    check_answers("the intersection of one set", mt_eval(m, E("set-intersection", mt_keep(pair))), folded(just_pair, 1, BOTH));
    mt_atom *singles[9];
    const mt_atom *nine_sets[9];
    for (int64_t i = 0; i < 9; i++) nine_sets[i] = singles[i] = E(i + 1);
    check_answers("the union of nine", mt_eval(m, E("set-union", mt_keep(singles[0]), mt_keep(singles[1]), mt_keep(singles[2]), mt_keep(singles[3]),
                                                    mt_keep(singles[4]), mt_keep(singles[5]), mt_keep(singles[6]), mt_keep(singles[7]),
                                                    mt_keep(singles[8]))),
                  folded(nine_sets, 9, ALL));
    mt_atom *runtime_union = E(mt_keep(pair), mt_keep(two_three), E(4)), *runtime_meet = E(mt_keep(ordered), mt_keep(two_four), E(3));
    check_answers("apply-to a union", mt_eval(m, E("apply-to", "set-union", E("quote", mt_keep(runtime_union)))),
                  folded(kids(runtime_union), mt_len(runtime_union), ALL));
    check_answers("apply-to an intersection", mt_eval(m, E("apply-to", "set-intersection", E("quote", mt_keep(runtime_meet)))),
                  folded(kids(runtime_meet), mt_len(runtime_meet), BOTH));

    mt_atom *held[] = { none, loose, bab, mixed, two_one, one_two_two, ab, x, y, ordered, twice, three, two, four, var, one_three, nine,
                        small_primes, primes_c, primes, two_three, two_four, three_four, five, three_five, pair, lone, just_three,
                        upto_four, just_two, just_seven, just_nine, ab_, ba_, either, left_side, without_one, without_two, right_side, arith, error,
                        repeated, ac, error_a, vars, exact_and_float, kinds, runtime_union, runtime_meet };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    for (size_t i = 0; i < 9; i++) mt_drop(singles[i]);
    return done(m);
}
