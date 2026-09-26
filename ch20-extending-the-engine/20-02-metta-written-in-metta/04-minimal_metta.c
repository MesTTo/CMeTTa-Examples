/* Purpose: minimal MeTTa's instruction set, run from minimal_metta_lib, and
 *   held to C's own model of each instruction. C's unify_mod() is the
 *   matcher the library's unify-mod is: a one-argument (:= x) pattern
 *   matches by equality, a pattern holding ... matches any run of atoms
 *   there, and anything else unifies, through cmetta's mt_unify, with the
 *   bindings substituted into the branch taken. mm-switch is C's walk over
 *   its cases with that matcher, answering nothing where no case matches
 *   and NotReducible from the internal worker. mm-reduce's loop is C's
 *   step_down(), STEP_DOWN compiled over the C_ and T_ operators, the body
 *   built as the engine's equation. collapse-bind's rows and what superpose-bind
 *   restores are C's edge table. The Turing machine is C's: a tape of two
 *   stacks and the cell under the machine, moved by the library's rule,
 *   a blank read past either end, run by the rule table C also turns into
 *   the machine's equations.
 * Guarantees: all thirty-three claims of the original hold
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

/* Whether a query answers exactly one value, where a value that is Empty is
   no answer at all: the engine answers a top-level Empty with nothing, and
   inside an expression Empty stays data. Takes both. */
static inline bool value_is(mt_answers *answers, mt_atom *value)
{
    mt_atom *empty = mt_sym("Empty");
    bool nothing = value && mt_eq(value, empty);
    mt_drop(empty);
    if (nothing) mt_drop(value);
    return answers_are(answers, nothing ? mt_exprv(0, NULL) : mt_exprv(1, &value));
}

/* Each MeTTa operator twice, for a body written once over its operators:
   C_X computes in C, and T_X builds the atom (X a b). */
#define C_IF(c, t, e) ((c) ? (t) : (e))
#define T_IF(c, t, e) mt_expr("if", c, t, e)
#define C_GT(a, b) ((a) > (b))
#define T_GT(a, b) mt_expr(">", a, b)
#define C_ADD(a, b) ((a) + (b))
#define T_ADD(a, b) mt_expr("+", a, b)
#define C_SUB(a, b) ((a) - (b))
#define T_SUB(a, b) mt_expr("-", a, b)

/* --------------------------------------------------------------- matching */

static bool is_symbol(const mt_atom *a, const char *name)
{
    return mt_kind_of(a) == MT_SYMBOL && strcmp(mt_name(a), name) == 0;
}

static bool headed(const mt_atom *a, const char *head)
{
    return mt_kind_of(a) == MT_EXPR && mt_len(a) > 0 && is_symbol(mt_at(a, 0), head);
}

static bool segmented(const mt_atom *pattern)
{
    if (mt_kind_of(pattern) != MT_EXPR) return false;
    for (size_t i = 0; i < mt_len(pattern); i++)
        if (is_symbol(mt_at(pattern, i), "...")) return true;
    return false;
}

/* An expression of N borrowed atoms, each kept. */
static mt_atom *kept(const mt_atom *const *items, size_t n)
{
    mt_atom **owned = malloc((n ? n : 1) * sizeof *owned);
    require("room", owned != NULL);
    for (size_t i = 0; i < n; i++) owned[i] = mt_keep(items[i]);
    mt_atom *tuple = mt_exprv(n, owned);
    free(owned);
    return tuple;
}

/* Align ITEMS from i with PATTERN from j: ... takes any run, the shortest
   first, and every other pattern item lands on one item. A complete
   alignment unifies its pairs as one expression, so they share one
   substitution. Time: O(C(n, s) * n) for n items and s segments. */
static mt_bindings *aligned(const mt_atom *items, size_t i, const mt_atom *pattern, size_t j,
                            const mt_atom **left, const mt_atom **right, size_t n)
{
    if (j == mt_len(pattern)) {
        if (i != mt_len(items)) return NULL;
        mt_atom *l = kept(left, n), *r = kept(right, n);
        mt_bindings *found = mt_unify(l, r);
        mt_drop(l), mt_drop(r);
        return found;
    }
    if (is_symbol(mt_at(pattern, j), "...")) {
        for (size_t k = i; k <= mt_len(items); k++) {
            mt_bindings *found = aligned(items, k, pattern, j + 1, left, right, n);
            if (found) return found;
        }
        return NULL;
    }
    if (i == mt_len(items)) return NULL;
    left[n] = mt_at(items, i);
    right[n] = mt_at(pattern, j);
    return aligned(items, i + 1, pattern, j + 1, left, right, n + 1);
}

/* The substitution under which ATOM matches PATTERN as unify-mod matches,
   or NULL. */
static mt_bindings *matches(const mt_atom *atom, const mt_atom *pattern)
{
    if (headed(pattern, ":=") && mt_len(pattern) == 2) return mt_eq(atom, mt_at(pattern, 1)) ? mt_unify(atom, atom) : NULL;
    if (segmented(pattern)) {
        if (mt_kind_of(atom) != MT_EXPR) return NULL;
        size_t most = mt_len(pattern);
        const mt_atom **left = malloc(most * sizeof *left), **right = malloc(most * sizeof *right);
        require("room", left && right);
        mt_bindings *found = aligned(atom, 0, pattern, 0, left, right, 0);
        free(left), free(right);
        return found;
    }
    return mt_unify(atom, pattern);
}

/* C's unify-mod: THEN under the match's bindings, or OTHERWISE. TAKES both
   branches. */
static mt_atom *unify_mod(const mt_atom *atom, const mt_atom *pattern, mt_atom *then, mt_atom *otherwise)
{
    mt_bindings *found = matches(atom, pattern);
    mt_atom *taken = found ? mt_substitute(then, found) : mt_keep(otherwise);
    mt_bindings_free(found);
    mt_drop(then), mt_drop(otherwise);
    return taken;
}

/* CASE's template, substituted, when ATOM matches its pattern; else NULL.
   A case is (pattern template). */
static mt_atom *case_answer(const mt_atom *atom, const mt_atom *c)
{
    mt_bindings *found = matches(atom, mt_at(c, 0));
    mt_atom *template = found ? mt_substitute(mt_at(c, 1), found) : NULL;
    mt_bindings_free(found);
    return template;
}

/* C's mm-switch: the first of CASES that ATOM matches, or NULL for none,
   which mm-switch answers as Empty, no answer at all. */
static mt_atom *switched(const mt_atom *atom, const mt_atom *cases)
{
    mt_atom *template = NULL;
    for (size_t i = 0; i < mt_len(cases) && !template; i++) template = case_answer(atom, mt_at(cases, i));
    return template;
}

/* C's mm-switch-internal over (case tail): the case's template, else
   NotReducible where the tail is empty, else the switch over the tail. */
static mt_atom *switched_internal(const mt_atom *atom, const mt_atom *pair)
{
    mt_atom *template = case_answer(atom, mt_at(pair, 0));
    if (template) return template;
    const mt_atom *tail = mt_at(pair, 1);
    if (mt_len(tail) == 0) return S("NotReducible");
    template = switched(atom, tail);
    return template ? template : S("NotReducible");
}

/* ------------------------------------------------------------------ reduce */

#define STEP_DOWN(IF, GT, SUB, SELF, DONE, n) IF(GT(n, 0), SELF(SUB(n, 1)), DONE)
#define T_STEP_DOWN(n) E("step-down", n)
static const char *step_down(int64_t n) { return STEP_DOWN(C_IF, C_GT, C_SUB, step_down, "done", n); }

/* TEMPLATE with VAR bound to VALUE, as mm-subst answers. Borrows all. */
static mt_atom *substituted(const mt_atom *value, const mt_atom *var, const mt_atom *template)
{
    mt_bindings *binding = mt_unify(var, value);
    require("a variable binds", binding != NULL);
    mt_atom *out = mt_substitute(template, binding);
    mt_bindings_free(binding);
    return out;
}

/* --------------------------------------------------------- the machine */

enum { BLANK = 0 };

/* A stack of cells, the nearest to the machine last. */
typedef struct cells {
    int64_t *at;
    size_t n, cap;
} cells;

static void push(cells *c, int64_t v)
{
    if (c->n == c->cap) {
        size_t cap = c->cap ? 2 * c->cap : 8;
        int64_t *grown = realloc(c->at, cap * sizeof *grown);
        require("room on the tape", grown != NULL);
        c->at = grown;
        c->cap = cap;
    }
    c->at[c->n++] = v;
}

static int64_t pop_or_blank(cells *c) { return c->n ? c->at[--c->n] : BLANK; }

/* The library's tape: the cells to the left, the cell under the machine,
   and the cells to the right. */
typedef struct tape {
    cells left, right;
    int64_t hole;
} tape;

/* A run of cells written nearest first, as the tape's atom lists them. */
typedef struct run {
    const int64_t *cells;
    size_t n;
} run;
#define RUN(...) ((run){ (const int64_t[]){ __VA_ARGS__ }, sizeof((const int64_t[]){ __VA_ARGS__ }) / sizeof(int64_t) })
#define NONE ((run){ NULL, 0 })

static tape tape_of(run left, int64_t hole, run right)
{
    tape t = { .hole = hole };
    for (size_t i = left.n; i-- > 0;) push(&t.left, left.cells[i]);
    for (size_t i = right.n; i-- > 0;) push(&t.right, right.cells[i]);
    return t;
}

static void tape_free(tape *t) { free(t->left.at), free(t->right.at); }

static mt_atom *listed(const cells *c)
{
    mt_atom **items = malloc((c->n ? c->n : 1) * sizeof *items);
    require("room", items != NULL);
    for (size_t i = 0; i < c->n; i++) items[i] = N(c->at[c->n - 1 - i]);
    mt_atom *list = mt_exprv(c->n, items);
    free(items);
    return list;
}

static mt_atom *tape_atom(const tape *t) { return E(listed(&t->left), t->hole, listed(&t->right)); }

/* mm-move: write CHAR under the machine and go one cell DIR, R, L or N. */
static void move(tape *t, int64_t written, char dir)
{
    if (dir == 'N') {
        t->hole = written;
    } else if (dir == 'R') {
        push(&t->left, written);
        t->hole = pop_or_blank(&t->right);
    } else {
        push(&t->right, written);
        t->hole = pop_or_blank(&t->left);
    }
}

static const char *const directions[] = { "R", "L", "N" };
static const char *direction(char dir) { return dir == 'R' ? directions[0] : dir == 'L' ? directions[1] : directions[2]; }

typedef struct rule {
    const char *state;
    int64_t read;
    const char *next;
    int64_t written;
    char dir;
} rule;

/* Flip a 0 to a 1 and walk right until a 1 is read, then halt. */
static const rule rules[] = { { "S", 0, "S", 1, 'R' }, { "S", 1, "HALT", 1, 'N' } };
#define RULES (sizeof rules / sizeof *rules)

/* mm-tm: run from STATE until HALT, reading each cell and moving by the
   rule for it. */
static void run_machine(tape *t, const char *state)
{
    while (strcmp(state, "HALT") != 0) {
        const rule *r = NULL;
        for (size_t i = 0; i < RULES && !r; i++)
            if (strcmp(rules[i].state, state) == 0 && rules[i].read == t->hole) r = &rules[i];
        require("a rule for the state and the cell", r != NULL);
        move(t, r->written, r->dir);
        state = r->next;
    }
}

/* ------------------------------------------------------------------- main */

static mt_answers *function(metta *m, mt_atom *body) { return mt_eval(m, E("function", body)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import minimal_metta_lib", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "minimal_metta_lib")))));

    assert(mt_one_int(function(m, E("return", 42))) == 42 && "function answers what return hands back");
    assert(mt_one_int(function(m, E("chain", T_ADD(1, 2), V("x"), E("return", V("x"))))) == C_ADD(1, 2)
           && "chain binds the value its operand produced");
    assert(answers_are(mt_eval(m, E("return", 7)), E(E("return", 7))) && "return is data outside function");
    assert(answers_are(function(m, E("foo", "bar")), E(E("Error", E("function", E("foo", "bar")), "NoReturn")))
           && "a body that never returns is an error on the original frame");

    struct { mt_atom *atom, *pattern, *then, *otherwise; } unifications[] = {
        { V("a"), S("Empty"), S("then"), S("else") },
        { V("a"), E(":=", "Empty"), S("then"), S("else") },
        { E(":=", "a", "b"), E(":=", V("x"), V("y")), S("then"), S("else") },
        { E("A", "B", "C", "D", "E"), E("A", "...", "D", "..."), S("matched"), S("nomatch") },
        { E("p", 5), E("p", V("x")), E("got", V("x")), S("else") },
    };
    for (size_t i = 0; i < sizeof unifications / sizeof *unifications; i++) {
        mt_atom *want = unify_mod(unifications[i].atom, unifications[i].pattern, mt_keep(unifications[i].then),
                                  mt_keep(unifications[i].otherwise));
        assert(answers_are(mt_eval(m, E("unify-mod", unifications[i].atom, unifications[i].pattern, unifications[i].then,
                                        unifications[i].otherwise)), E(want))
               && "unify-mod takes the branch C's matcher takes");
    }

    mt_atom *cases = E(E(1, "one"), E(2, "two")), *by_shape = E(E(E("p", V("x")), E("got", V("x"))));
    struct { mt_atom *atom; const mt_atom *cases; } switches[] = { { N(1), cases }, { N(2), cases }, { E("p", 5), by_shape } };
    for (size_t i = 0; i < sizeof switches / sizeof *switches; i++)
        assert(answers_are(mt_eval(m, E("mm-switch", mt_keep(switches[i].atom), mt_keep(switches[i].cases))), E(switched(switches[i].atom, switches[i].cases)))
               && "mm-switch answers the case C's walk finds");
    mt_atom *nine = N(9), *missed = switched(nine, cases);
    require("C's walk finds no case for 9", missed == NULL);
    assert(!mt_first(mt_eval(m, E("mm-switch", mt_keep(nine), mt_keep(cases)))) && mt_ok() && "so mm-switch answers nothing");

    require("step-down", mt_add(m, E("=", T_STEP_DOWN(V("n")), STEP_DOWN(T_IF, T_GT, T_SUB, T_STEP_DOWN, "done", V("n")))));
    const int64_t steps = 3;
    mt_atom *done_atom = S(step_down(steps));
    assert(answers_are(mt_eval(m, E("mm-reduce", E("step-down", steps), V("x"), V("x"))), E(mt_keep(done_atom)))
           && "mm-reduce reduces until nothing changes");
    mt_atom *x = V("x"), *wrapped = E("wrapped", V("x"));
    assert(answers_are(mt_eval(m, E("mm-reduce", E("step-down", steps), mt_keep(x), mt_keep(wrapped))), E(substituted(done_atom, x, wrapped)))
           && "then substitutes into the template");
    assert(mt_one_int(mt_eval(m, E("mm-reduce", T_ADD(1, 2), V("y"), V("y")))) == C_ADD(1, 2) && "an arithmetic step reduces too");
    mt_drop(done_atom), mt_drop(x), mt_drop(wrapped);

    static const char *const reached[] = { "b", "c" };
    enum { REACHED = sizeof reached / sizeof *reached };
    for (size_t i = 0; i < REACHED; i++) require("an edge", mt_add(m, E("edge", "a", reached[i])));
    mt_atom *found_rows = E("match", mt_spaceref("&self"), E("edge", "a", V("y")), E("found", V("y")));
    mt_atom *rows[REACHED], *founds[REACHED], *restored[REACHED];
    for (size_t i = 0; i < REACHED; i++) {
        rows[i] = E(E("found", reached[i]), E("bindings", E("<-", V("y"), reached[i])));
        founds[i] = E("found", reached[i]);
        restored[i] = E(E("found", reached[i]), reached[i]);
    }
    assert(answers_are(mt_eval(m, E("collapse-bind", mt_keep(found_rows))), E(mt_exprv(REACHED, rows)))
           && "collapse-bind pairs each answer with its bindings");
    assert(answers_are(mt_eval(m, E("chain", E("collapse-bind", mt_keep(found_rows)), V("rows"), E("superpose-bind", V("rows")))), mt_exprv(REACHED, founds))
           && "superpose-bind puts each row back");
    assert(answers_are(mt_eval(m, E("chain", E("collapse-bind", mt_keep(found_rows)), V("c"),
                                    E("chain", E("superpose-bind", V("c")), V("x"), E(V("x"), V("y"))))), mt_exprv(REACHED, restored))
           && "and restores the row's own bindings");
    mt_drop(found_rows);

    for (size_t i = 0; i < RULES; i++)
        require("a rule of the machine", mt_add(m, E("=", E("rule", rules[i].state, rules[i].read),
                                                     E(rules[i].next, rules[i].written, direction(rules[i].dir)))));
    tape machines[] = { tape_of(NONE, 1, NONE), tape_of(NONE, 0, RUN(1)), tape_of(NONE, 0, RUN(0, 0, 1)) };
    for (size_t i = 0; i < sizeof machines / sizeof *machines; i++) {
        mt_atom *start = tape_atom(&machines[i]);
        run_machine(&machines[i], "S");
        assert(answers_are(mt_eval(m, E("mm-tm", "rule", "S", start)), E(tape_atom(&machines[i]))) && "the machine halts on C's tape");
        tape_free(&machines[i]);
    }

    struct { tape tape; int64_t written; char dir; } moves[] = {
        { tape_of(NONE, 0, RUN(7)), 1, 'R' },
        { tape_of(RUN(9), 0, NONE), 1, 'L' },
        { tape_of(NONE, 0, NONE), 1, 'R' },
        { tape_of(RUN(1), 0, RUN(2)), 9, 'N' },
    };
    for (size_t i = 0; i < sizeof moves / sizeof *moves; i++) {
        mt_atom *start = tape_atom(&moves[i].tape);
        move(&moves[i].tape, moves[i].written, moves[i].dir);
        assert(answers_are(mt_eval(m, E("mm-move", start, moves[i].written, direction(moves[i].dir))), E(tape_atom(&moves[i].tape)))
               && "mm-move is C's move");
        tape_free(&moves[i].tape);
    }

    mt_atom *yes = S("yes"), *truth = B(true), *falsity = B(false);
    assert(answers_are(mt_eval(m, E("if-partial", mt_keep(truth), mt_keep(yes))), E(unify_mod(truth, truth, mt_keep(yes), S("Empty"))))
           && "a partial function answers where it is defined");
    mt_atom *declined = unify_mod(falsity, truth, mt_keep(yes), S("Empty"));
    assert(value_is(mt_eval(m, E("if-partial", mt_keep(falsity), mt_keep(yes))), declined) && "and nothing where it is not");
    mt_drop(yes), mt_drop(truth), mt_drop(falsity);

    tape read = tape_of(RUN(1), 7, RUN(2));
    assert(mt_one_int(mt_eval(m, E("mm-read", tape_atom(&read)))) == read.hole && "mm-read is the cell under the machine");
    mt_atom *one_case = E(E(1, "one"), mt_exprv(0, NULL));
    static const int64_t looked_up[] = { 1, 9 };
    for (size_t i = 0; i < sizeof looked_up / sizeof *looked_up; i++) {
        mt_atom *key = N(looked_up[i]);
        assert(answers_are(function(m, E("chain", E("eval", E("mm-switch-internal", mt_keep(key), mt_keep(one_case))), V("r"),
                                         E("return", V("r")))), E(switched_internal(key, one_case)))
               && "the worker answers the case or NotReducible");
        mt_drop(key);
    }
    mt_drop(one_case);
    mt_atom *five = N(5), *var = V("x"), *doubled = E(V("x"), V("x"));
    assert(answers_are(mt_eval(m, E("mm-subst", mt_keep(five), mt_keep(var), mt_keep(doubled))), E(substituted(five, var, doubled)))
           && "mm-subst replaces the variable throughout");
    mt_drop(five), mt_drop(var), mt_drop(doubled);
    assert(answers_are(function(m, E("eval", E("mm-tm-body", "unused", "HALT", tape_atom(&read)))), E(tape_atom(&read)))
           && "a halted machine is its tape");
    tape_free(&read);
    mt_drop(cases), mt_drop(by_shape), mt_drop(nine);
    for (size_t i = 0; i < sizeof switches / sizeof *switches; i++) mt_drop(switches[i].atom);
    mt_close(m);
    return 0;
}
