/* Purpose: a call carrying a function specializes on it. Four map-flat
 *   variants differ only in where the function and the list sit in the
 *   head, so one C generator builds all eight equations from a table of call
 *   shapes; the other functions are equations built as terms, and p1, the
 *   function one of them is handed, is C.
 * Guarantees: all eleven claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>

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

/* How each variant spells a call on the function f and the list l. */
typedef mt_atom *(*call_shape)(mt_atom *f, mt_atom *l);
static mt_atom *shape1(mt_atom *f, mt_atom *l) { return E("map-flat", f, l); }
static mt_atom *shape2(mt_atom *f, mt_atom *l) { return E("map-flat2", E(l, f)); }
static mt_atom *shape3(mt_atom *f, mt_atom *l) { return E("map-flat3", E(f, l)); }
static mt_atom *shape4(mt_atom *f, mt_atom *l) { return E("map-flat4", E(V("v"), E(f, l))); }

/* (= (call $f ()) ()) and (= (call $f (cons $x $xs))
      (let $head ($f $x) (let $rest (call $f $xs) (cons $head $rest)))) */
static void define_mapper(metta *m, call_shape call)
{
    require("the empty case", mt_add(m, E("=", call(V("f"), mt_unit()), mt_unit())));
    require("the cons case", mt_add(m, E("=", call(V("f"), E("cons", V("x"), V("xs"))),
        E("let", V("head"), E(V("f"), V("x")),
          E("let", V("rest"), call(V("f"), V("xs")), E("cons", V("head"), V("rest")))))));
}

static mt_status p1(mt_call *call_, void *user)
{
    (void)user;
    mt_clear();
    int64_t x = mt_int(mt_arg(call_, 0));
    if (!mt_ok()) return mt_fail(call_, "p1 adds one to an integer");
    return mt_answer(call_, N(1 + x));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const struct { call_shape call; const char *typed; } mappers[] = {
        { shape1, NULL }, { shape2, NULL }, { shape3, "map-flat3" }, { shape4, "map-flat4" },
    };
    for (size_t i = 0; i < 4; i++) {
        if (mappers[i].typed)           /* (: map-flatN (-> Atom %Undefined%)) */
            require("declare its argument held", mt_add(m, E(":", mappers[i].typed, E("->", "Atom", "%Undefined%"))));
        define_mapper(m, mappers[i].call);
    }
    require("publish p1", mt_def(m, (mt_op){ .name = "p1", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = p1 }));

    assert(answers_are(mt_eval(m, shape1(E("+", 1), E(1, 2, 3))), E(E(2, 3, 4))) && "map-flat");
    assert(answers_are(mt_eval(m, shape2(E("+", 1), E(1, 2, 3))), E(E(2, 3, 4))) && "map-flat2");
    assert(answers_are(mt_eval(m, E("map-flat3", E("p1", E(1, 2)))), E(E(2, 3))) && "map-flat3 over the C function p1");
    assert(answers_are(mt_eval(m, E("map-flat4", E("x", E("p1", E(1, 2))))), E(E(2, 3))) && "map-flat4");

    require("(= (wrapper $f $list) (map-flat $f $list))",
            mt_add(m, E("=", E("wrapper", V("f"), V("list")), E("map-flat", V("f"), V("list")))));
    assert(answers_are(mt_eval(m, E("wrapper", E("+", 1), E(1, 2, 3))), E(E(2, 3, 4))) && "a wrapper specializes through");
    require("(= (wrapper2 $f) (id $f))", mt_add(m, E("=", E("wrapper2", V("f")), E("id", V("f")))));
    /* A partial application arrives in the wire grammar every seat reads,
       as the expression (partial F Args), so C compares it as a term. */
    mt_atom *partial = mt_one(mt_eval(m, E("wrapper2", E("+", 1))));
    assert(atom_is(mt_keep(partial), mt_one(mt_eval(m, E("+", 1)))) && "wrapper2 hands back the partial (+ 1) itself");
    assert(atom_is(partial, E("partial", "+", E(1))) && "the expression C builds for it");

    /* (= (trickyspec $f) (if (= ($f 1) 2) (trickyspec (+ 2)) ($f 1))) */
    require("define trickyspec", mt_add(m, E("=", E("trickyspec", V("f")),
        E("if", E("=", E(V("f"), 1), 2), E("trickyspec", E("+", 2)), E(V("f"), 1)))));
    assert(mt_one_int(mt_eval(m, E("trickyspec", E("+", 4)))) == 5 && "(trickyspec (+ 4)) is 5");
    assert(mt_one_int(mt_eval(m, E("trickyspec", E("+", 1)))) == 3 && "(trickyspec (+ 1)) is 3");

    /* fold-nested over a nested list */
    require("fold-nested over ()", mt_add(m, E("=", E("fold-nested", V("f"), V("init"), mt_unit()), V("init"))));
    require("fold-nested over a cons", mt_add(m, E("=", E("fold-nested", V("f"), V("init"), E("cons", V("x"), V("xs"))),
        E("if", E("is-expr", V("x")),
          E("fold-nested", V("f"), E("fold-nested", V("f"), V("init"), V("x")), V("xs")),
          E("fold-nested", V("f"), E(V("f"), V("init"), V("x")), V("xs"))))));
    assert(mt_one_int(mt_eval(m, E("fold-nested", "+", 0, E(1, E(2, 3))))) == 6
           && "(fold-nested + 0 (1 (2 3))) is 6");

    /* (= (higher-order-fun $a $b) (($a 1) ($b 1))) and two callers */
    require("define higher-order-fun", mt_add(m, E("=", E("higher-order-fun", V("a"), V("b")),
                                                   E(E(V("a"), 1), E(V("b"), 1)))));
    require("define fun2", mt_add(m, E("=", E("fun2"), E("higher-order-fun", E("+", 1), E("*", 1)))));
    require("define fun3", mt_add(m, E("=", E("fun3"), E("higher-order-fun", E("*", 1), E("+", 1)))));
    assert(answers_are(mt_eval(m, E("fun2")), E(E(2, 1))) && "fun2");
    assert(answers_are(mt_eval(m, E("fun3")), E(E(1, 2))) && "fun3");
    mt_close(m);
    return 0;
}
