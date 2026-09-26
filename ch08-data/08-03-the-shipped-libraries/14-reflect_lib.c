/* Purpose: lib_reflect, the engine's surface and literal code as data. What
 *   the engine has registered only the engine knows, so C does what it can
 *   on its own with what the engine answers: it collects each enumeration
 *   once and answers membership, inclusion and size with its own set code;
 *   it knows the heads it defined itself; it reads a registered arity as the
 *   arguments it passes plus the answer position, the rule the original
 *   states; it counts the functions as the union of builtins and its own
 *   functions; and it reads surface-json with cJSON, each array the size of
 *   its list. Code as data is C's own work: atom-replace is a C walk that
 *   looks each rule's left side up exactly, replaces the topmost match with
 *   every right side in turn and multiplies the alternatives of an
 *   expression's children, first child slowest, and atom-variables collects
 *   variables by first occurrence. The strategies are the identities the
 *   original names: an empty composition keeps its term, an empty choice
 *   has none, a numeric repeat answers its term that many times.
 * Build: cc 14-reflect_lib.c $(pkg-config --cflags --libs cmetta libcjson)
 * Assumes: libcjson, found through pkg-config.
 * Guarantees: all fifty-nine claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#if __has_include(<cjson/cJSON.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

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

#define COUNT(array) (sizeof (array) / sizeof *(array))
enum { MOST = 16 };

/* ---- the surface, read once and asked by C ------------------------------ */

static mt_list enumeration(metta *m, const char *head)
{
    mt_list names = mt_all(mt_eval(m, E(head)));
    require(head, mt_ok() && names.len > 0);
    return names;
}

static bool member(mt_list list, const mt_atom *x)
{
    for (size_t i = 0; i < list.len; i++)
        if (mt_eq(list.items[i], x)) return true;
    return false;
}

static bool member_named(mt_list list, const char *name)
{
    mt_atom *x = mt_sym(name);
    bool found = member(list, x);
    mt_drop(x);
    return found;
}

/* |a ∪ b|, duplicates across the two counted once. */
static int64_t union_size(mt_list a, mt_list b)
{
    int64_t n = (int64_t)a.len;
    for (size_t i = 0; i < b.len; i++) n += !member(a, b.items[i]);
    return n;
}

/* The heads this program defines itself, the only surface C knows first hand. */
static const char *const DEFINED[] = { "mine" };

static bool defined_here(const char *name)
{
    for (size_t i = 0; i < COUNT(DEFINED); i++)
        if (strcmp(DEFINED[i], name) == 0) return true;
    return false;
}

/* A function registered at the arguments it takes plus the answer position:
   C passes car-atom one argument and cons-atom two. */
static mt_atom *arity_of_call(size_t arguments) { return mt_num((int64_t)arguments + 1); }

/* ---- code as data --------------------------------------------------------- */

typedef struct answers { size_t n; mt_atom *item[MOST]; } answers;

static void push(answers *a, mt_atom *x)
{
    require("room for the alternative", a->n < MOST);
    a->item[a->n++] = x;
}

static void release(answers *a)
{
    for (size_t i = 0; i < a->n; i++) mt_drop(a->item[i]);
    a->n = 0;
}

/* The alternatives atom-replace gives a term: every rule whose left side is
   the term exactly gives its right side, and the term's inside is then not
   looked at; otherwise an expression's alternatives are the product of its
   children's, the first child slowest, and anything else is itself. */
static void replacements(const mt_atom *term, const mt_atom *rules, answers *out)
{
    bool hit = false;
    for (size_t i = 0; i < mt_len(rules); i++)
        if (mt_eq(mt_at(mt_at(rules, i), 0), term)) {
            push(out, mt_keep(mt_at(mt_at(rules, i), 1)));
            hit = true;
        }
    if (hit) return;
    size_t n = mt_len(term);
    if (mt_kind_of(term) != MT_EXPR || n == 0) {
        push(out, mt_keep(term));
        return;
    }
    answers kids[MOST];
    size_t wheel[MOST] = { 0 };
    require("room for the children", n <= MOST);
    for (size_t i = 0; i < n; i++) {
        kids[i].n = 0;
        replacements(mt_at(term, i), rules, &kids[i]);
    }
    for (;;) {
        mt_atom *chosen[MOST];
        for (size_t i = 0; i < n; i++) chosen[i] = mt_keep(kids[i].item[wheel[i]]);
        push(out, mt_exprv(n, chosen));
        size_t i = n;
        while (i > 0 && ++wheel[i - 1] == kids[i - 1].n) wheel[--i] = 0;
        if (i == 0) break;
    }
    for (size_t i = 0; i < n; i++) release(&kids[i]);
}

static mt_atom *replaced_all(const mt_atom *term, const mt_atom *rules)
{
    answers out = { 0 };
    replacements(term, rules, &out);
    return mt_exprv(out.n, out.item);
}

static mt_atom *replaced_one(const mt_atom *term, const mt_atom *rules)
{
    answers out = { 0 };
    replacements(term, rules, &out);
    require("one alternative", out.n == 1);
    return out.item[0];
}

static void collect_variables(const mt_atom *t, answers *seen)
{
    if (mt_kind_of(t) == MT_VARIABLE) {
        for (size_t i = 0; i < seen->n; i++)
            if (mt_eq(seen->item[i], t)) return;
        push(seen, mt_keep(t));
        return;
    }
    for (size_t i = 0; i < mt_len(t); i++) collect_variables(mt_at(t, i), seen);
}

static mt_atom *variables_of(const mt_atom *t)
{
    answers seen = { 0 };
    collect_variables(t, &seen);
    return mt_exprv(seen.n, seen.item);
}

/* Whether two atoms C computed agree, both released. */
static bool same(mt_atom *a, mt_atom *b)
{
    bool equal = mt_eq(a, b);
    mt_drop(a);
    mt_drop(b);
    return equal;
}

/* Each rule is a (from to) pair. */
static bool well_formed(const mt_atom *rules)
{
    for (size_t i = 0; i < mt_len(rules); i++)
        if (mt_len(mt_at(rules, i)) != 2) return false;
    return true;
}

static mt_atom *verdict(bool holds) { return mt_sym(holds ? "accepted" : "refused"); }

static mt_atom *repeated(size_t n, mt_atom *x)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = mt_keep(x);
    mt_drop(x);
    return mt_exprv(n, kids);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_reflect", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_reflect")))));
    require("mine", mt_add(m, E("=", E("mine", V("x")), V("x"))));
    static const char *const RULES[][2] = { { "a", "b" }, { "a", "c" } };
    for (size_t i = 0; i < COUNT(RULES); i++)
        require("a reflect-rule", mt_add(m, E("reflect-rule", mt_sym(RULES[i][0]), mt_sym(RULES[i][1]))));

    mt_list builtins = enumeration(m, "builtins"), special = enumeration(m, "special-forms"),
            functions = enumeration(m, "functions"), user = enumeration(m, "user-functions"),
            points = enumeration(m, "extension-points");

    assert(answers_are(mt_eval(m, E("knows?", "car-atom")), E(B(member_named(functions, "car-atom")))) && "knows? a builtin");
    assert(answers_are(mt_eval(m, E("knows?", "mine")), E(B(defined_here("mine")))) && "knows? a head defined here");
    assert(answers_are(mt_eval(m, E("knows?", "no-such-name-anywhere")), E(B(member_named(functions, "no-such-name-anywhere"))))
           && "knows? nothing else");
    assert(answers_are(mt_eval(m, E("collapse", E("arity-of", "car-atom"))), E(E(arity_of_call(1)))) && "car-atom's registered arity");
    assert(answers_are(mt_eval(m, E("collapse", E("arity-of", "cons-atom"))), E(E(arity_of_call(2)))) && "cons-atom's");
    assert(answers_are(mt_eval(m, E("collapse", E("arity-of", "no-such-name-anywhere"))), E(mt_unit())) && "an unknown name has none");
    assert(answers_are(mt_eval(m, E("once", E("car-atom", E("origin-of", "car-atom")))), E(mt_sym("origin"))) && "an origin row");
    assert(answers_are(mt_eval(m, E("car-atom", E("origin-of", "mine"))), E(mt_sym("origin"))) && "for a head defined here too");
    assert(answers_are(mt_eval(m, E("collapse", E("origin-of", "no-such-name-anywhere"))), E(mt_unit())) && "an unknown name has no origin");

    assert(answers_are(mt_eval(m, E(">", E("size-atom", E("collapse", E("builtins"))), 100)), E(B(builtins.len > 100)))
           && "more than a hundred builtins");
    assert(answers_are(mt_eval(m, E(">", E("size-atom", E("collapse", E("special-forms"))), 10)), E(B(special.len > 10)))
           && "more than ten special forms");
    assert(answers_are(mt_eval(m, E("is-member", "mine", E("collapse", E("user-functions")))), E(B(defined_here("mine"))))
           && "mine is a user function");
    assert(answers_are(mt_eval(m, E("is-member", "car-atom", E("collapse", E("user-functions")))), E(B(defined_here("car-atom"))))
           && "car-atom is not");
    assert(answers_are(mt_eval(m, E("is-member", "car-atom", E("collapse", E("functions")))), E(B(member_named(builtins, "car-atom") || member_named(user, "car-atom"))))
           && "car-atom is a function");
    assert(answers_are(mt_eval(m, E("is-member", "case", E("collapse", E("builtins")))), E(B(member_named(builtins, "case"))))
           && "case is not a builtin");
    assert(answers_are(mt_eval(m, E("is-member", "case", E("collapse", E("special-forms")))), E(B(member_named(special, "case"))))
           && "case is a special form");
    assert(answers_are(mt_eval(m, E("is-member", "let", E("collapse", E("builtins")))), E(B(member_named(builtins, "let"))))
           && "let is a builtin");
    assert(answers_are(mt_eval(m, E("is-member", "let", E("collapse", E("special-forms")))), E(B(member_named(special, "let"))))
           && "and a special form");
    assert(answers_are(mt_eval(m, E(">", E("size-atom", E("collapse", E("extension-points"))), 3)), E(B(points.len > 3)))
           && "more than three extension points");
    mt_atom *foreign = E("foreign_space", 1, "ownership");
    assert(answers_are(mt_eval(m, E("is-member", mt_keep(foreign), E("collapse", E("extension-points")))), E(B(member(points, foreign))))
           && "foreign_space is one");
    mt_drop(foreign);

    /* The counts: the four enumerations C read, the functions their union. */
    const int64_t counted[] = { (int64_t)builtins.len, (int64_t)special.len, union_size(builtins, user), (int64_t)user.len };
    static const char *const KINDS[] = { "builtins", "special-forms", "functions", "user-functions" };
    assert(answers_are(mt_eval(m, E("size-atom", E("surface-counts"))), E((int64_t)COUNT(KINDS))) && "four counts");
    assert(answers_are(mt_eval(m, E("let", V("counts"), E("surface-counts"), E("index-atom", E("car-atom", V("counts")), 0))), E(mt_sym(KINDS[0])))
           && "builtins first");
    mt_atom *pairs[COUNT(KINDS)];
    for (size_t i = 0; i < COUNT(KINDS); i++) pairs[i] = E(mt_sym(KINDS[i]), counted[i]);
    assert(answers_are(mt_eval(m, E("surface-counts")), E(mt_exprv(COUNT(KINDS), pairs))) && "each count is what C counted");
    require("functions are everything else", (size_t)counted[2] == functions.len);

    mt_atom *json = mt_one(mt_eval(m, E("surface-json")));
    require("surface-json answers text", json && mt_kind_of(json) == MT_TEXT);
    const char *text = mt_name(json);
    assert(answers_are(mt_eval(m, E(">", E("string-length", E("surface-json")), 1000)), E(B(strlen(text) > 1000))) && "the JSON is long");
    assert(answers_are(mt_eval(m, E("string-starts-with", E("surface-json"), T("{"))), E(B(text[0] == '{'))) && "and an object");
    cJSON *surface = cJSON_Parse(text);
    require("cJSON reads it", surface != NULL);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(surface, "builtinNames")) == (int)builtins.len &&
           cJSON_GetArraySize(cJSON_GetObjectItem(surface, "specialFormNames")) == (int)special.len &&
           cJSON_GetArraySize(cJSON_GetObjectItem(surface, "userFunctionNames")) == (int)user.len
           && "cJSON reads each list at its size");
    cJSON_Delete(surface);
    mt_drop(json);

    /* The Prolog rungs answer what the accessors answer. */
    assert(answers_are(mt_eval(m, E("==", E("collapse", E("engine-arity", "car-atom")), E("collapse", E("arity-of", "car-atom")))), E(B(true)))
           && "engine-arity");
    assert(answers_are(mt_eval(m, E("engine-knows", "car-atom")), E(B(member_named(functions, "car-atom")))) && "engine-knows");
    assert(answers_are(mt_eval(m, E("engine-knows", "no-such-name-anywhere")), E(B(member_named(functions, "no-such-name-anywhere"))))
           && "engine-knows nothing else");
    assert(answers_are(mt_eval(m, E("collapse", E("engine-origin", "car-atom"))), E(member_named(builtins, "car-atom") ? E(E("builtin")) : mt_unit()))
           && "engine-origin");
    mt_atom *counts = mt_one(mt_eval(m, E("surface-counts"))), *engine_counts = mt_one(mt_eval(m, E("engine-surface-counts")));
    assert(answers_are(mt_eval(m, E("==", E("engine-surface-counts"), E("surface-counts"))), E(B(same(counts, engine_counts))))
           && "engine-surface-counts");
    static const char *const RUNGS[][2] = { { "engine-builtin", "builtins" }, { "engine-special-form", "special-forms" },
                                            { "engine-function", "functions" }, { "engine-user-function", "user-functions" },
                                            { "engine-extension-point", "extension-points" } };
    for (size_t i = 0; i < COUNT(RUNGS); i++) {
        mt_list rung = mt_all(mt_eval(m, E(RUNGS[i][0]))), accessor = mt_all(mt_eval(m, E(RUNGS[i][1])));
        assert(answers_are(mt_eval(m, E("==", E("size-atom", E("collapse", E(RUNGS[i][0]))),
                                       E("size-atom", E("collapse", E(RUNGS[i][1]))))), E(B(rung.len == accessor.len)))
               && RUNGS[i][0]);
        mt_list_free(rung);
        mt_list_free(accessor);
    }

    /* Code as data. */
    struct { const char *claim; mt_atom *term, *rules; bool all; } cases[] = {
        { "a literal sum stays literal", E("+", 1, 2), E(E(1, 10)), false },
        { "the topmost match wins", E("f", "a"), E(E(E("f", "a"), "root"), E("a", "child")), false },
        { "one pass, not a chain", E("a", "a"), E(E("a", "b"), E("b", "c")), false },
        { "every alternative, first child slowest", E("a", "a"), E(E("a", "b"), E("a", "c")), true },
        { "repeated rules repeat", mt_sym("a"), E(E("a", "b"), E("a", "b")), true },
        { "a head and an empty child", E("f", mt_unit()), E(E("f", "g"), E(mt_unit(), "empty-value")), false },
        { "the empty expression itself", mt_unit(), E(E(mt_unit(), E("f", "a"))), false },
        { "no rules, no change", E("+", 1, 2), mt_unit(), false },
        { "an Error stays data", mt_sym("a"), E(E("a", E("Error", "data", "code"))), false },
        { "so does Empty", mt_sym("a"), E(E("a", "Empty")), false },
        { "Empty among the alternatives", E("a", "a"), E(E("a", "Empty"), E("a", "b")), true },
        { "1 and 1.0 are different atoms", E(1, 1.0), E(E(1, "integer"), E(1.0, "float")), false },
    };
    for (size_t i = 0; i < COUNT(cases); i++) {
        mt_atom *goal = E("atom-replace", mt_keep(cases[i].term), mt_keep(cases[i].rules));
        if (cases[i].all)
            assert(answers_are(mt_eval(m, E("collapse", goal)), E(replaced_all(cases[i].term, cases[i].rules))) && cases[i].claim);
        else
            assert(value_is(mt_eval(m, goal), replaced_one(cases[i].term, cases[i].rules)) && cases[i].claim);
        mt_drop(cases[i].term);
        mt_drop(cases[i].rules);
    }

    /* Variables compare by identity inside one expression, as the original
       compares them, so an alpha-renamed answer cannot pass. */
    mt_atom *spread = E(V("x"), E(V("y"), V("x")), V("z"), V("y")), *xyz = E(V("x"), V("y"), V("z"));
    assert(answers_are(mt_eval(m, E("let", V("vars"), E("atom-variables", mt_keep(spread)), E("==", V("vars"), E("quote", mt_keep(xyz))))), E(B(same(variables_of(spread), mt_keep(xyz)))))
           && "every variable, first occurrence first");
    mt_atom *binder = E("let", V("x"), V("y"), E("f", V("x"), V("z")));
    assert(answers_are(mt_eval(m, E("let", V("vars"), E("atom-variables", mt_keep(binder)), E("==", V("vars"), E("quote", mt_keep(xyz))))), E(B(same(variables_of(binder), mt_keep(xyz)))))
           && "binder positions count");
    mt_atom *sum = E("+", 1, 2);
    assert(answers_are(mt_eval(m, E("atom-variables", mt_keep(sum))), E(variables_of(sum))) && "a ground term has none");
    mt_atom *xyx = E(V("x"), V("y"), V("x")), *x_to_z = E(E(V("x"), V("z")));
    assert(answers_are(mt_eval(m, E("let", V("changed"), E("atom-replace", mt_keep(xyx), mt_keep(x_to_z)),
                                    E("==", V("changed"), E("quote", E(V("z"), V("y"), V("z")))))), E(B(same(replaced_one(xyx, x_to_z), E(V("z"), V("y"), V("z"))))))
           && "a variable replaced everywhere it occurs");
    mt_atom *px = E("p", V("x")), *py_rule = E(E(E("p", V("y")), "wrong"));
    assert(answers_are(mt_eval(m, E("let", V("kept"), E("atom-replace", mt_keep(px), mt_keep(py_rule)),
                                    E("==", V("kept"), E("quote", mt_keep(px))))), E(B(same(replaced_one(px, py_rule), mt_keep(px)))))
           && "and only that variable");

    /* Rules from a space. */
    mt_atom *rules[COUNT(RULES)], *ha = E("h", "a");
    for (size_t i = 0; i < COUNT(RULES); i++) rules[i] = E(mt_sym(RULES[i][0]), mt_sym(RULES[i][1]));
    mt_atom *table = mt_exprv(COUNT(RULES), rules);
    assert(answers_are(mt_eval(m, E("let", V("rules"), E("collapse", E("match", "&self", E("reflect-rule", V("from"), V("to")),
                                                                       E("quote", E(V("from"), V("to"))))),
                                    E("collapse", E("atom-replace", mt_keep(ha), V("rules"))))), E(replaced_all(ha, table)))
           && "rules matched out of a space");

    /* Strategies: the identities, and a numeric repeat. */
    assert(answers_are(mt_eval(m, E("alltd", "fail", mt_keep(sum))), E(mt_keep(sum))) && "alltd of fail keeps its term");
    assert(answers_are(mt_eval(m, E("seq", mt_keep(sum))), E(mt_keep(sum))) && "an empty sequence keeps it");
    assert(answers_are(mt_eval(m, E("seq", "id", "id", "id", mt_keep(sum))), E(mt_keep(sum))) && "so do identities");
    assert(!mt_first(mt_eval(m, E("choice", mt_keep(sum)))) && mt_ok() && "an empty choice has none");
    assert(answers_are(mt_eval(m, E("collapse", E("repeat", 3, E("noeval", "token")))), E(repeated(3, mt_sym("token")))) && "repeat three times");
    mt_atom *malformed = E(E("a"));
    assert(answers_are(mt_eval(m, E("if-error", E("catch", E("atom-replace", "a", mt_keep(malformed))), "refused", "accepted")), E(verdict(well_formed(malformed))))
           && "a rule that is no pair is refused");

    /* The library's own equation, reconstructed and applied. */
    mt_atom *xx = E(V("x"), V("x"));
    assert(answers_are(mt_eval(m, E("let", V("source"),
                                    E("match", "&self", E("=", E("atom-variables", V("term")), V("body")),
                                      E("quote", E("|->", E(V("term")), V("body")))),
                                    E("let", V("inspect"), E("eval", V("source")),
                                      E("let", V("vars"), E(V("inspect"), E("quote", mt_keep(xx))), E("==", V("vars"), E("quote", E(V("x")))))))), E(B(same(variables_of(xx), E(V("x"))))))
           && "atom-variables' own equation, as a function");

    mt_drop(spread); mt_drop(xyz); mt_drop(binder); mt_drop(sum); mt_drop(xyx); mt_drop(x_to_z);
    mt_drop(px); mt_drop(py_rule); mt_drop(ha); mt_drop(table); mt_drop(malformed); mt_drop(xx);
    mt_list_free(builtins); mt_list_free(special); mt_list_free(functions); mt_list_free(user); mt_list_free(points);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without cJSON's headers the program only says what it needs. */
int main(void)
{
    fputs("14-reflect_lib.c needs cJSON: install its development files, then build with\n"
          "cc 14-reflect_lib.c $(pkg-config --cflags --libs cmetta libcjson)\n", stderr);
    return 77;
}
#endif
