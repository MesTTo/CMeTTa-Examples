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
 * Guarantees: all fifty-four claims of the original hold
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

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
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_sets", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_sets")))));
    require("import lib_functional", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_functional")))));

    /* set-of sorts and drops repeats once. */
    mt_atom *none = mt_unit();
    mt_atom *loose = E(3, 1, 2, 1), *bab = E("b", "a", "b"), *mixed = E("b", E("x"), 2, "a", 1), *two_one = E(2, 1), *one_two_two = E(1, 2, 2);
    assert(answers_are(mt_eval(m, E("set-of", mt_keep(loose))), E(set_of(loose))) && "set-of");
    assert(answers_are(mt_eval(m, E("set-of", mt_unit())), E(set_of(none))) && "the empty set");
    mt_atom *ab = set_of(bab);
    assert(answers_are(mt_eval(m, E("size-atom", E("set-of", mt_keep(bab)))), E((int64_t)mt_len(ab))) && "a set is an ordinary expression");
    mt_atom *x = set_of(two_one), *y = set_of(one_two_two);
    assert(answers_are(mt_eval(m, E("==", E("set-of", mt_keep(two_one)), E("set-of", mt_keep(one_two_two)))), E(B(mt_eq(x, y)))) && "== compares sets");
    assert(answers_are(mt_eval(m, E("set-of", mt_keep(mixed))), E(set_of(mixed))) && "numbers, then symbols, then expressions");

    /* set-is asks whether a value already is one. */
    mt_atom *ordered = E(1, 2, 3), *twice = E(1, 1), *three = mt_num(3);
    assert(answers_are(mt_eval(m, E("set-is", mt_keep(ordered))), E(B(is_set(ordered)))) && "ordered and unique");
    assert(answers_are(mt_eval(m, E("set-is", mt_keep(two_one))), E(B(is_set(two_one)))) && "out of order");
    assert(answers_are(mt_eval(m, E("set-is", mt_keep(twice))), E(B(is_set(twice)))) && "a repeat");
    assert(answers_are(mt_eval(m, E("set-is", mt_keep(three))), E(B(is_set(three)))) && "not an expression");

    /* Membership compares terms, so a variable is no member of numbers. */
    mt_atom *two = mt_num(2), *four = mt_num(4), *var = V("x");
    assert(answers_are(mt_eval(m, E("set-member", mt_keep(ordered), mt_keep(two))), E(B(member(ordered, two)))) && "a member");
    assert(answers_are(mt_eval(m, E("set-member", mt_keep(ordered), mt_keep(four))), E(B(member(ordered, four)))) && "not a member");
    assert(answers_are(mt_eval(m, E("set-member", mt_keep(ordered), mt_keep(var))), E(B(member(ordered, var)))) && "a variable is not a member of numbers");

    /* Insert and remove answer new sets. */
    mt_atom *one_three = E(1, 3), *nine = mt_num(9), *just_two = E(2), *just_three = E(3), *just_seven = E(7), *just_nine = E(9);
    assert(answers_are(mt_eval(m, E("set-insert", mt_keep(one_three), mt_keep(two))), E(merged(one_three, just_two, ALL))) && "insert");
    assert(answers_are(mt_eval(m, E("set-insert", mt_keep(one_three), 3)), E(merged(one_three, just_three, ALL))) && "inserting what is there");
    assert(answers_are(mt_eval(m, E("set-remove", mt_keep(ordered), mt_keep(two))), E(merged(ordered, just_two, LEFT))) && "remove");
    assert(answers_are(mt_eval(m, E("set-remove", mt_keep(ordered), mt_keep(nine))), E(merged(ordered, just_nine, LEFT))) && "removing what is not there");
    mt_atom *small_primes = E(2, 3, 5), *primes_c = set_of(small_primes);
    mt_atom *primes = mt_one(mt_eval(m, E("set-of", mt_keep(small_primes))));
    require("&primes", primes != NULL);
    assert(answers_are(mt_eval(m, E("set-insert", mt_keep(primes), 7)), E(merged(primes_c, just_seven, ALL))) && "insert into a held set");
    assert(atom_is(mt_keep(primes), mt_keep(primes_c)) && "which it leaves alone");

    /* The combinations. */
    mt_atom *two_three = E(2, 3), *two_four = E(2, 3, 4), *three_four = E(3, 4), *five = E(5), *three_five = E(3, 4, 5);
    assert(answers_are(mt_eval(m, E("set-union", mt_keep(one_three), mt_keep(two_three))), E(merged(one_three, two_three, ALL))) && "union");
    assert(answers_are(mt_eval(m, E("set-intersection", mt_keep(ordered), mt_keep(two_four))), E(merged(ordered, two_four, BOTH))) && "intersection");
    assert(answers_are(mt_eval(m, E("set-difference", mt_keep(ordered), mt_keep(two_four))), E(merged(ordered, two_four, LEFT))) && "difference");
    assert(answers_are(mt_eval(m, E("set-difference", mt_keep(two_four), mt_keep(ordered))), E(merged(two_four, ordered, LEFT))) && "difference the other way");
    assert(answers_are(mt_eval(m, E("set-symmetric-difference", mt_keep(ordered), mt_keep(two_four))), E(merged(ordered, two_four, LEFT | RIGHT)))
           && "symmetric difference");
    assert(answers_are(mt_eval(m, E("set-union", mt_unit(), mt_unit())), E(merged(none, none, ALL))) && "the union of nothing");
    mt_atom *pair = E(1, 2);
    assert(answers_are(mt_eval(m, E("set-intersection", mt_keep(pair), mt_keep(three_four))), E(merged(pair, three_four, BOTH)))
           && "disjoint sets share nothing");

    /* Any number of sets. */
    const mt_atom *three_sets[] = { pair, two_three, five }, *overlapping[] = { ordered, two_four, three_five };
    assert(answers_are(mt_eval(m, E("set-union", mt_keep(pair), mt_keep(two_three), mt_keep(five))), E(folded(three_sets, 3, ALL))) && "a union of three");
    assert(answers_are(mt_eval(m, E("set-union")), E(folded(NULL, 0, ALL))) && "a union of none");
    assert(answers_are(mt_eval(m, E("set-intersection", mt_keep(ordered), mt_keep(two_four), mt_keep(three_five))), E(folded(overlapping, 3, BOTH)))
           && "an intersection of three");
    assert(answers_are(mt_eval(m, guarded(E("set-intersection"))), E(verdict(computed(folded(NULL, 0, BOTH))))) && "an intersection needs a universe");

    /* Subset and disjoint. */
    mt_atom *lone = E(1);
    assert(answers_are(mt_eval(m, E("set-subset", mt_keep(pair), mt_keep(ordered))), E(B(emptied(merged(pair, ordered, LEFT))))) && "a subset");
    assert(answers_are(mt_eval(m, E("set-subset", mt_keep(ordered), mt_keep(pair))), E(B(emptied(merged(ordered, pair, LEFT))))) && "not a subset");
    assert(answers_are(mt_eval(m, E("set-subset", mt_unit(), mt_keep(lone))), E(B(emptied(merged(none, lone, LEFT))))) && "the empty set is a subset");
    assert(answers_are(mt_eval(m, E("set-subset", mt_keep(pair), mt_keep(pair))), E(B(emptied(merged(pair, pair, LEFT))))) && "of itself too");
    assert(answers_are(mt_eval(m, E("set-disjoint", mt_keep(pair), mt_keep(three_four))), E(B(emptied(merged(pair, three_four, BOTH))))) && "disjoint");
    assert(answers_are(mt_eval(m, E("set-disjoint", mt_keep(pair), mt_keep(two_three))), E(B(emptied(merged(pair, two_three, BOTH))))) && "not disjoint");
    assert(answers_are(mt_eval(m, E("set-disjoint", mt_unit(), mt_unit())), E(B(emptied(merged(none, none, BOTH)))))
           && "the empty set is disjoint from itself");

    /* The laws hold because the representation is canonical. */
    mt_atom *upto_four = E(1, 2, 3, 4);
    mt_atom *ab_ = merged(pair, just_three, ALL), *ba_ = merged(just_three, pair, ALL);
    assert(answers_are(mt_eval(m, E("==", E("set-union", mt_keep(pair), mt_keep(just_three)), E("set-union", mt_keep(just_three), mt_keep(pair)))), E(B(mt_eq(ab_, ba_))))
           && "union commutes");
    mt_atom *either = merged(lone, just_two, ALL), *left_side = merged(upto_four, either, LEFT);
    mt_atom *without_one = merged(upto_four, lone, LEFT), *without_two = merged(upto_four, just_two, LEFT);
    mt_atom *right_side = merged(without_one, without_two, BOTH);
    assert(answers_are(mt_eval(m, E("==", E("set-difference", mt_keep(upto_four), E("set-union", mt_keep(lone), mt_keep(just_two))),
                                    E("set-intersection", E("set-difference", mt_keep(upto_four), mt_keep(lone)),
                                      E("set-difference", mt_keep(upto_four), mt_keep(just_two))))), E(B(mt_eq(left_side, right_side))))
           && "De Morgan's law");

    /* A value that is no set is refused rather than merged. */
    assert(answers_are(mt_eval(m, guarded(E("set-union", mt_keep(two_one), mt_keep(lone)))), E(verdict(computed(merged(two_one, lone, ALL)))))
           && "the first argument must be a set");
    assert(answers_are(mt_eval(m, guarded(E("set-member", mt_keep(twice), 1))), E(verdict(is_set(twice)))) && "so must a member's set");
    assert(answers_are(mt_eval(m, guarded(E("set-union", mt_keep(lone), mt_keep(two_one)))), E(verdict(computed(merged(lone, two_one, ALL)))))
           && "and every argument");

    /* Runnable data, variables and distinct numeric kinds are terms too. */
    mt_atom *arith = E("+", 1, 2), *error = E("Error", "a", "b"), *repeated = E(mt_keep(arith), mt_keep(error), mt_keep(arith)),
            *ac = E("a", "c"), *error_a = E("Error", "a"), *vars = E(mt_keep(var)), *exact_and_float = E(1, 1.0);
    assert(answers_are(mt_eval(m, E("set-of", E("quote", mt_keep(repeated)))), E(set_of(repeated))) && "runnable data stays data");
    assert(answers_are(mt_eval(m, E("set-is", E("quote", mt_keep(error)))), E(B(is_set(error)))) && "an Error expression can be a set");
    assert(answers_are(mt_eval(m, E("set-union", E("quote", mt_keep(error)), E("quote", mt_keep(ac)))), E(merged(error, ac, ALL))) && "and merge");
    assert(answers_are(mt_eval(m, E("set-intersection", E("quote", mt_keep(error)), E("quote", mt_keep(error_a)))), E(merged(error, error_a, BOTH)))
           && "and intersect");
    assert(answers_are(mt_eval(m, E("set-member", E("quote", mt_keep(vars)), mt_keep(var))), E(B(member(vars, var)))) && "a variable is a member of its own set");
    mt_atom *kinds = set_of(exact_and_float);
    assert(answers_are(mt_eval(m, E("size-atom", E("set-of", mt_keep(exact_and_float)))), E((int64_t)mt_len(kinds))) && "1 and 1.0 are two terms");
    assert(answers_are(mt_eval(m, E("set-is", mt_keep(var))), E(B(is_set(var)))) && "a variable is no set");
    const mt_atom *unordered[] = { two_one };
    assert(answers_are(mt_eval(m, guarded(E("set-intersection", mt_keep(two_one)))), E(verdict(computed(folded(unordered, 1, BOTH)))))
           && "one unordered argument is refused");

    /* One variadic arrow: one set, nine sets, a runtime collection. */
    const mt_atom *just_pair[] = { pair };
    assert(answers_are(mt_eval(m, E("set-union", mt_keep(pair))), E(folded(just_pair, 1, ALL))) && "the union of one set");
    assert(answers_are(mt_eval(m, E("set-intersection", mt_keep(pair))), E(folded(just_pair, 1, BOTH))) && "the intersection of one set");
    mt_atom *singles[9];
    const mt_atom *nine_sets[9];
    for (int64_t i = 0; i < 9; i++) nine_sets[i] = singles[i] = E(i + 1);
    assert(answers_are(mt_eval(m, E("set-union", mt_keep(singles[0]), mt_keep(singles[1]), mt_keep(singles[2]), mt_keep(singles[3]),
                                    mt_keep(singles[4]), mt_keep(singles[5]), mt_keep(singles[6]), mt_keep(singles[7]),
                                    mt_keep(singles[8]))), E(folded(nine_sets, 9, ALL)))
           && "the union of nine");
    mt_atom *runtime_union = E(mt_keep(pair), mt_keep(two_three), E(4)), *runtime_meet = E(mt_keep(ordered), mt_keep(two_four), E(3));
    assert(answers_are(mt_eval(m, E("apply-to", "set-union", E("quote", mt_keep(runtime_union)))), E(folded(kids(runtime_union), mt_len(runtime_union), ALL)))
           && "apply-to a union");
    assert(answers_are(mt_eval(m, E("apply-to", "set-intersection", E("quote", mt_keep(runtime_meet)))), E(folded(kids(runtime_meet), mt_len(runtime_meet), BOTH)))
           && "apply-to an intersection");

    mt_atom *held[] = { none, loose, bab, mixed, two_one, one_two_two, ab, x, y, ordered, twice, three, two, four, var, one_three, nine,
                        small_primes, primes_c, primes, two_three, two_four, three_four, five, three_five, pair, lone, just_three,
                        upto_four, just_two, just_seven, just_nine, ab_, ba_, either, left_side, without_one, without_two, right_side, arith, error,
                        repeated, ac, error_a, vars, exact_and_float, kinds, runtime_union, runtime_meet };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    for (size_t i = 0; i < 9; i++) mt_drop(singles[i]);
    mt_close(m);
    return 0;
}
