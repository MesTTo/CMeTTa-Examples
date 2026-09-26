# CMeTTa by example

Build and run a program against an installed cmetta:

```sh
git clone --recurse-submodules https://github.com/MesTTo/MeTTa.git
make -C MeTTa/extensions/cmetta install PREFIX=$HOME/.local
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig
cd MeTTa/extensions/cmetta/examples/ch01-getting-started
cc first_steps.c $(pkg-config --cflags --libs cmetta) -Wl,-rpath,$HOME/.local/lib -o first_steps
./first_steps
```

A program that links more than cmetta, libm or an optional library, gives its whole command in the `Build:` line of its header.

cmetta embeds the patched SWI-Prolog 10 the engine runs on, which has to be first on your `PATH`; [docs/patched-host.md](https://github.com/MesTTo/MeTTa/blob/main/docs/patched-host.md) builds it.

The excerpts keep source indentation, and `...` marks omitted lines; the linked file holds its includes, its helpers and its checks.

## Getting started

### [ch01-getting-started/first_steps.c](ch01-getting-started/first_steps.c)

Build a term and evaluate it, publish a C function, join stored facts, and take every answer.

```c
static mt_status twice(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "double wants an integer");
    return mt_answer(call, N(2 * x));
}
...
    /* A term is built, not written: (+ 20 22). */
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "(+ 20 22) is 42");

    require("publish double", mt_def(m, (mt_op){ .name = "double", .arity = 1,
                                                 .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = twice }));
    assert(mt_one_int(mt_eval(m, E("double", 21))) == 42 && "(double 21) is 42");
...
    mt_atom *grandparent = E(",", E("Parent", V("gp"), V("p")),
                                  E("Parent", V("p"), V("gc")));
    size_t rows = 0;
    mt_rows (row, mt_query(m, grandparent, NULL)) {
        if (rows++ == 0) {
            assert(atom_is(mt_keep(mt_bound(row, "gp")), S("Tom")) && "the first grandparent is Tom");
            assert(atom_is(mt_keep(mt_bound(row, "gc")), S("Ann")) && "and the grandchild Ann");
        }
    }
    assert((int64_t)rows == 2 && "two grandparent links");
...
    /* One expression, three answers. */
    assert(answers_are(mt_eval(m, E("superpose", E(1, 2, 3))), E(1, 2, 3))
           && "superpose answers each value");
```

## Atoms and expressions

### [ch03-atoms-and-expressions/atom_values.c](ch03-atoms-and-expressions/atom_values.c)

Each atom kind is a C value, and a kept child outlives its parent.

```c
    mt_atom *record = E("record", T("Ada"), 42, B(true));
    assert(mt_kind_of(record) == MT_EXPR && "an expression");
    assert((int64_t)mt_len(record) == 4 && "of four children");
    assert(mt_kind_of(mt_at(record, 0)) == MT_SYMBOL && "the head is a symbol");
    assert(mt_kind_of(mt_at(record, 1)) == MT_TEXT && "the name is text, not a symbol");
    assert(mt_int(mt_at(record, 2)) == 42 && "the number is an integer");
    assert(mt_kind_of(mt_at(record, 3)) == MT_BOOL &&
           mt_truth(mt_at(record, 3))
           && "the flag is a boolean");

    mt_atom *name = mt_keep(mt_at(record, 1));
    mt_drop(record);
    assert(atom_is(name, T("Ada")) && "a kept child outlives its parent");
```

### [ch03-atoms-and-expressions/embedding_lifetime.c](ch03-atoms-and-expressions/embedding_lifetime.c)

An atom built before the engine opens takes part in an evaluation, and the answer outlives the close.

```c
    int64_t application_total = 10;
    mt_atom *input = N(32);                 /* no engine is running yet */
    assert(mt_int(input) == 32 && "an atom needs no engine");

    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *answer = mt_one(mt_eval(m, E("+", application_total, input)));
    assert(mt_int(answer) == 42 && "the application's value takes part");
    mt_close(m);
    require("close the engine", mt_ok());

    assert(mt_int(answer) == 42 && "the answer outlives the engine");
    mt_drop(answer);
```

## Spaces and matching

### [ch04-spaces-and-matching/04-01-a-space-is-where-a-program-lives/prepared_queries.c](ch04-spaces-and-matching/04-01-a-space-is-where-a-program-lives/prepared_queries.c)

A pattern kept by the program sees the facts the space holds when each query runs.

```c
    mt_atom *score = E("score", V("who"), V("n"));
    require("Ada scores 7", mt_add(m, E("score", "Ada", 7)));
    assert(answers_are(mt_query(m, mt_keep(score), E(">", V("n"), 5)), E(E("score", "Ada", 7)))
           && "scores above 5");
    require("Bob scores 9", mt_add(m, E("score", "Bob", 9)));
    assert(answers_are(mt_query(m, mt_keep(score), E(">", V("n"), 8)), E(E("score", "Bob", 9)))
           && "the same pattern sees the new fact");
```

### [ch04-spaces-and-matching/04-02-patterns-and-bindings/unification.c](ch04-spaces-and-matching/04-02-patterns-and-bindings/unification.c)

Unify two atoms in C, fill a template from the bindings, and refuse unequal fields for a repeated variable.

```c
    mt_atom *pattern = E("Parent", V("parent"), V("child"));
    mt_atom *fact = E("Parent", "Tom", "Bob");
    mt_bindings *bindings = mt_unify(pattern, fact);
    require("the fact unifies", bindings != NULL);
    mt_atom *template = E("Cares", V("parent"), V("child"));
    assert(atom_is(mt_substitute(template, bindings), E("Cares", "Tom", "Bob"))
           && "the bindings fill the template");
...
    mt_atom *diagonal = E("pair", V("x"), V("x"));
    mt_atom *unequal = E("pair", 1, 2);
    mt_clear();
    assert(mt_unify(diagonal, unequal) == NULL && mt_ok()
           && "a repeated variable refuses unequal fields, without an error");
```

### [ch04-spaces-and-matching/04-02-patterns-and-bindings/regex_matching.c](ch04-spaces-and-matching/04-02-patterns-and-bindings/regex_matching.c)

A C value that decides its own matches: a POSIX regex behind the engine's unify.

```c
static mt_status matches(mt_call *call, void *user)
{
    const char *name = mt_name(mt_arg(call, 0));
    if (!name) return mt_fail(call, "a regex matches symbols and text");
    int result = regexec(user, name, 0, NULL, 0);
    if (result != 0 && result != REG_NOMATCH) return mt_fail(call, "regexec failed");
    return result == 0 ? mt_answer(call, mt_keep(mt_arg(call, 0))) : MT_FAIL;
}
...
    mt_atom *matcher = mt_matcher(matches, starts_with_a, release);
    require("make it a matcher", matcher != NULL);

    assert(answers_are(mt_eval(m, E("unify", mt_keep(matcher), "abbey", "hit", "miss")), E("hit")) && "abbey matches");
    assert(answers_are(mt_eval(m, E("unify", mt_keep(matcher), "zebra", "hit", "miss")), E("miss")) && "zebra does not");
```

## Equations and evaluation

### [ch05-equations-and-evaluation/05-01-an-equation-is-a-rewrite/01-identity.c](ch05-equations-and-evaluation/05-01-an-equation-is-a-rewrite/01-identity.c)

One body written over its operators is both the C function and the equation the engine runs.

```c
#define SQUARE(MUL, x) MUL(x, x)

static int64_t square(int64_t x) { return SQUARE(C_MUL, x); }
...
    require("define f", mt_add(m, E("=", E("f", V("x")), SQUARE(T_MUL, V("x")))));
...
    bool agree = true;
    for (int64_t x = -100; x <= 100 && agree; x++)
        agree = mt_one_int(mt_eval(m, E("f", x))) == square(x);
    assert(agree && "the equation computes square() on -100..100");
```

### [ch05-equations-and-evaluation/05-01-an-equation-is-a-rewrite/routing_equations.c](ch05-equations-and-evaluation/05-01-an-equation-is-a-rewrite/routing_equations.c)

Routes are equations, a catch-all equation is the 404, and middleware is composition.

```c
    for (size_t i = 0; i < 2; i++)     /* (= (route home) (Page 200 "Welcome")) */
        require("a route", mt_add(m, E("=", E("route", pages[i].path), E("Page", 200, T(pages[i].title)))));
    /* (= (route $other) (NotFound 404 $other)) */
    require("the fallback", mt_add(m, E("=", E("route", V("other")), E("NotFound", 404, V("other")))));
    /* (= (handle $r) (once (route $r))) */
    require("handle", mt_add(m, E("=", E("handle", V("r")), E("once", E("route", V("r"))))));
    /* (= (logged $r) (let $response (handle $r) (Logged $r $response))) */
    require("logged", mt_add(m, E("=", E("logged", V("r")),
        E("let", V("response"), E("handle", V("r")), E("Logged", V("r"), V("response"))))));

    assert(answers_are(mt_eval(m, E("handle", "home")), E(E("Page", 200, T("Welcome")))) && "a known route");
    assert(answers_are(mt_eval(m, E("handle", "nowhere")), E(E("NotFound", 404, "nowhere"))) && "a missing one");
    assert(answers_are(mt_eval(m, E("logged", "about")), E(E("Logged", "about", E("Page", 200, T("About us")))))
           && "middleware composes");
```

## Many answers

### [ch06-many-answers/03-collapse.c](ch06-many-answers/03-collapse.c)

Every answer of a query, gathered into one C list.

```c
    assert(list_is(mt_all(mt_eval(m, E(1, 2, 3))), E(E(1, 2, 3))) && "(1 2 3) answers itself, once");
```

### [ch06-many-answers/native_iterator.c](ch06-many-answers/native_iterator.c)

A C generator becomes a cursor like any the engine answers, and is closed once however it ends.

```c
static mt_status count_up(void *state, mt_atom **out)
{
    counter *c = state;
    if (c->next == c->stop) return MT_DONE;
    *out = N(c->next++);
    return *out ? MT_ROW : mt_error();
}
...
    mt_answers *three = range(3, &closed);
    const mt_atom *value;
    for (int64_t i = 0; i < 3; i++) {
        require("a row", mt_step(three, &value) == MT_ROW);
        assert(mt_int(value) == i && "the counter's next value");
    }
    assert(mt_step(three, &value) == MT_DONE && value == NULL && "then exhaustion, told apart from a row");
    mt_answers_free(three);
    assert(closed == 1 && "exhaustion closes the counter once");
```

### [ch06-many-answers/snapshot.c](ch06-many-answers/snapshot.c)

Collected answers are the program's own list, whatever the space does next.

```c
    require("store (item 1)", mt_add(m, E("item", 1)));
    mt_list snapshot = mt_all(mt_atoms(m));
    require("store (item 2)", mt_add(m, E("item", 2)));

    assert((int64_t)snapshot.len == 1 && "the snapshot still holds one row");
    assert(snapshot.len == 1 && alpha_equal(snapshot.items[0], E("item", 1)) && "and it is (item 1)");
    assert((int64_t)mt_count(m) == 2 && "while the space holds two");
```

## Control flow

### [ch07-control-flow/07-02-case/03-caseconstrain.c](ch07-control-flow/07-02-case/03-caseconstrain.c)

case destructures an expression, and C's view of the same expression shares its children.

```c
    assert(answers_are(mt_eval(m, E("case", E(1, 2, 3), E(E(E("cons", V("h"), V("t")), V("h"))))), E(1))
           && "(case (1 2 3) (((cons $h $t) $h)))");

    mt_atom *e = E(1, 2, 3);
    mt_atom *tail = mt_expr_ref(mt_len(e) - 1, mt_children(e) + 1, mt_keep(e), drop_parent);
    assert(atom_is(mt_keep(mt_at(e, 0)), N(1)) && "C's head is the first child");
    assert(atom_is(tail, E(2, 3)) && "and the tail a view over the rest");
```

### [ch07-control-flow/07-05-recursion/01-factorial.c](ch07-control-flow/07-05-recursion/01-factorial.c)

A recursive body written once, over its operators and its own name.

```c
#define FAC(IF, EQ, MUL, SUB, SELF, n) IF(EQ(n, 0), 1, MUL(n, SELF(SUB(n, 1))))
#define T_FACF(n) E("facF", n)

static int64_t facF(int64_t n) { return FAC(C_IF, C_EQ, C_MUL, C_SUB, facF, n); }
...
    require("facF", mt_add(m, E("=", T_FACF(V("n")), FAC(T_IF, T_EQ, T_MUL, T_SUB, T_FACF, V("n")))));
    assert(answers_are(mt_eval(m, E("facF", 10)), E(facF(10))) && "(facF 10)");
```

## Data

### [ch08-data/exact_numbers.c](ch08-data/exact_numbers.c)

Numbers stay exact across the boundary: a BigInt keeps every digit and ratios stay in lowest terms.

```c
    mt_atom *wide = mt_unum(UINT64_MAX);
    assert(mt_kind_of(wide) == MT_BIGINT && "2^64-1 is a BigInt");
    assert(strcmp(mt_name(wide), "18446744073709551615") == 0 && "with every digit");

    mt_atom *ratio = mt_rational(6, -8);
    mt_ratio lowest = mt_ratio_of(ratio);
    assert(lowest.num == -3 && lowest.den == 4 && "6/-8 is stored as -3/4");

    mt_atom *sum = mt_one(mt_eval(m, E("+", mt_keep(ratio), mt_rational(1, 4))));
    mt_ratio exact = mt_ratio_of(sum);
    assert(mt_kind_of(sum) == MT_RATIONAL &&
            exact.num == -1 && exact.den == 2
           && "-3/4 + 1/4 is exactly -1/2");
```

### [ch08-data/array_marshalling.c](ch08-data/array_marshalling.c)

A C array becomes an expression of any length and comes back element for element.

```c
    mt_atom **children = mt_calloc(count, sizeof *children);
    require("allocate the child vector", children != NULL);
    for (size_t i = 0; i < count; i++) children[i] = N(values[i]);
    mt_atom *sequence = mt_exprv(count, children);
    mt_free(children);     /* mt_exprv took the children and copied the vector */

    assert((int64_t)mt_len(sequence) == (int64_t)count && "every element is a child");
    mt_list back = mt_all(mt_eval(m, E("superpose", mt_keep(sequence))));
    bool same = back.len == count;
    for (size_t i = 0; same && i < count; i++) same = mt_int(back.items[i]) == values[i];
    assert(same && mt_ok() && "the engine answers each element, in order");
```

## Types

### [ch09-types/08-parametric_types.c](ch09-types/08-parametric_types.c)

Types are terms: declare one, run the function, and ask the engine for the type back.

```c
    require("(: apply (-> (-> $tx $ty) $tx $ty))", mt_add(m, E(":", "apply", E("->", E("->", V("tx"), V("ty")), V("tx"), V("ty")))));
    require("(= (apply $f $x) ($f $x))", mt_add(m, E("=", E("apply", V("f"), V("x")), E(V("f"), V("x")))));
    assert(answers_are(mt_eval(m, E("apply", "not", B(false))), E(B(!false))) && "apply runs not");
    assert(answers_are(mt_eval(m, E("get-type", E("apply", "not", B(false)))), E(S("Bool"))) && "its type is not's result");
    assert(answers_are(mt_eval(m, E("let", E("get-type", "apply"), E("->", E("->", "Bool", "Bool"), "Bool", V("result")), V("result"))), E(S("Bool")))
           && "$result unifies to Bool");
```

### [ch09-types/annotation_contracts.c](ch09-types/annotation_contracts.c)

A declared type decides whether an argument arrives as written or reduced.

```c
    static const struct { const char *name, *type; } functions[] = {
        { "written-term", "Atom" }, { "reduced-value", "Number" },
    };
    for (size_t i = 0; i < 2; i++) {
        require("publish", mt_def(m, (mt_op){ .name = functions[i].name, .arity = 1,
                                              .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = same }));
        require("declare its type",   /* (: name (-> T T)) */
                mt_add(m, E(":", functions[i].name, E("->", functions[i].type, functions[i].type))));
    }
    assert(answers_are(mt_eval(m, E("written-term", E("+", 20, 22))), E(E("+", 20, 22)))
           && "an Atom argument arrives as written");
    assert(answers_are(mt_eval(m, E("reduced-value", E("+", 20, 22))), E(42))
           && "a Number argument arrives reduced");
```

## Errors and refusals

### [ch10-errors-and-refusals/callback_errors.c](ch10-errors-and-refusals/callback_errors.c)

A C function refuses with its own words, and the next request is served.

```c
static mt_status positive(mt_call *call, void *user)
{
    (void)user;
    mt_clear();
    int64_t value = mt_int(mt_arg(call, 0));
    if (!mt_ok()) return mt_fail(call, "positive expects an integer");
    if (value < 0) return mt_fail(call, "positive refuses negative input");
    return mt_answer(call, N(value));
}
...
    mt_clear();
    mt_atom *refused = mt_first(mt_eval(m, E("positive", -1)));
    assert(refused == NULL && mt_error() == MT_ERROR && "the refusal is an error status");
    assert(mt_errmsg() && strstr(mt_errmsg(), "negative input") != NULL
           && "carrying the function's own words");
    mt_clear();
    assert(mt_one_int(mt_eval(m, E("positive", 42))) == 42 && "the next request is served");
```

### [ch10-errors-and-refusals/error_handling.c](ch10-errors-and-refusals/error_handling.c)

A wrong read records a failure, an empty answer is none, and a false assertion is an engine error.

```c
    mt_clear();
    assert(mt_int(words) == 0 && "a wrong read returns 0");
    assert(mt_error() == MT_MISUSE && mt_errmsg() != NULL && "and records MT_MISUSE with words");
    assert(mt_int(number) == 42 && "a right read returns its value");
    assert(mt_error() == MT_MISUSE && "and leaves the earlier failure standing");
...
    assert(!mt_first(mt_eval(m, S("Empty"))) && mt_ok() && "an empty answer is not a failure");
    assert(mt_ok() && "so nothing was recorded");

    mt_atom *verdict = mt_first(mt_eval(m, E("assertEqual", 1, 2)));
    assert(verdict == NULL && mt_error() == MT_ERROR && "a false assertion is an engine error");
    assert(mt_remedy() != NULL && mt_ground() != NULL && "with the engine's remedy and its ground");
```

## Python as a notation

### [ch11-python-as-a-notation/struct_marshalling.c](ch11-python-as-a-notation/struct_marshalling.c)

A C struct becomes a record atom and fills a struct again.

```c
static mt_atom *person_atom(const person *p)
{
    return E("Person", T(p->name), p->age, B(p->active));
}
...
    size_t found = 0;
    mt_each (row, mt_match(m, E("Person", V("name"), V("age"), V("active")))) {
        person read;
        require("read the record back", person_of(row, &read));
        for (size_t i = 0; i < 2; i++)
            if (strcmp(read.name, staff[i].name) == 0)
                assert(read.age == staff[i].age &&
                       read.active == staff[i].active
                       && "every field round-trips");
        found++;
    }
    assert((int64_t)found == 2 && "both records came back");
```

### [ch11-python-as-a-notation/object_lifetime.c](ch11-python-as-a-notation/object_lifetime.c)

A live C pointer in the space: stored, matched back, typed, and released once.

```c
    int *counter = malloc(sizeof *counter);
    require("allocate the payload", counter != NULL);
    *counter = 42;
    mt_atom *object = mt_object(counter, "Counter", release_payload);
    require("box the payload", object != NULL);

    require("store it", mt_add(m, E("holds", mt_keep(object))));
    mt_atom *row = mt_one(mt_match(m, E("holds", V("x"))));
    assert(row && mt_value(mt_at(row, 1)) == counter && "the same pointer comes back");
    mt_drop(row);
    assert(answers_are(mt_eval(m, E("get-type", mt_keep(object))), E("Counter"))
           && "its type is the name it was boxed under");
    require("remove the fact", mt_del(m, E("holds", mt_keep(object))));
    require("release the object now", mt_object_free(object));
    assert(released == 1 && "the payload is released once");
```

## Testing

### [ch12-testing/property_instances.c](ch12-testing/property_instances.c)

Properties of unification, substitution and hashing checked over a grid of instances.

```c
    mt_atom *diagonal = E("pair", V("x"), V("x"));
    bool unifies_on_diagonal = true, rebuilds = true, hashes_alike = true;
    for (int a = -8; a <= 8; a++)
        for (int b = -8; b <= 8; b++) {
            mt_atom *pair = E("pair", a, b);
            mt_bindings *bindings = mt_unify(diagonal, pair);
            unifies_on_diagonal &= (bindings != NULL) == (a == b);
            if (bindings) {
                mt_atom *instance = mt_substitute(diagonal, bindings);
                rebuilds &= mt_eq(instance, pair);
                hashes_alike &= mt_hash(instance) == mt_hash(pair);
                mt_drop(instance);
                mt_bindings_free(bindings);
            }
            mt_drop(pair);
        }
    assert(unifies_on_diagonal && "(pair $x $x) unifies exactly on the diagonal");
    assert(rebuilds && "the substitution rebuilds the instance");
    assert(hashes_alike && "equal atoms hash alike");
```

## Seeing your program

### [ch14-seeing-your-program/engine_controls.c](ch14-seeing-your-program/engine_controls.c)

Bound an endless query by inferences, and price a finite one.

```c
    /* (= (from $n) (superpose ($n (from (+ $n 1))))) */
    require("define an endless generator", mt_add(m, E("=", E("from", V("n")),
        E("superpose", E(V("n"), E("from", E("+", V("n"), 1)))))));
    require("bound every cursor", mt_limit(m, (mt_limits){ .inferences = 5000 }));
    mt_answers *answers = mt_eval(m, E("from", 0));
    require("open the cursor", answers != NULL);
    const mt_atom *answer;
    mt_status status;
    size_t taken = 0;
    while ((status = mt_step(answers, &answer)) == MT_ROW) taken++;
    assert(status == MT_LIMIT && mt_error() == MT_LIMIT && "the bound stops it, and says so");
    assert(taken > 0 && "after some answers");
...
    mt_stats before = mt_stats_now(m);
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "a finite question");
    mt_stats spent = mt_stats_since(before, mt_stats_now(m));
    assert(spent.inferences > 0 && "is priced in inferences");
```

## Writing transactions and worlds

### [ch15-writing-transactions-and-worlds/transactions.c](ch15-writing-transactions-and-worlds/transactions.c)

One callback committed, rolled back and speculated.

```c
static mt_status write_pair(metta *m, void *user)
{
    (void)m;
    work *w = user;
    if (!mt_add(w->space, E("edge", 1, 2)) || !mt_add(w->space, E("edge", 2, 3)))
        return mt_error();
    return w->verdict;
}
...
    work w = { mt_self(m), MT_FAIL };
    assert(mt_transaction(m, write_pair, &w) == MT_FAIL && "MT_FAIL rolls back");
    assert((int64_t)mt_count(m) == 0 && "and nothing was written");
    w.verdict = MT_OK;
    assert(mt_speculate(m, write_pair, &w) == MT_OK && "speculation succeeds");
    assert((int64_t)mt_count(m) == 0 && "and discards its writes anyway");
    assert(mt_transaction(m, write_pair, &w) == MT_OK && "MT_OK commits");
    assert(answers_are(mt_atoms(m), E(E("edge", 1, 2), E("edge", 2, 3))) && "both edges are published together");
```

## Events and standing queries

### [ch16-events-and-standing-queries/standing_queries.c](ch16-events-and-standing-queries/standing_queries.c)

A subscription hears committed writes and removals, and nothing speculative.

```c
static mt_status changed(void *user, bool added, const mt_atom *item)
{
    seen *s = user;
    if (!alpha_equal(item, E("item", 7))) s->other = true;
    if (added) s->adds++;
    else s->removes++;
    return MT_OK;
}
...
    seen s = {0};
    require("watch items", mt_subscribe(m, "items", (mt_subscription){
        .space = "&self", .pattern = E("item", V("x")), .notify = changed, .user = &s }));

    require("speculate a write", mt_speculate(m, insert, NULL) == MT_OK);
    assert(s.adds == 0 && mt_count(m) == 0
           && "a speculative write raises no event and leaves nothing");
    require("commit the write", mt_transaction(m, insert, NULL) == MT_OK);
    assert(s.adds == 1 && "a committed write raises one event");
    require("remove the item", mt_del(m, E("item", 7)));
    assert(s.removes == 1 && "a removal raises one");

    require("stop watching", mt_unsubscribe(m, "items"));
    require("write again", mt_add(m, E("item", 7)));
    assert(s.adds == 1 && !s.other && "nothing is delivered after unsubscribing");
```

## Concurrency and the loop

### [ch17-concurrency-and-the-loop/threads.c](ch17-concurrency-and-the-loop/threads.c)

C threads attach to one engine, and each keeps its own error state.

```c
static void *work(void *user)
{
    job *j = user;
    if (!mt_thread_attach()) return NULL;
    mt_clear();
    j->output = mt_one_int(mt_eval(j->runtime, E("+", j->input, 10)));
    if (j->input % 2) {                    /* odd workers make a mistake of their own */
        mt_atom *word = S("not-a-number");
        (void)mt_int(word);
        mt_drop(word);
        j->own_error = mt_error() == MT_MISUSE;
    } else {
        j->own_error = mt_ok();
    }
    mt_thread_detach();
    return NULL;
}
...
    for (int64_t i = 0; i < 4; i++) {
        jobs[i] = (job){ .runtime = m, .input = i + 1 };
        require("start a worker", pthread_create(&threads[i], NULL, work, &jobs[i]) == 0);
    }
    for (size_t i = 0; i < 4; i++) require("join a worker", pthread_join(threads[i], NULL) == 0);
    for (size_t i = 0; i < 4; i++) {
        assert(jobs[i].output == jobs[i].input + 10 && "each worker's answer");
        assert(jobs[i].own_error && "each worker saw only its own error state");
    }
    assert(mt_ok() && "no worker's error reached the main thread");
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "and the main thread still answers");
```

## Performance

### [ch18-performance/18-03-algebra-carriers/family_algebras.c](ch18-performance/18-03-algebra-carriers/family_algebras.c)

One relation asked in every direction under every algebra carrier.

```c
    struct { const char *carrier; mt_atom *identity; } algebras[] = {
        { "counting", N(1) }, { "tropical", N(0) }, { "prov", S("one") },
        { "ranked", N(1) },   { "prob", N(1) },
    };
    struct { mt_atom *goal; size_t answers; } directions[] = {
        { E("ancestor", "Tom", "Ann"), 1 },     /* is it so? */
        { E("ancestor", V("x"), "Ann"), 2 },    /* who are Ann's ancestors? */
        { E("ancestor", "Tom", V("y")), 2 },    /* whose ancestor is Tom? */
        { E("ancestor", V("x"), V("y")), 3 },   /* every pair */
    };
    for (size_t a = 0; a < 5; a++) {
        for (size_t d = 0; d < 4; d++) {
            mt_list rows = mt_all(mt_eval_under(m, S(algebras[a].carrier), mt_keep(directions[d].goal)));
            assert(mt_ok() && rows.len == directions[d].answers && "each direction keeps its answer count");
            for (size_t i = 0; i < rows.len; i++)
                assert(mt_len(rows.items[i]) == 2 &&
                       alpha_equal(mt_at(rows.items[i], 0), B(true)) &&
                       mt_alpha_eq(mt_at(rows.items[i], 1), algebras[a].identity)
                       && "each answer is True with the carrier's identity");
```

## Spaces backed by anything

### [ch19-spaces-backed-by-anything/19-01-spaces-of-your-own/sqlite_space.c](ch19-spaces-backed-by-anything/19-01-spaces-of-your-own/sqlite_space.c)

A space whose atoms are SQLite rows: joins, duplicates, rollback, and a cursor that outlives its provider.

```c
    sql_open(m, "&sql", ":memory:", &released);
    mt_space *sql = mt_space_open(m, "&sql");
    require("open &sql", sql != NULL);
    require("a row", mt_add(sql, E("edge", "a", "b")));
    require("a loop", mt_add(sql, E("edge", "b", "b")));
    require("a duplicate row", mt_add(sql, E("edge", "a", "b")));

    assert(answers_are(mt_eval(sql, E("match", mt_spaceref("&sql"), E("edge", V("x"), V("x")), V("x"))), E("b"))
           && "a repeated variable joins on equal fields");
    assert(answers_are(mt_match(sql, E("edge", "a", "b")), E(E("edge", "a", "b"), E("edge", "a", "b")))
           && "duplicates stay duplicates");
    assert(mt_transaction(m, abandoned, sql) == MT_FAIL && "a failed transaction reports MT_FAIL");
    assert(!mt_first(mt_match(sql, E("edge", "a", "z"))) && mt_ok() && "and its row was rolled back");
...
    mt_answers *rows = mt_atoms(sql);
    require("hold a cursor open", rows != NULL && mt_next(rows) != NULL);
    require("withdraw the provider", mt_provider_close(m, "&sql"));
    assert((int64_t)released == 0 && "the open cursor keeps the connection");
    mt_answers_free(rows);
    assert((int64_t)released == 1 && "closing it releases the connection");
```

### [ch19-spaces-backed-by-anything/19-03-a-builtin-in-c/01-c_extension.c](ch19-spaces-backed-by-anything/19-03-a-builtin-in-c/01-c_extension.c)

A builtin written in C is a C function the engine calls.

```c
#define BUMP(ADD, x) ADD(x, 1)

static mt_status c_bump(mt_call *call, void *user)
{
    (void)user;
    if (mt_kind_of(mt_arg(call, 0)) != MT_INT) return MT_FAIL;
    return mt_answer(call, N(BUMP(C_ADD, mt_int(mt_arg(call, 0)))));
}
...
    require("c-bump", mt_def(m, (mt_op){ .name = "c-bump", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = c_bump }));
    assert(answers_are(mt_eval(m, E("c-bump", 41)), E(BUMP(C_ADD, 41))) && "(c-bump 41) is C's");
```

## Extending the engine

### [ch20-extending-the-engine/20-01-translator-rules/01-translatorrule.c](ch20-extending-the-engine/20-01-translator-rules/01-translatorrule.c)

A translator rule compiles a definition, and every spelling answers what C's cons builds.

```c
    for (size_t i = 0; i < DEFINITIONS; i++) {
        mt_atom *body = E("cons", HEAD, V("arg"));
        require(definitions[i].name,
                mt_add(m, E("=", E(definitions[i].name, V("arg")), definitions[i].noeval ? E("noeval", body) : body)));
    }
    for (size_t i = 0; i < DEFINITIONS; i++)
        if (definitions[i].rule)
            require("register its rule", mt_one_truth(mt_eval(m, E("add-translator-rule!", definitions[i].name))));

    mt_atom *list = E(43);
    for (size_t i = 0; i < DEFINITIONS; i++)
        assert(answers_are(mt_eval(m, E(definitions[i].name, mt_keep(list))), E(cons(HEAD, list))) && definitions[i].name);
```

### [ch20-extending-the-engine/20-04-modules-and-the-catalog/registration_lifecycle.c](ch20-extending-the-engine/20-04-modules-and-the-catalog/registration_lifecycle.c)

Register how a C value prints through the seam, then withdraw the registration.

```c
    char *value = strdup("<C value>");
    require("own a value", value != NULL);
    mt_atom *object = mt_object(value, "ExampleValue", free);
    require("box it", object != NULL);
    require("register how it prints", mt_repr(m, "ExampleValue", display, NULL));
    assert(strcmp(mt_show(object), "<C value>") == 0 && "the object prints through the registration");
    assert(answers_are(mt_eval(m, E("get-type", mt_keep(object))), E("ExampleValue"))
           && "and its type is the name it was boxed under");

    require("withdraw the registration", mt_unregister(m, "repr", "ExampleValue"));
    mt_clear();
    assert(!mt_unregister(m, "repr", "ExampleValue") && mt_ok()
           && "withdrawing twice finds nothing, without an error");
    assert(strcmp(mt_value(object), "<C value>") == 0 && "the value itself lives on");
```

## A reasoner you can serve

### [ch22-a-reasoner-you-can-serve/22-02-weighted-answers/pln_uncertain_reasoning.c](ch22-a-reasoner-you-can-serve/22-02-weighted-answers/pln_uncertain_reasoning.c)

Call a PLN truth function and read its strength and confidence in C.

```c
    require("import lib_pln",
            mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_pln")))));
    mt_atom *truth = mt_one(mt_eval(m, E("Truth_ModusPonens", E("stv", 1.0, 0.95), E("stv", 0.6, 0.9))));
    require("a truth value", truth != NULL && mt_len(truth) == 3);
    assert(strcmp(mt_name(mt_at(truth, 0)), "stv") == 0 && "it is an stv");
    double strength = mt_float(mt_at(truth, 1)), confidence = mt_float(mt_at(truth, 2));
    assert(strength == 0.6 && "the strength is the premise's");
    assert(confidence > 0.0 && confidence < 1.0 && "and confidence stays uncertain");
```

### [ch22-a-reasoner-you-can-serve/22-02-weighted-answers/uncertain_perception.c](ch22-a-reasoner-you-can-serve/22-02-weighted-answers/uncertain_perception.c)

An exact rule sharpens two uncertain sensors.

```c
    require("the sum rule", mt_add(m, E("=", E("consistent", V("sum")),
        E("match", "&self", E(",", E("sees", "a", V("a"), V("wa")), E("sees", "b", V("b"), V("wb"))),
          E("if", E("==", E("+", V("a"), V("b")), V("sum")),
            E("Hypothesis", V("a"), E("*", V("wa"), V("wb"))), "Empty")))));

    mt_list hypotheses = mt_all(mt_eval(m, E("consistent", 3)));
    assert((int64_t)hypotheses.len == 2 && "two readings are consistent with the rule");
    double mass = 0.0, right = 0.0;
    for (size_t i = 0; i < hypotheses.len; i++) {
        double weight = mt_float(mt_at(hypotheses.items[i], 2));
        mass += weight;
        if (mt_int(mt_at(hypotheses.items[i], 1)) == 1) right += weight;
    }
    assert(fabs(mass - 0.505) < 1e-12 && "their joint mass is 0.505");
    assert(right / mass > 0.9 && right / mass > scores[0][1]
           && "and the right reading carries over 90% of it, more than either sensor");
```

A numbered program is the C twin of the MeTTa program at the same path in [the MeTTa corpus](https://github.com/MesTTo/MeTTa-Examples).

[cmetta](https://github.com/MesTTo/CMeTTa)'s `make corpus-check` runs every program here against its own build and each twin beside its MeTTa original, and builds every program on its own against an installed cmetta.
