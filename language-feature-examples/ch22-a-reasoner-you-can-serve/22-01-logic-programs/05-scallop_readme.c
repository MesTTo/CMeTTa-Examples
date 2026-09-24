/* Purpose: the five Scallop README examples, each a relation C keeps as a
 *   table and turns into atoms, and a program whose equations are the terms
 *   they are. C computes each printed answer from its tables: the paths over
 *   the edges, walked the way the rules walk them, one step and then onward;
 *   the numbers that are not odd; the objects per colour; each class's
 *   student with the top grade; and the objects whose kind has animal among
 *   its ancestors, counted once each however many derivations reach them.
 * Guarantees: all five claims of the original hold [tested: make twins;
 *   commit=4fe77404069bc1a630ecc9e7860856a1117a200c].
 */
#define MT_SHORTHAND
#include "common.h"
#include "lowering.h"

static mt_atom *here(void) { return E("context-space"); }

/* (match (context-space) PATTERN TEMPLATE). TAKES both. */
static mt_atom *matching(mt_atom *pattern, mt_atom *answer) { return E("match", here(), pattern, answer); }

static void define(metta *m, mt_atom *head, mt_atom *body) { require("an equation", mt_add(m, E("=", head, body))); }

/* ---- paths over edges ---- */
static const int64_t edges[][2] = { { 0, 1 }, { 1, 2 } };
#define EDGES (sizeof edges / sizeof *edges)

/* Every node a path from FROM reaches, as the rules find them: each edge
   out, then onward from where it lands. */
static void paths_from(int64_t from, int64_t origin, mt_list *out)
{
    for (size_t e = 0; e < EDGES; e++)
        if (edges[e][0] == from) {
            mt_atom **grown = mt_resize(out->items, (out->len + 1) * sizeof *grown);
            require("room", grown != NULL);
            out->items = grown;
            out->items[out->len++] = E(origin, edges[e][1]);
        }
    for (size_t e = 0; e < EDGES; e++)
        if (edges[e][0] == from) paths_from(edges[e][1], origin, out);
}

/* ---- numbers ---- */
enum { NUMBERS = 11 };

/* ---- colours ---- */
static const struct { int64_t object; const char *colour; } colours[] = { { 0, "blue" }, { 1, "green" }, { 2, "blue" } };
static const char *const palette[] = { "blue", "green" };

/* ---- grades ---- */
static const struct { int64_t class; const char *student; int64_t grade; } grades[] = {
    { 0, "tom", 50 }, { 0, "jerry", 70 }, { 0, "alice", 60 }, { 1, "bob", 80 }, { 1, "sherry", 90 }, { 1, "frank", 30 },
};
#define GRADES (sizeof grades / sizeof *grades)

/* ---- kinds ---- */
static const char *const kinds[][2] = { { "giraffe", "mammal" }, { "tiger", "mammal" }, { "mammal", "animal" } };
static const struct { int64_t object; const char *kind; } names[] = { { 1, "giraffe" }, { 1, "tiger" }, { 2, "giraffe" }, { 2, "tiger" } };
#define NAMES (sizeof names / sizeof *names)

static bool descends(const char *kind, const char *ancestor)
{
    for (size_t i = 0; i < sizeof kinds / sizeof *kinds; i++)
        if (strcmp(kinds[i][0], kind) == 0 && (strcmp(kinds[i][1], ancestor) == 0 || descends(kinds[i][1], ancestor))) return true;
    return false;
}

int main(void)
{
    metta *m = open_engine();

    for (size_t e = 0; e < EDGES; e++) require("an edge", mt_add(m, E("sc-edge", edges[e][0], edges[e][1])));
    define(m, E("sc-edge-to", V("a")), matching(E("sc-edge", V("a"), V("b")), V("b")));
    define(m, E("sc-path-to", V("a")), E("sc-edge-to", V("a")));
    define(m, E("sc-path-to", V("a")), E("let", V("b"), E("sc-edge-to", V("a")), E("sc-path-to", V("b"))));
    define(m, E("sc-paths"), E("collapse", E("let", V("a"), matching(E("sc-edge", V("a"), V("_")), V("a")),
                                              E("let", V("b"), E("sc-path-to", V("a")), E(V("a"), V("b"))))));
    mt_list paths = { NULL, 0 };
    for (size_t e = 0; e < EDGES; e++) paths_from(edges[e][0], edges[e][0], &paths);
    check_answers("the paths over the edges", mt_eval(m, E("sc-paths")), mt_exprv(paths.len, paths.items));
    mt_free(paths.items);

    for (int64_t x = 0; x < NUMBERS; x++) require("a number", mt_add(m, E("sc-number", x)));
    define(m, E("sc-odd?", 1), B(true));
    define(m, E("sc-odd?", V("x")), E("let", V("n"), matching(E("sc-number", V("x")), V("x")), E("sc-odd?", T_SUB(V("n"), 2))));
    define(m, E("sc-evens"), E("collapse", E("let", V("y"), matching(E("sc-number", V("y")), V("y")),
                                               E("let", B(true), E("not-provable", E("sc-odd?", V("y"))), V("y")))));
    mt_atom *evens[NUMBERS];
    size_t nevens = 0;
    for (int64_t x = 0; x < NUMBERS; x++)
        if (C_MOD(x, 2) == 0) evens[nevens++] = N(x);
    check_answers("the numbers that are not odd", mt_eval(m, E("sc-evens")), mt_exprv(nevens, evens));

    for (size_t i = 0; i < 3; i++) require("a colour", mt_add(m, E("sc-object-color", colours[i].object, T(colours[i].colour))));
    define(m, E("sc-one-color", V("c")), E("let", V("o"), matching(E("sc-object-color", V("o"), V("c")), V("o")), 1));
    define(m, E("sc-color-count", V("c")), E("foldall", "+", E("sc-one-color", V("c")), 0));
    define(m, E("sc-color-counts"), E("collapse", E("let", V("c"), E("superpose", E(T(palette[0]), T(palette[1]))),
                                                     E(V("c"), E("sc-color-count", V("c"))))));
    mt_atom *counts[2];
    for (size_t p = 0; p < 2; p++) {
        int64_t n = 0;
        for (size_t i = 0; i < 3; i++) n += strcmp(colours[i].colour, palette[p]) == 0;
        counts[p] = E(T(palette[p]), n);
    }
    check_answers("objects per colour", mt_eval(m, E("sc-color-counts")), mt_exprv(2, counts));

    for (size_t i = 0; i < GRADES; i++)
        require("a grade", mt_add(m, E("sc-class-student-grade", grades[i].class, T(grades[i].student), grades[i].grade)));
    define(m, E("sc-pick-max", V("a"), V("b")), T_IF(T_GT(V("a"), V("b")), V("a"), V("b")));
    define(m, E("sc-class-max", V("c")),
           E("foldall", "sc-pick-max", matching(E("sc-class-student-grade", V("c"), V("_"), V("g")), V("g")), -1));
    define(m, E("sc-class-top"),
           E("collapse", E("let", V("c"), E("superpose", E(0, 1)),
                           E("let", V("g"), E("sc-class-max", V("c")),
                             E("let", V("s"), matching(E("sc-class-student-grade", V("c"), V("s"), V("g")), V("s")),
                               E(V("c"), V("s")))))));
    mt_atom *tops[2];
    for (int64_t c = 0; c < 2; c++) {
        size_t best = GRADES;
        for (size_t i = 0; i < GRADES; i++)
            if (grades[i].class == c && (best == GRADES || C_GT(grades[i].grade, grades[best].grade))) best = i;
        tops[c] = E(c, T(grades[best].student));
    }
    check_answers("each class's top student", mt_eval(m, E("sc-class-top")), mt_exprv(2, tops));

    for (size_t i = 0; i < 3; i++) require("an is-a", mt_add(m, E("sc-is-a", kinds[i][0], kinds[i][1])));
    for (size_t i = 0; i < NAMES; i++) require("a name", mt_add(m, E("sc-name", names[i].object, names[i].kind)));
    define(m, E("sc-parent-kind", V("x")), matching(E("sc-is-a", V("x"), V("y")), V("y")));
    define(m, E("sc-ancestor-kind", V("x")), E("sc-parent-kind", V("x")));
    define(m, E("sc-ancestor-kind", V("x")), E("let", V("y"), E("sc-parent-kind", V("x")), E("sc-ancestor-kind", V("y"))));
    define(m, E("sc-animal-object"),
           E("let", E(V("o"), V("kind")), matching(E("sc-name", V("o"), V("kind")), E(V("o"), V("kind"))),
             E("let", V("ancestor"), E("sc-ancestor-kind", V("kind")),
               T_IF(T_EQ(V("ancestor"), S("animal")), V("o"), E("empty")))));
    define(m, E("sc-one-animal"), E("let", V("o"), E("unique", E("sc-animal-object")), 1));
    define(m, E("sc-animal-count"), E("foldall", "+", E("sc-one-animal"), 0));
    int64_t animals = 0;
    for (size_t i = 0; i < NAMES; i++) {
        bool first = true;
        for (size_t j = 0; j < i; j++) first = first && !(names[j].object == names[i].object && descends(names[j].kind, "animal"));
        animals += first && descends(names[i].kind, "animal");
    }
    check_int("each animal object once", mt_one_int(mt_eval(m, E("sc-animal-count"))), animals);
    return done(m);
}
