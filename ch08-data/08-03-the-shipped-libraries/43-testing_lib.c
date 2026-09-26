/* Purpose: lib_testing over lib_combinatorics's domains, held against C's
 *   own loops. A domain is a generator C walks: range is a half-open interval
 *   of integers, counted in GMP so a bound past int64 is the same loop, and
 *   cartesian-power is an odometer whose last place turns fastest. forall is
 *   an all-of loop whose check holds for a value when True is among its
 *   answers, foldall a fold, here a count, and once the loop's first witness.
 *   A bag assertion is two counted differences that must both be empty, the
 *   same two a failed one reports as its missing and excess bags; freshness
 *   is mt_alpha_eq over the copies the engine made. Each of the original's
 *   forall forms is a C loop over the engine's generator that proves its
 *   check on every value.
 * Build: cc 43-testing_lib.c $(pkg-config --cflags --libs cmetta gmp)
 * Guarantees: all thirty-seven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<gmp.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include "_fixtures/exact_oracle.h"

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

/* from less take, as a bag: an atom of from keeps its place unless take
   still holds an identical one to cancel it, the counted subtraction
   subtraction-atom makes. Borrows both; the answer is owned.
   Time: n*m comparisons, n and m the two lengths. */
static inline mt_atom *bag_minus(const mt_atom *from, const mt_atom *take)
{
    size_t n = mt_len(from), m = mt_len(take), kept = 0;
    bool *spent = calloc(m + 1, sizeof *spent);
    mt_atom **left = malloc((n + 1) * sizeof *left);
    require("room for a bag", spent && left);
    for (size_t i = 0; i < n; i++) {
        size_t j = 0;
        while (j < m && (spent[j] || mt_compare(mt_at(from, i), mt_at(take, j)) != 0)) j++;
        if (j < m) spent[j] = true;
        else left[kept++] = mt_keep(mt_at(from, i));
    }
    mt_atom *out = mt_exprv(kept, left);
    free(spent), free(left);
    return out;
}

/* The integers of [lo, hi), each an Int or a BigInt as its width decides:
   range's answers, collapsed. */
static mt_atom *interval(const char *lo, const char *hi)
{
    mpz_t at, end, count;
    mpz_inits(at, end, count, NULL);
    require("decimal bounds", mpz_set_str(at, lo, 10) == 0 && mpz_set_str(end, hi, 10) == 0);
    mpz_sub(count, end, at);
    require("an interval C can hold", mpz_sgn(count) <= 0 || mpz_fits_ulong_p(count));
    size_t n = mpz_sgn(count) > 0 ? mpz_get_ui(count) : 0;
    mt_atom **items = malloc((n + 1) * sizeof *items);
    require("room for the interval", items != NULL);
    for (size_t i = 0; i < n; i++, mpz_add_ui(at, at, 1)) items[i] = integer_of(at);
    mpz_clears(at, end, count, NULL);
    mt_atom *out = mt_exprv(n, items);
    free(items);
    return out;
}

/* p^k, refused rather than wrapped past size_t. */
static size_t tuples_of(size_t p, size_t k)
{
    size_t count = 1;
    for (size_t i = 0; i < k; i++) {
        require("a count C can hold", p == 0 || count <= SIZE_MAX / p);
        count *= p;
    }
    return count;
}

/* Every tuple of `length` items drawn from `population`, appended to out:
   the odometer, mixed-radix counting with every radix the population's size,
   the last place turning fastest as cartesian-power answers [source: Donald
   E. Knuth, The Art of Computer Programming, vol. 4A, section 7.2.1.1,
   Algorithm M]. Time: p^k tuples of k items, p = population size, k =
   length. */
static void power_into(mt_atom **out, size_t *n, const mt_atom *population, size_t length)
{
    size_t p = mt_len(population);
    if (p == 0 && length > 0) return;
    size_t *place = calloc(length + 1, sizeof *place);
    mt_atom **items = malloc((length + 1) * sizeof *items);
    require("room for the odometer", place && items);
    for (bool turned = true; turned;) {
        for (size_t i = 0; i < length; i++) items[i] = mt_keep(mt_at(population, place[i]));
        out[(*n)++] = mt_exprv(length, items);
        turned = false;
        for (size_t i = length; i > 0 && !turned; i--)
            if (++place[i - 1] < p) turned = true;
            else place[i - 1] = 0;
    }
    free(place), free(items);
}

/* cartesian-power's answers for each length in turn, collapsed. */
static mt_atom *powers(const mt_atom *population, const size_t *lengths, size_t count)
{
    size_t total = 0, n = 0;
    for (size_t i = 0; i < count; i++) {
        size_t more = tuples_of(mt_len(population), lengths[i]);
        require("a count C can hold", total <= SIZE_MAX - more - 1);
        total += more;
    }
    mt_atom **out = malloc((total + 1) * sizeof *out);
    require("room for the tuples", out != NULL);
    for (size_t i = 0; i < count; i++) power_into(out, &n, population, lengths[i]);
    mt_atom *all = mt_exprv(n, out);
    free(out);
    return all;
}

/* cartesian-power's precondition: a length is a nonnegative integer, which
   lib_combinatorics asks as (% x 1) being 0 and x being at least 0. */
static bool a_length(const mt_atom *x)
{
    mt_kind k = mt_kind_of(x);
    mt_atom *zero = mt_num(0);
    bool fine = (k == MT_INT || k == MT_BIGINT) && mt_compare(x, zero) >= 0;
    mt_drop(zero);
    return fine;
}

/* assertEqualToResult's verdict: nothing missing and nothing in excess. */
static bool same_bag(const mt_atom *actual, const mt_atom *expected)
{
    mt_atom *missing = bag_minus(expected, actual), *excess = bag_minus(actual, expected);
    bool same = mt_len(missing) == 0 && mt_len(excess) == 0;
    mt_drop(missing), mt_drop(excess);
    return same;
}

/* True among a check's answers, which is when forall's check holds. */
static bool some_true(const mt_atom *answers)
{
    for (size_t i = 0; i < mt_len(answers); i++)
        if (mt_kind_of(mt_at(answers, i)) == MT_BOOL && mt_truth(mt_at(answers, i))) return true;
    return false;
}

/* The first item of a list satisfying a C predicate, once's witness; NULL
   when none does. */
static const mt_atom *first(const mt_atom *items, bool (*holds)(const mt_atom *x, const void *with), const void *with)
{
    for (size_t i = 0; i < mt_len(items); i++)
        if (holds(mt_at(items, i), with)) return mt_at(items, i);
    return NULL;
}

static bool at_least(const mt_atom *x, const void *bound) { return mt_int(x) >= *(const int64_t *)bound; }
static bool given(const mt_atom *x, const void *truth) { return (void)x, *(const bool *)truth; }
static bool empty_expression(const mt_atom *x, const void *with) { return (void)with, mt_len(x) == 0; }
static bool equal_to(const mt_atom *x, const void *other) { return mt_eq(x, other); }

/* A witness as a collapse answers it: (w), or () when there is none. */
static mt_atom *witnessed(const mt_atom *w) { return w ? E(mt_keep(w)) : mt_unit(); }

/* A one-parameter lambda, (|-> ($x) body). */
static mt_atom *lambda(mt_atom *body) { return E("|->", E(V("x")), body); }

static mt_atom *counter(void) { return E("|->", E(V("value"), V("count")), E("+", V("count"), 1)); }
static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_combinatorics", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_combinatorics")))));
    require("import lib_testing", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_testing")))));

    /* A domain is an ordinary generator; range excludes its upper bound. */
    const struct { const char *claim, *lo, *hi; } ranges[] = {
        { "a range excludes its upper bound", "-2", "3" },
        { "one value", "3", "4" },
        { "an empty range", "3", "3" },
        { "bounds past int64", "100000000000000000000", "100000000000000000003" },
    };
    for (size_t i = 0; i < sizeof ranges / sizeof *ranges; i++)
        assert(answers_are(mt_eval(m, E("collapse", E("range", mt_bigint(ranges[i].lo), mt_bigint(ranges[i].hi)))), E(interval(ranges[i].lo, ranges[i].hi)))
               && ranges[i].claim);
    mt_atom *listed[] = { E("a", "a", T("π")), mt_unit() };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, E("collapse", E("superpose", mt_keep(listed[i])))), E(mt_keep(listed[i])))
               && (i ? "superposing nothing" : "superpose keeps order and repeats"));

    /* superpose evaluates alternatives; indexing selects a value as data. */
    mt_atom *literal = E("+", 1, 2), *quoted = E("quote", E(mt_keep(literal)));
    assert(answers_are(mt_eval(m, E("size-atom", E("index-atom", mt_keep(quoted), 0))), E(N((int64_t)mt_len(literal)))) && "an indexed sum stays data");
    mt_atom *units = E(mt_unit());
    assert(answers_are(mt_eval(m, E("collapse", E("index-atom", mt_keep(units), 0))), E(E(mt_keep(mt_at(units, 0))))) && "a unit indexed");
    mt_atom *one_to_three = interval("1", "4");
    assert(answers_are(mt_eval(m, E("let", V("items"), E("collapse", E("range", 1, 4)),
                                     E("collapse", E("index-atom", V("items"), E("range", 0, E("size-atom", V("items"))))))), E(mt_keep(one_to_three)))
           && "every index of a list");

    /* List lengths and element choices vary independently. */
    mt_atom *bits = interval("0", "2");
    const struct { const char *claim; mt_atom *population, *length; size_t lengths[3], n; } power_rows[] = {
        { "length zero is one empty tuple", E(0, 1), N(0), { 0 }, 1 },
        { "every pair, the last place fastest", E(0, 1), N(2), { 2 }, 1 },
        { "repeats are positions", E("a", "a"), N(2), { 2 }, 1 },
        { "nothing chosen from nothing", mt_unit(), N(0), { 0 }, 1 },
        { "nothing to choose from", mt_unit(), N(2), { 2 }, 1 },
        { "no length at all", E(0, 1), E("range", 2, 2), { 0 }, 0 },
        { "units are items", E(mt_unit(), E("a")), N(1), { 1 }, 1 },
    };
    for (size_t i = 0; i < sizeof power_rows / sizeof *power_rows; i++)
        assert(answers_are(mt_eval(m, E("collapse", E("cartesian-power", mt_keep(power_rows[i].population), power_rows[i].length))), E(powers(power_rows[i].population, power_rows[i].lengths, power_rows[i].n)))
               && power_rows[i].claim);
    const size_t short_lengths[] = { 0, 1, 2 };
    assert(answers_are(mt_eval(m, E("collapse", E("let*", E(E(V("population"), E("collapse", E("range", 0, 2))), E(V("length"), E("range", 0, 3))),
                                                   E("cartesian-power", V("population"), V("length"))))), E(powers(bits, short_lengths, 3)))
           && "lengths from a generator");

    /* Freshness is explicit; sharing inside each copied term survives. */
    mt_atom *shared = E("row", V("x"), V("x")), *copies = E(E("row", V("a"), V("a")), E("row", V("b"), V("b")));
    mt_atom *pair = mt_one(mt_eval(m, E("let", V("choice"), E("index-atom", E(mt_keep(shared)), 0),
                                        E("map-atom", E(1, 2), V("ignored"), E("copy_term", E("quote", V("choice")))))));
    require("the copies", pair != NULL);
    assert(answers_are(mt_eval(m, E("=alpha", mt_keep(pair), mt_keep(copies))), E(B(mt_alpha_eq(pair, copies)))) && "each copy its own variables, shared inside");

    /* forall traverses and test judges: C walks the engine's generator and
       proves the check on every value. */
    mt_list xs = mt_all(mt_eval(m, E("range", -3, 4)));
    for (size_t i = 0; i < xs.len; i++) {
        int64_t x = mt_int(xs.items[i]);
        assert(answers_are(mt_eval(m, E(">=", E("*", x, x), 0)), E(B(x * x >= 0))) && "a square is never negative");
    }
    mt_list_free(xs);
    const size_t up_to_three[] = { 0, 1, 2, 3 };
    assert(answers_are(mt_eval(m, E("collapse", E("cartesian-power", mt_keep(bits), E("range", 0, 4)))), E(powers(bits, up_to_three, 4)))
           && "every tuple up to length three, in order");
    mt_list tuples = mt_all(mt_eval(m, E("cartesian-power", mt_keep(bits), E("range", 0, 4))));
    for (size_t i = 0; i < tuples.len; i++)
        assert(answers_are(mt_eval(m, E("reverse", E("reverse", mt_keep(tuples.items[i])))), E(mt_keep(tuples.items[i]))) && "reversed twice is itself");
    mt_list_free(tuples);
    mt_atom *zero_to_five = interval("0", "5");
    bool nonnegative = true;
    for (size_t i = 0; i < mt_len(zero_to_five); i++) nonnegative &= 0 <= mt_int(mt_at(zero_to_five, i));
    assert(answers_are(mt_eval(m, E("forall", E("range", 0, 5), E("<=", 0))), E(B(nonnegative))) && "a curried comparison as the check");
    mt_atom *abb = E("a", "a", "b");
    assert(answers_are(mt_eval(m, E("foldall", counter(), E("superpose", mt_keep(abb)), 0)), E(N((int64_t)mt_len(abb)))) && "a fold counts every answer");

    /* A check holds for a value when True is among its answers, since forall
       is \+ (Generator, \+ Check); an empty domain holds vacuously. */
    const struct { const char *claim; mt_atom *generator, *answers; size_t values; } checks[] = {
        { "an empty domain holds", E("superpose", mt_unit()), E(B(false)), 0 },
        { "one True answer is enough", E("range", 0, 2), E(B(false), B(true)), 2 },
        { "no answer is no verdict", E("range", 0, 2), mt_unit(), 2 },
        { "a value is not a verdict", E("range", 0, 2), E(7), 2 },
    };
    for (size_t i = 0; i < sizeof checks / sizeof *checks; i++) {
        mt_atom *answers = checks[i].answers, *check_body = mt_len(answers) == 0 ? E("empty")
                                                            : mt_len(answers) == 1 ? mt_keep(mt_at(answers, 0))
                                                                                   : E("superpose", mt_keep(answers));
        assert(answers_are(mt_eval(m, E("forall", checks[i].generator, lambda(check_body))), E(B(checks[i].values == 0 || some_true(answers))))
               && checks[i].claim);
        mt_drop(answers);
    }
    assert(answers_are(mt_eval(m, E("foldall", counter(), E("superpose", mt_unit()), 0)), E(N(0))) && "folding nothing leaves the start");

    /* A bag assertion checks all answers and their multiplicities. */
    mt_atom *two_ones = E(1, 2, 1), *nothing = mt_unit();
    const struct { const char *claim; mt_atom *goal; const mt_atom *bag; } bags[] = {
        { "the answers are that bag", E("superpose", E(2, 1, 1)), two_ones },
        { "no answers are the empty bag", E("empty"), nothing },
    };
    for (size_t b = 0; b < sizeof bags / sizeof *bags; b++) {
        xs = mt_all(mt_eval(m, E("range", 1, 3)));
        for (size_t i = 0; i < xs.len; i++) {
            mt_atom *got = mt_one(mt_eval(m, E("collapse", mt_keep(bags[b].goal))));
            assert(got && same_bag(got, bags[b].bag) && bags[b].claim);
            mt_drop(got);
        }
        mt_list_free(xs);
        mt_drop(bags[b].goal);
    }
    xs = mt_all(mt_eval(m, E("index-atom", mt_keep(quoted), 0)));
    for (size_t i = 0; i < xs.len; i++) {
        mt_atom *got = E(mt_keep(xs.items[i])), *want = E(mt_keep(literal));
        assert(same_bag(got, want) && "a quoted value is one answer");
        mt_drop(got), mt_drop(want);
    }
    mt_list_free(xs);
    mt_atom *row_y = E("row", V("y"), V("y"));
    xs = mt_all(mt_eval(m, E("index-atom", E(mt_keep(shared)), 0)));
    for (size_t i = 0; i < xs.len; i++)
        assert(answers_are(mt_eval(m, E("=alpha", mt_keep(xs.items[i]), mt_keep(row_y))), E(B(mt_alpha_eq(xs.items[i], row_y))))
               && "a traversed value keeps its sharing");
    mt_list_free(xs);

    /* once commits to the first answer satisfying a condition. */
    const int64_t three = 3;
    mt_atom *zero_to_six = interval("0", "6"), *zero_to_three = interval("0", "3");
    assert(answers_are(mt_eval(m, E("once", E("let", V("x"), E("range", 0, 6), E("if", E(">=", V("x"), 3), V("x"), E("empty"))))), E(mt_keep(first(zero_to_six, at_least, &three))))
           && "the first witness");
    assert(answers_are(mt_eval(m, E("collapse", E("once", E("let", V("x"), E("range", 0, 6), E("if", E(">=", V("x"), 3), V("x"), E("empty")))))), E(witnessed(first(zero_to_six, at_least, &three))))
           && "collapsed, one answer");
    assert(answers_are(mt_eval(m, E("collapse", E("once", E("let", V("x"), E("range", 0, 3), E("if", E(">=", V("x"), 3), V("x"), E("empty")))))), E(witnessed(first(zero_to_three, at_least, &three))))
           && "no witness, no answer");
    assert(answers_are(mt_eval(m, E("collapse", E("once", E("superpose", mt_unit())))), E(witnessed(first(nothing, given, &(bool){ true })))) && "once over nothing");
    mt_list ones = mt_all(mt_eval(m, E("superpose", E(2, 1, 1))));
    qsort(ones.items, ones.len, sizeof *ones.items, mt_order);
    mt_atom *sorted_ones = mt_exprv(ones.len, ones.items), *want_sorted = E(1, 1, 2);
    mt_free(ones.items);
    bool sorted_matches = mt_eq(sorted_ones, want_sorted);
    assert(answers_are(mt_eval(m, E("once", E("let", V("x"), E("range", 0, 6),
                                               E("if", E("==", E("sort-atom", E("collapse", E("superpose", E(2, 1, 1)))), E(1, 1, 2)), V("x"), E("empty"))))), E(mt_keep(first(zero_to_six, given, &sorted_matches))))
           && "a condition on a sorted bag");
    mt_drop(sorted_ones), mt_drop(want_sorted);
    assert(answers_are(mt_eval(m, E("size-atom", E("once", E("index-atom", mt_keep(quoted), 0)))), E(N((int64_t)mt_len(literal)))) && "once keeps a value as data");
    mt_atom *unit_or_a = E(mt_unit(), E("a"));
    assert(answers_are(mt_eval(m, E("collapse", E("once", E("let", V("x"), E("index-atom", mt_keep(unit_or_a), E("range", 0, 2)),
                                                         E("if", E("==", E("size-atom", V("x")), 0), V("x"), E("empty")))))), E(witnessed(first(unit_or_a, empty_expression, NULL))))
           && "the first empty item");
    xs = mt_all(mt_eval(m, E("range", 0, 3)));
    for (size_t i = 0; i < xs.len; i++)
        assert(answers_are(mt_eval(m, E("once", E("let", V("y"), E("range", 0, 3), E("if", E("==", mt_keep(xs.items[i]), V("y")), V("y"), E("empty"))))), E(mt_keep(first(zero_to_three, equal_to, xs.items[i]))))
               && "each value finds itself");
    mt_list_free(xs);

    /* Counting the checked cases is a fold over the same generator. */
    int64_t counted = 0;
    for (size_t i = 0; i < mt_len(zero_to_three); i++) counted += mt_int(mt_at(zero_to_three, i)) >= 0;
    assert(answers_are(mt_eval(m, E("foldall", E("|->", E(V("x"), V("count")), E("let", V("checked"), E("test", E(">=", V("x"), 0), B(true)), E("+", V("count"), 1))),
                                        E("range", 0, 3), 0)), E(N(counted)))
           && "a count of checked cases");

    /* A caught failure keeps the two bags its verdict was built from. */
    mt_atom *bag_goal = E("assertEqualToResult", E("superpose", E(1, 1)), E(1, 2)), *expected_bag = E(1, 2), *actual_bag = E(1, 1);
    mt_atom *error = mt_one(mt_eval(m, E("catch", mt_keep(bag_goal))));
    require("an error", error && mt_kind_of(error) == MT_EXPR && mt_len(error) == 3);
    assert(atom_is(mt_keep(mt_at(error, 1)), E("metta_assertion_failed", mt_keep(bag_goal), bag_minus(expected_bag, actual_bag), bag_minus(actual_bag, expected_bag)))
           && "the missing and the excess bags");
    mt_drop(error);
    mt_atom *lengths[] = { N(-1), mt_real(1.5) };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(guarded(m, E("cartesian-power", E("a"), mt_keep(lengths[i]))), E(verdict(a_length(lengths[i]))))
               && (i ? "a fractional length is refused" : "a negative length is refused"));

    /* Commitment keeps the selected binding, as in any once expression. */
    assert(answers_are(mt_eval(m, E("let", V("value"), E("once", E("unify", V("x"), 3, V("x"), E("empty"))), V("x"))), E(N(three))) && "once keeps its binding");

    mt_atom *held[] = { listed[0], listed[1], literal, quoted, units, one_to_three, bits, shared, copies, pair, zero_to_five, abb, two_ones, nothing, row_y,
                        zero_to_six, zero_to_three, unit_or_a, bag_goal, expected_bag, actual_bag, lengths[0], lengths[1] };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    for (size_t i = 0; i < sizeof power_rows / sizeof *power_rows; i++) mt_drop(power_rows[i].population);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without GMP's headers the program only says what it needs. */
int main(void)
{
    fputs("43-testing_lib.c needs GMP: install its development files, then build with\n"
          "cc 43-testing_lib.c $(pkg-config --cflags --libs cmetta gmp)\n", stderr);
    return 77;
}
#endif
