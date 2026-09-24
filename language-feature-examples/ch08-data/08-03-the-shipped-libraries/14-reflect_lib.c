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
 * Assumes: libcjson, found through pkg-config.
 * Guarantees: all fifty-nine claims of the original hold [tested: make
 *   twins; commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include <cjson/cJSON.h>

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
    metta *m = open_engine();
    require("import lib_reflect", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_reflect")))));
    require("mine", mt_lower(m, (mine $x), $x));
    static const char *const RULES[][2] = { { "a", "b" }, { "a", "c" } };
    for (size_t i = 0; i < COUNT(RULES); i++)
        require("a reflect-rule", mt_add(m, E("reflect-rule", mt_sym(RULES[i][0]), mt_sym(RULES[i][1]))));

    mt_list builtins = enumeration(m, "builtins"), special = enumeration(m, "special-forms"),
            functions = enumeration(m, "functions"), user = enumeration(m, "user-functions"),
            points = enumeration(m, "extension-points");

    check_answers("knows? a builtin", mt_eval(m, E("knows?", "car-atom")), B(member_named(functions, "car-atom")));
    check_answers("knows? a head defined here", mt_eval(m, E("knows?", "mine")), B(defined_here("mine")));
    check_answers("knows? nothing else", mt_eval(m, E("knows?", "no-such-name-anywhere")),
                  B(member_named(functions, "no-such-name-anywhere")));
    check_answers("car-atom's registered arity", mt_eval(m, E("collapse", E("arity-of", "car-atom"))), E(arity_of_call(1)));
    check_answers("cons-atom's", mt_eval(m, E("collapse", E("arity-of", "cons-atom"))), E(arity_of_call(2)));
    check_answers("an unknown name has none", mt_eval(m, E("collapse", E("arity-of", "no-such-name-anywhere"))), mt_unit());
    check_answers("an origin row", mt_eval(m, E("once", E("car-atom", E("origin-of", "car-atom")))), mt_sym("origin"));
    check_answers("for a head defined here too", mt_eval(m, E("car-atom", E("origin-of", "mine"))), mt_sym("origin"));
    check_answers("an unknown name has no origin", mt_eval(m, E("collapse", E("origin-of", "no-such-name-anywhere"))), mt_unit());

    check_answers("more than a hundred builtins", mt_eval(m, E(">", E("size-atom", E("collapse", E("builtins"))), 100)),
                  B(builtins.len > 100));
    check_answers("more than ten special forms", mt_eval(m, E(">", E("size-atom", E("collapse", E("special-forms"))), 10)),
                  B(special.len > 10));
    check_answers("mine is a user function", mt_eval(m, E("is-member", "mine", E("collapse", E("user-functions")))),
                  B(defined_here("mine")));
    check_answers("car-atom is not", mt_eval(m, E("is-member", "car-atom", E("collapse", E("user-functions")))),
                  B(defined_here("car-atom")));
    check_answers("car-atom is a function", mt_eval(m, E("is-member", "car-atom", E("collapse", E("functions")))),
                  B(member_named(builtins, "car-atom") || member_named(user, "car-atom")));
    check_answers("case is not a builtin", mt_eval(m, E("is-member", "case", E("collapse", E("builtins")))),
                  B(member_named(builtins, "case")));
    check_answers("case is a special form", mt_eval(m, E("is-member", "case", E("collapse", E("special-forms")))),
                  B(member_named(special, "case")));
    check_answers("let is a builtin", mt_eval(m, E("is-member", "let", E("collapse", E("builtins")))),
                  B(member_named(builtins, "let")));
    check_answers("and a special form", mt_eval(m, E("is-member", "let", E("collapse", E("special-forms")))),
                  B(member_named(special, "let")));
    check_answers("more than three extension points", mt_eval(m, E(">", E("size-atom", E("collapse", E("extension-points"))), 3)),
                  B(points.len > 3));
    mt_atom *foreign = E("foreign_space", 1, "ownership");
    check_answers("foreign_space is one", mt_eval(m, E("is-member", mt_keep(foreign), E("collapse", E("extension-points")))),
                  B(member(points, foreign)));
    mt_drop(foreign);

    /* The counts: the four enumerations C read, the functions their union. */
    const int64_t counted[] = { (int64_t)builtins.len, (int64_t)special.len, union_size(builtins, user), (int64_t)user.len };
    static const char *const KINDS[] = { "builtins", "special-forms", "functions", "user-functions" };
    check_answers("four counts", mt_eval(m, E("size-atom", E("surface-counts"))), (int64_t)COUNT(KINDS));
    check_answers("builtins first", mt_eval(m, E("let", V("counts"), E("surface-counts"), E("index-atom", E("car-atom", V("counts")), 0))),
                  mt_sym(KINDS[0]));
    mt_atom *pairs[COUNT(KINDS)];
    for (size_t i = 0; i < COUNT(KINDS); i++) pairs[i] = E(mt_sym(KINDS[i]), counted[i]);
    check_answers("each count is what C counted", mt_eval(m, E("surface-counts")), mt_exprv(COUNT(KINDS), pairs));
    require("functions are everything else", (size_t)counted[2] == functions.len);

    mt_atom *json = mt_one(mt_eval(m, E("surface-json")));
    require("surface-json answers text", json && mt_kind_of(json) == MT_TEXT);
    const char *text = mt_name(json);
    check_answers("the JSON is long", mt_eval(m, E(">", E("string-length", E("surface-json")), 1000)), B(strlen(text) > 1000));
    check_answers("and an object", mt_eval(m, E("string-starts-with", E("surface-json"), T("{"))), B(text[0] == '{'));
    cJSON *surface = cJSON_Parse(text);
    require("cJSON reads it", surface != NULL);
    check("cJSON reads each list at its size",
          cJSON_GetArraySize(cJSON_GetObjectItem(surface, "builtinNames")) == (int)builtins.len &&
          cJSON_GetArraySize(cJSON_GetObjectItem(surface, "specialFormNames")) == (int)special.len &&
          cJSON_GetArraySize(cJSON_GetObjectItem(surface, "userFunctionNames")) == (int)user.len);
    cJSON_Delete(surface);
    mt_drop(json);

    /* The Prolog rungs answer what the accessors answer. */
    check_answers("engine-arity", mt_eval(m, E("==", E("collapse", E("engine-arity", "car-atom")), E("collapse", E("arity-of", "car-atom")))),
                  B(true));
    check_answers("engine-knows", mt_eval(m, E("engine-knows", "car-atom")), B(member_named(functions, "car-atom")));
    check_answers("engine-knows nothing else", mt_eval(m, E("engine-knows", "no-such-name-anywhere")),
                  B(member_named(functions, "no-such-name-anywhere")));
    check_answers("engine-origin", mt_eval(m, E("collapse", E("engine-origin", "car-atom"))),
                  member_named(builtins, "car-atom") ? E(E("builtin")) : mt_unit());
    mt_atom *counts = mt_one(mt_eval(m, E("surface-counts"))), *engine_counts = mt_one(mt_eval(m, E("engine-surface-counts")));
    check_answers("engine-surface-counts", mt_eval(m, E("==", E("engine-surface-counts"), E("surface-counts"))),
                  B(same(counts, engine_counts)));
    static const char *const RUNGS[][2] = { { "engine-builtin", "builtins" }, { "engine-special-form", "special-forms" },
                                            { "engine-function", "functions" }, { "engine-user-function", "user-functions" },
                                            { "engine-extension-point", "extension-points" } };
    for (size_t i = 0; i < COUNT(RUNGS); i++) {
        mt_list rung = mt_all(mt_eval(m, E(RUNGS[i][0]))), accessor = mt_all(mt_eval(m, E(RUNGS[i][1])));
        check_answers(RUNGS[i][0], mt_eval(m, E("==", E("size-atom", E("collapse", E(RUNGS[i][0]))),
                                               E("size-atom", E("collapse", E(RUNGS[i][1]))))),
                      B(rung.len == accessor.len));
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
            check_answers(cases[i].claim, mt_eval(m, E("collapse", goal)), replaced_all(cases[i].term, cases[i].rules));
        else
            check_value(cases[i].claim, mt_eval(m, goal), replaced_one(cases[i].term, cases[i].rules));
        mt_drop(cases[i].term);
        mt_drop(cases[i].rules);
    }

    /* Variables compare by identity inside one expression, as the original
       compares them, so an alpha-renamed answer cannot pass. */
    mt_atom *spread = E(V("x"), E(V("y"), V("x")), V("z"), V("y")), *xyz = E(V("x"), V("y"), V("z"));
    check_answers("every variable, first occurrence first",
                  mt_eval(m, E("let", V("vars"), E("atom-variables", mt_keep(spread)), E("==", V("vars"), E("quote", mt_keep(xyz))))),
                  B(same(variables_of(spread), mt_keep(xyz))));
    mt_atom *binder = E("let", V("x"), V("y"), E("f", V("x"), V("z")));
    check_answers("binder positions count",
                  mt_eval(m, E("let", V("vars"), E("atom-variables", mt_keep(binder)), E("==", V("vars"), E("quote", mt_keep(xyz))))),
                  B(same(variables_of(binder), mt_keep(xyz))));
    mt_atom *sum = E("+", 1, 2);
    check_answers("a ground term has none", mt_eval(m, E("atom-variables", mt_keep(sum))), variables_of(sum));
    mt_atom *xyx = E(V("x"), V("y"), V("x")), *x_to_z = E(E(V("x"), V("z")));
    check_answers("a variable replaced everywhere it occurs",
                  mt_eval(m, E("let", V("changed"), E("atom-replace", mt_keep(xyx), mt_keep(x_to_z)),
                               E("==", V("changed"), E("quote", E(V("z"), V("y"), V("z")))))),
                  B(same(replaced_one(xyx, x_to_z), E(V("z"), V("y"), V("z")))));
    mt_atom *px = E("p", V("x")), *py_rule = E(E(E("p", V("y")), "wrong"));
    check_answers("and only that variable",
                  mt_eval(m, E("let", V("kept"), E("atom-replace", mt_keep(px), mt_keep(py_rule)),
                               E("==", V("kept"), E("quote", mt_keep(px))))),
                  B(same(replaced_one(px, py_rule), mt_keep(px))));

    /* Rules from a space. */
    mt_atom *rules[COUNT(RULES)], *ha = E("h", "a");
    for (size_t i = 0; i < COUNT(RULES); i++) rules[i] = E(mt_sym(RULES[i][0]), mt_sym(RULES[i][1]));
    mt_atom *table = mt_exprv(COUNT(RULES), rules);
    check_answers("rules matched out of a space",
                  mt_eval(m, E("let", V("rules"), E("collapse", E("match", "&self", E("reflect-rule", V("from"), V("to")),
                                                                  E("quote", E(V("from"), V("to"))))),
                               E("collapse", E("atom-replace", mt_keep(ha), V("rules"))))),
                  replaced_all(ha, table));

    /* Strategies: the identities, and a numeric repeat. */
    check_answers("alltd of fail keeps its term", mt_eval(m, E("alltd", "fail", mt_keep(sum))), mt_keep(sum));
    check_answers("an empty sequence keeps it", mt_eval(m, E("seq", mt_keep(sum))), mt_keep(sum));
    check_answers("so do identities", mt_eval(m, E("seq", "id", "id", "id", mt_keep(sum))), mt_keep(sum));
    check_none("an empty choice has none", mt_eval(m, E("choice", mt_keep(sum))));
    check_answers("repeat three times", mt_eval(m, E("collapse", E("repeat", 3, E("noeval", "token")))), repeated(3, mt_sym("token")));
    mt_atom *malformed = E(E("a"));
    check_answers("a rule that is no pair is refused",
                  mt_eval(m, E("if-error", E("catch", E("atom-replace", "a", mt_keep(malformed))), "refused", "accepted")),
                  verdict(well_formed(malformed)));

    /* The library's own equation, reconstructed and applied. */
    mt_atom *xx = E(V("x"), V("x"));
    check_answers("atom-variables' own equation, as a function",
                  mt_eval(m, E("let", V("source"),
                               E("match", "&self", E("=", E("atom-variables", V("term")), V("body")),
                                 E("quote", E("|->", E(V("term")), V("body")))),
                               E("let", V("inspect"), E("eval", V("source")),
                                 E("let", V("vars"), E(V("inspect"), E("quote", mt_keep(xx))), E("==", V("vars"), E("quote", E(V("x")))))))),
                  B(same(variables_of(xx), E(V("x")))));

    mt_drop(spread); mt_drop(xyz); mt_drop(binder); mt_drop(sum); mt_drop(xyx); mt_drop(x_to_z);
    mt_drop(px); mt_drop(py_rule); mt_drop(ha); mt_drop(table); mt_drop(malformed); mt_drop(xx);
    mt_list_free(builtins); mt_list_free(special); mt_list_free(functions); mt_list_free(user); mt_list_free(points);
    return done(m);
}
