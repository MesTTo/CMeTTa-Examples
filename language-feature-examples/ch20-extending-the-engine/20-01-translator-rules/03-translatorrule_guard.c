/* Purpose: rules that decline. A translator rule's head is a pattern, so a
 *   call of another shape is left to ordinary dispatch, where it has no
 *   answer rather than an error; that is what lets a definition hold a miss.
 *   add-pairs and hold-pairs add pairs through one body, PAIR_SUM, which over
 *   lowering.h's atom builders is the rules' expansion and over its C
 *   operators is the pair C expects; hold-pairs' second equation hands a
 *   miss back as the call it was. union is the engine's own rule written
 *   that way: two superpositions become one stream, which C concatenates
 *   from its own arrays, and anything else comes back as written. A rule's
 *   body is its condition too, so (pick a) is rewritten by the second
 *   equation, and a rule no clause applies to declines as a whole. The
 *   same two equations written as a plain function answer twice. The
 *   dispatch policy the original writes into &metta is a row C adds to the
 *   catalog, its member named from vocabularies.h's NoMatchEnum words.
 * Guarantees: all eleven claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

#define PAIR_SUM(ADD, a, b, c, d) E("pair", ADD(a, c), ADD(b, d))

static const int64_t left[] = { 1, 2 }, right[] = { 10, 20 };

/* (: name (-> Atom Atom %Undefined%)) or, for one argument, (-> Atom %Undefined%). */
static mt_atom *atom_typed(const char *name, size_t arity)
{
    return E(":", name, arity == 2 ? E("->", "Atom", "Atom", "%Undefined%") : E("->", "Atom", "%Undefined%"));
}

/* (name (pair $a $b) (pair $c $d)) -> (noeval <the sum>) */
static mt_atom *summing(const char *name)
{
    return E("=", E(name, E("pair", V("a"), V("b")), E("pair", V("c"), V("d"))),
             E("noeval", PAIR_SUM(T_ADD, V("a"), V("b"), V("c"), V("d"))));
}

static void rule(metta *m, const char *name)
{
    require(name, mt_one_truth(mt_eval(m, E("add-translator-rule!", name))));
}

static mt_atom *pairs_call(const char *name) { return E(name, E("pair", left[0], left[1]), E("pair", right[0], right[1])); }

/* C's own union of two streams: the first's items, then the second's. */
static mt_list streamed(const int64_t *a, size_t n, const int64_t *b, size_t k)
{
    mt_list out = { mt_alloc((n + k ? n + k : 1) * sizeof *out.items), 0 };
    require("room", out.items != NULL);
    for (size_t i = 0; i < n; i++) out.items[out.len++] = N(a[i]);
    for (size_t i = 0; i < k; i++) out.items[out.len++] = N(b[i]);
    return out;
}

/* (superpose (<items>)) over C's integers. */
static mt_atom *superposed(const int64_t *items, size_t n)
{
    mt_list list = streamed(items, n, NULL, 0);
    mt_atom *expression = mt_exprv(list.len, list.items);
    mt_free(list.items);
    return E("superpose", expression);
}

int main(void)
{
    metta *m = open_engine();
    require("add-pairs misses as failure",
            mt_add(mt_catalog(m), E("dispatch-policy", "add-pairs", "NoMatchEnum",
                                    mt_no_match_enum_names[MT_NO_MATCH_ENUM_NO_MATCH_FAIL])));

    require("add-pairs's type", mt_add(m, atom_typed("add-pairs", 2)));
    require("add-pairs's equation", mt_add(m, summing("add-pairs")));
    rule(m, "add-pairs");
    check_answers("a call of the rule's shape is rewritten", mt_eval(m, pairs_call("add-pairs")),
                  PAIR_SUM(C_ADD, left[0], left[1], right[0], right[1]));
    check_none("a call of another shape has no answer", mt_eval(m, E("add-pairs", 1, 2)));
    require("a definition holding a miss", mt_add(m, E("=", E("holds-a-miss"), E("add-pairs", 1, 2))));
    check_none("so a definition can hold a miss", mt_eval(m, E("holds-a-miss")));

    require("hold-pairs's type", mt_add(m, atom_typed("hold-pairs", 2)));
    require("hold-pairs's equation", mt_add(m, summing("hold-pairs")));
    require("and its identity",
            mt_add(m, E("=", E("hold-pairs", V("a"), V("b")), E("noeval", E("noeval", E("hold-pairs", V("a"), V("b")))))));
    rule(m, "hold-pairs");
    check_answers("the guarded equation wins where it fits", mt_eval(m, pairs_call("hold-pairs")),
                  PAIR_SUM(C_ADD, left[0], left[1], right[0], right[1]));
    check_answers("and a miss comes back as data", mt_eval(m, E("hold-pairs", 1, 2)), E("hold-pairs", 1, 2));

    static const int64_t first[] = { 1, 2 }, second[] = { 2, 3 };
    mt_list merged = mt_all(mt_eval(m, E("union", superposed(first, 2), superposed(second, 2))));
    mt_list want = streamed(first, 2, second, 2);
    check_list_("union is one stream of both", merged, want.len, want.items);
    mt_free(want.items);
    check_answers("and anything else as written", mt_eval(m, E("union", "foo", "bar")), E("union", "foo", "bar"));

    require("pick's type", mt_add(m, atom_typed("pick", 1)));
    require("a clause whose body declines", mt_add(m, E("=", E("pick", "a"), E("empty"))));
    require("and the next", mt_add(m, E("=", E("pick", V("x")), E("noeval", E("picked", V("x"))))));
    rule(m, "pick");
    check_answers("a declining body passes to the next clause", mt_eval(m, E("pick", "a")), E("picked", "a"));
    check_answers("which takes the rest", mt_eval(m, E("pick", "b")), E("picked", "b"));

    require("only-a's type", mt_add(m, atom_typed("only-a", 1)));
    require("its one declining clause", mt_add(m, E("=", E("only-a", "a"), E("empty"))));
    rule(m, "only-a");
    check_none("a rule no clause applies to declines as a whole", mt_eval(m, E("only-a", "a")));

    require("both-ways, once", mt_add(m, E("=", E("both-ways", V("x")), "bw-one")));
    require("and twice", mt_add(m, E("=", E("both-ways", V("x")), "bw-two")));
    check_answers("a plain function answers every clause", mt_eval(m, E("both-ways", "q")), "bw-one", "bw-two");
    return done(m);
}
