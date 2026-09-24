/* Purpose: lib_strategy from the inside, held to C's model of each
 *   operation. A strategy applied to a term answers at most one atom here:
 *   mark, preserve and summarize wrap it, their equations built from C's
 *   table of wrappers, id answers it and fail declines. choice-tail takes
 *   the first strategy with an answer; all applies the strategy to every
 *   immediate child, the head included, and leaves a leaf or the empty
 *   expression as it is; all-tail does the same over an expression and
 *   declines on a leaf; one answers a rewrite at each child position that
 *   rewrites, left to right. The typed operations read C's declarations:
 *   the type-preserving scheme takes the sort of an arrow with the same
 *   sort on both sides, the type-unifying one the argument sort of an arrow
 *   answering the sort asked, and a strategy then applies only to a term
 *   whose declared type is that sort.
 * Guarantees: all twenty-three claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

/* The strategies whose equations wrap their term, (= (name $x) (wrapper $x)). */
static const struct { const char *name, *wrapper; } wrapping[] = {
    { "mark", "marked" }, { "preserve", "kept" }, { "summarize", "sum" },
};
#define WRAPPING (sizeof wrapping / sizeof *wrapping)

/* What STRATEGY answers for TERM, or NULL where it declines. Borrows. */
static mt_atom *applied(const char *strategy, const mt_atom *term)
{
    if (strcmp(strategy, "id") == 0) return mt_keep(term);
    for (size_t i = 0; i < WRAPPING; i++)
        if (strcmp(wrapping[i].name, strategy) == 0) return E(wrapping[i].wrapper, mt_keep(term));
    return NULL;
}

static bool leaf_or_empty(const mt_atom *term) { return mt_kind_of(term) != MT_EXPR || mt_len(term) == 0; }

/* strategy-all-tail: every item rewritten, or NULL where one declines or
   TERM is no expression. */
static mt_atom *all_tail(const char *strategy, const mt_atom *term)
{
    if (mt_kind_of(term) != MT_EXPR) return NULL;
    size_t n = mt_len(term);
    mt_atom **items = malloc((n ? n : 1) * sizeof *items);
    require("room", items != NULL);
    for (size_t i = 0; i < n; i++) {
        items[i] = applied(strategy, mt_at(term, i));
        if (!items[i]) {
            while (i-- > 0) mt_drop(items[i]);
            free(items);
            return NULL;
        }
    }
    mt_atom *rewritten = mt_exprv(n, items);
    free(items);
    return rewritten;
}

/* strategy-all: a leaf and the empty expression are identities. */
static mt_atom *all(const char *strategy, const mt_atom *term)
{
    return leaf_or_empty(term) ? mt_keep(term) : all_tail(strategy, term);
}

/* strategy-one: the rewrite at each position that rewrites, in order. */
static mt_list one(const char *strategy, const mt_atom *term)
{
    mt_list out = { NULL, 0 };
    if (mt_kind_of(term) != MT_EXPR) return out;
    size_t n = mt_len(term);
    out.items = mt_alloc((n ? n : 1) * sizeof *out.items);
    require("room", out.items != NULL);
    for (size_t at = 0; at < n; at++) {
        mt_atom *changed = applied(strategy, mt_at(term, at));
        if (!changed) continue;
        mt_atom **items = malloc(n * sizeof *items);
        require("room", items != NULL);
        for (size_t i = 0; i < n; i++) items[i] = i == at ? changed : mt_keep(mt_at(term, i));
        out.items[out.len++] = mt_exprv(n, items);
        free(items);
    }
    return out;
}

/* The declarations, each (: atom type). */
static mt_atom *declarations[5];
#define DECLARATIONS (sizeof declarations / sizeof *declarations)

/* TERM's declared type, borrowed, or NULL. */
static const mt_atom *declared_type(const mt_atom *term)
{
    for (size_t i = 0; i < DECLARATIONS; i++)
        if (mt_eq(mt_at(declarations[i], 1), term)) return mt_at(declarations[i], 2);
    return NULL;
}

/* strategy-typed-apply: STRATEGY where TERM's declared type is EXPECTED. */
static mt_atom *typed_apply(const char *strategy, const mt_atom *expected, const mt_atom *term)
{
    const mt_atom *actual = declared_type(term);
    return expected && actual && mt_eq(actual, expected) ? applied(strategy, term) : NULL;
}

/* The argument sort of STRATEGY's arrow when its result is RESULT, or with
   RESULT NULL when both sides are one sort; else NULL. */
static const mt_atom *arrow_sort(const char *strategy, const mt_atom *result)
{
    mt_atom *name = S(strategy);
    const mt_atom *arrow = declared_type(name);
    mt_drop(name);
    if (!arrow || mt_kind_of(arrow) != MT_EXPR || mt_len(arrow) != 3) return NULL;
    const mt_atom *from = mt_at(arrow, 1), *to = mt_at(arrow, 2);
    return mt_eq(to, result ? result : from) ? from : NULL;
}

static void check_maybe(const char *claim, mt_answers *answers, mt_atom *want)
{
    if (want)
        check_answers(claim, answers, want);
    else
        check_none(claim, answers);
}

int main(void)
{
    metta *m = open_engine();
    require("import lib_strategy", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_strategy")))));
    require("mark", mt_add(m, E("=", E(wrapping[0].name, V("x")), E(wrapping[0].wrapper, V("x")))));

    mt_atom *a = S("a");
    check_maybe("strategy-eval is application", mt_eval(m, E("strategy-eval", "mark", mt_keep(a))), applied("mark", a));
    check_maybe("id answers its term", mt_eval(m, E("strategy-eval", "id", mt_keep(a))), applied("id", a));
    mt_atom *by_eval = applied("mark", a), *by_apply = applied("mark", a);
    check_answers("strategy-apply is strategy-eval",
                  mt_eval(m, E("==", E("strategy-eval", "mark", mt_keep(a)), E("strategy-apply", "mark", mt_keep(a)))),
                  B(mt_eq(by_eval, by_apply)));
    mt_drop(by_eval), mt_drop(by_apply);

    static const char *const steps[] = { "fail", "id", "mark" };
    mt_atom *sum = T_ADD(1, 2), *chosen = NULL;
    for (size_t i = 0; i < sizeof steps / sizeof *steps && !chosen; i++) chosen = applied(steps[i], sum);
    check_maybe("choice-tail takes the first strategy with an answer",
                mt_eval(m, E("strategy-choice-tail", E(steps[0], steps[1], steps[2]), mt_keep(sum))), chosen);
    check_none("and none from no strategies", mt_eval(m, E("strategy-choice-tail", mt_exprv(0, NULL), mt_keep(a))));
    mt_drop(sum);

    mt_atom *hab = E("h", "a", "b"), *ab = E("a", "b"), *leaf = S("leaf"), *unit = mt_exprv(0, NULL);
    const mt_atom *all_terms[] = { hab, leaf, unit };
    for (size_t i = 0; i < 3; i++)
        check_maybe("all rewrites every child", mt_eval(m, E("strategy-all", "mark", mt_keep(all_terms[i]))),
                    all("mark", all_terms[i]));
    const mt_atom *tail_terms[] = { ab, unit };
    for (size_t i = 0; i < 2; i++)
        check_maybe("all-tail maps over an expression", mt_eval(m, E("strategy-all-tail", "mark", mt_keep(tail_terms[i]))),
                    all_tail("mark", tail_terms[i]));
    mt_atom *tail_of_all = all("mark", hab), *mapped = all_tail("mark", ab);
    mt_atom *rest[] = { mt_keep(mt_at(tail_of_all, 1)), mt_keep(mt_at(tail_of_all, 2)) }, *rest_of_all = mt_exprv(2, rest);
    check_answers("all-tail is all without the head",
                  mt_eval(m, E("==", E("strategy-all-tail", "mark", mt_keep(ab)), E("cdr-atom", E("strategy-all", "mark", mt_keep(hab))))),
                  B(mt_eq(mapped, rest_of_all)));
    mt_drop(tail_of_all), mt_drop(mapped), mt_drop(rest_of_all);
    check_maybe("all-tail declines a leaf", mt_eval(m, E("strategy-all-tail", "mark", mt_keep(leaf))), all_tail("mark", leaf));
    check_maybe("while all keeps it", mt_eval(m, E("strategy-all", "mark", mt_keep(leaf))), all("mark", leaf));

    mt_atom *ha = E("h", "a");
    const mt_atom *one_terms[] = { ha, leaf };
    for (size_t i = 0; i < 2; i++) {
        mt_list want = one("mark", one_terms[i]);
        check_answers_("one rewrites each position on its own", mt_eval(m, E("strategy-one", "mark", mt_keep(one_terms[i]))),
                       want.len, want.items);
        mt_free(want.items);
    }
    mt_drop(hab), mt_drop(ab), mt_drop(unit), mt_drop(ha);

    declarations[0] = E(":", "DA", "Type");
    declarations[1] = E(":", "DS", "Type");
    declarations[2] = E(":", "da", "DA");
    declarations[3] = E(":", "preserve", E("->", "DA", "DA"));
    declarations[4] = E(":", "summarize", E("->", "DA", "DS"));
    for (size_t i = 0; i < 4; i++) require("a declaration", mt_add(m, mt_keep(declarations[i])));
    require("preserve", mt_add(m, E("=", E(wrapping[1].name, V("x")), E(wrapping[1].wrapper, V("x")))));
    require("summarize's declaration", mt_add(m, mt_keep(declarations[4])));
    require("summarize", mt_add(m, E("=", E(wrapping[2].name, V("x")), E(wrapping[2].wrapper, V("x")))));

    mt_atom *da = S("da"), *one_n = N(1), *DA = S("DA"), *DS = S("DS"), *undeclared = E("undeclared-name");
    const mt_atom *tp_terms[] = { da, one_n };
    for (size_t i = 0; i < 2; i++)
        check_maybe("the type-preserving scheme", mt_eval(m, E("strategy-typed-tp", "preserve", mt_keep(tp_terms[i]))),
                    typed_apply("preserve", arrow_sort("preserve", NULL), tp_terms[i]));
    const mt_atom *unified[] = { DS, DA };
    for (size_t i = 0; i < 2; i++)
        check_maybe("the type-unifying scheme",
                    mt_eval(m, E("strategy-typed-tu", "summarize", mt_keep(unified[i]), mt_keep(da))),
                    typed_apply("summarize", arrow_sort("summarize", unified[i]), da));
    const mt_atom *apply_terms[] = { da, one_n };
    for (size_t i = 0; i < 2; i++)
        check_maybe("the type check", mt_eval(m, E("strategy-typed-apply", "preserve", mt_keep(DA), mt_keep(apply_terms[i]))),
                    typed_apply("preserve", DA, apply_terms[i]));
    mt_atom *typed = typed_apply("preserve", DA, da), *by_triangle = typed_apply("preserve", arrow_sort("preserve", NULL), da);
    check_answers("◁ TP is the same application",
                  mt_eval(m, E("==", E("strategy-typed-apply", "preserve", mt_keep(DA), mt_keep(da)), E("◁", "preserve", "TP", mt_keep(da)))),
                  B(typed && by_triangle && mt_eq(typed, by_triangle)));
    mt_drop(typed), mt_drop(by_triangle);
    check_maybe("an undeclared subject declines",
                mt_eval(m, E("strategy-typed-apply", "preserve", mt_keep(DA), mt_keep(undeclared))),
                typed_apply("preserve", DA, undeclared));
    mt_drop(da), mt_drop(one_n), mt_drop(DA), mt_drop(DS), mt_drop(undeclared), mt_drop(a), mt_drop(leaf);
    for (size_t i = 0; i < DECLARATIONS; i++) mt_drop(declarations[i]);
    return done(m);
}
