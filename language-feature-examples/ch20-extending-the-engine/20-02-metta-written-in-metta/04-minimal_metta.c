/* Purpose: minimal MeTTa's instruction set, run from minimal_metta_lib, and
 *   held to C's own model of each instruction. C's unify_mod() is the
 *   matcher the library's unify-mod is: a one-argument (:= x) pattern
 *   matches by equality, a pattern holding ... matches any run of atoms
 *   there, and anything else unifies, through cmetta's mt_unify, with the
 *   bindings substituted into the branch taken. mm-switch is C's walk over
 *   its cases with that matcher, answering nothing where no case matches
 *   and NotReducible from the internal worker. mm-reduce's loop is C's
 *   step_down(), STEP_DOWN compiled over lowering.h's operators, the body
 *   lowered for the engine. collapse-bind's rows and what superpose-bind
 *   restores are C's edge table. The Turing machine is C's: a tape of two
 *   stacks and the cell under the machine, moved by the library's rule,
 *   a blank read past either end, run by the rule table C also turns into
 *   the machine's equations.
 * Guarantees: all thirty-three claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

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
#define M_STEP_DOWN(n) (step-down n)
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
    metta *m = open_engine();
    require("import minimal_metta_lib", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "minimal_metta_lib")))));

    check_int("function answers what return hands back", mt_one_int(function(m, E("return", 42))), 42);
    check_int("chain binds the value its operand produced",
              mt_one_int(function(m, E("chain", T_ADD(1, 2), V("x"), E("return", V("x"))))), C_ADD(1, 2));
    check_answers("return is data outside function", mt_eval(m, E("return", 7)), E("return", 7));
    check_answers("a body that never returns is an error on the original frame", function(m, E("foo", "bar")),
                  E("Error", E("function", E("foo", "bar")), "NoReturn"));

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
        check_answers("unify-mod takes the branch C's matcher takes",
                      mt_eval(m, E("unify-mod", unifications[i].atom, unifications[i].pattern, unifications[i].then,
                                   unifications[i].otherwise)),
                      want);
    }

    mt_atom *cases = E(E(1, "one"), E(2, "two")), *by_shape = E(E(E("p", V("x")), E("got", V("x"))));
    struct { mt_atom *atom; const mt_atom *cases; } switches[] = { { N(1), cases }, { N(2), cases }, { E("p", 5), by_shape } };
    for (size_t i = 0; i < sizeof switches / sizeof *switches; i++)
        check_answers("mm-switch answers the case C's walk finds",
                      mt_eval(m, E("mm-switch", mt_keep(switches[i].atom), mt_keep(switches[i].cases))),
                      switched(switches[i].atom, switches[i].cases));
    mt_atom *nine = N(9), *missed = switched(nine, cases);
    require("C's walk finds no case for 9", missed == NULL);
    check_none("so mm-switch answers nothing", mt_eval(m, E("mm-switch", mt_keep(nine), mt_keep(cases))));

    require("step-down", mt_lower(m, (step-down $n), STEP_DOWN(M_IF, M_GT, M_SUB, M_STEP_DOWN, done, $n)));
    const int64_t steps = 3;
    mt_atom *done_atom = S(step_down(steps));
    check_answers("mm-reduce reduces until nothing changes", mt_eval(m, E("mm-reduce", E("step-down", steps), V("x"), V("x"))),
                  mt_keep(done_atom));
    mt_atom *x = V("x"), *wrapped = E("wrapped", V("x"));
    check_answers("then substitutes into the template",
                  mt_eval(m, E("mm-reduce", E("step-down", steps), mt_keep(x), mt_keep(wrapped))),
                  substituted(done_atom, x, wrapped));
    check_int("an arithmetic step reduces too", mt_one_int(mt_eval(m, E("mm-reduce", T_ADD(1, 2), V("y"), V("y")))), C_ADD(1, 2));
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
    check_answers("collapse-bind pairs each answer with its bindings", mt_eval(m, E("collapse-bind", mt_keep(found_rows))),
                  mt_exprv(REACHED, rows));
    check_answers_("superpose-bind puts each row back",
                   mt_eval(m, E("chain", E("collapse-bind", mt_keep(found_rows)), V("rows"), E("superpose-bind", V("rows")))),
                   REACHED, founds);
    check_answers_("and restores the row's own bindings",
                   mt_eval(m, E("chain", E("collapse-bind", mt_keep(found_rows)), V("c"),
                                E("chain", E("superpose-bind", V("c")), V("x"), E(V("x"), V("y"))))),
                   REACHED, restored);
    mt_drop(found_rows);

    for (size_t i = 0; i < RULES; i++)
        require("a rule of the machine", mt_add(m, E("=", E("rule", rules[i].state, rules[i].read),
                                                     E(rules[i].next, rules[i].written, direction(rules[i].dir)))));
    tape machines[] = { tape_of(NONE, 1, NONE), tape_of(NONE, 0, RUN(1)), tape_of(NONE, 0, RUN(0, 0, 1)) };
    for (size_t i = 0; i < sizeof machines / sizeof *machines; i++) {
        mt_atom *start = tape_atom(&machines[i]);
        run_machine(&machines[i], "S");
        check_answers("the machine halts on C's tape", mt_eval(m, E("mm-tm", "rule", "S", start)), tape_atom(&machines[i]));
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
        check_answers("mm-move is C's move", mt_eval(m, E("mm-move", start, moves[i].written, direction(moves[i].dir))),
                      tape_atom(&moves[i].tape));
        tape_free(&moves[i].tape);
    }

    mt_atom *yes = S("yes"), *truth = B(true), *falsity = B(false);
    check_answers("a partial function answers where it is defined", mt_eval(m, E("if-partial", mt_keep(truth), mt_keep(yes))),
                  unify_mod(truth, truth, mt_keep(yes), S("Empty")));
    mt_atom *declined = unify_mod(falsity, truth, mt_keep(yes), S("Empty"));
    check_value("and nothing where it is not", mt_eval(m, E("if-partial", mt_keep(falsity), mt_keep(yes))), declined);
    mt_drop(yes), mt_drop(truth), mt_drop(falsity);

    tape read = tape_of(RUN(1), 7, RUN(2));
    check_int("mm-read is the cell under the machine", mt_one_int(mt_eval(m, E("mm-read", tape_atom(&read)))), read.hole);
    mt_atom *one_case = E(E(1, "one"), mt_exprv(0, NULL));
    static const int64_t looked_up[] = { 1, 9 };
    for (size_t i = 0; i < sizeof looked_up / sizeof *looked_up; i++) {
        mt_atom *key = N(looked_up[i]);
        check_answers("the worker answers the case or NotReducible",
                      function(m, E("chain", E("eval", E("mm-switch-internal", mt_keep(key), mt_keep(one_case))), V("r"),
                                    E("return", V("r")))),
                      switched_internal(key, one_case));
        mt_drop(key);
    }
    mt_drop(one_case);
    mt_atom *five = N(5), *var = V("x"), *doubled = E(V("x"), V("x"));
    check_answers("mm-subst replaces the variable throughout",
                  mt_eval(m, E("mm-subst", mt_keep(five), mt_keep(var), mt_keep(doubled))), substituted(five, var, doubled));
    mt_drop(five), mt_drop(var), mt_drop(doubled);
    check_answers("a halted machine is its tape", function(m, E("eval", E("mm-tm-body", "unused", "HALT", tape_atom(&read)))),
                  tape_atom(&read));
    tape_free(&read);
    mt_drop(cases), mt_drop(by_shape), mt_drop(nine);
    for (size_t i = 0; i < sizeof switches / sizeof *switches; i++) mt_drop(switches[i].atom);
    return done(m);
}
