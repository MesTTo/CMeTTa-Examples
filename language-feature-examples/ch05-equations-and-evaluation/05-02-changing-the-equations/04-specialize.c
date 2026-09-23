/* Purpose: a call carrying a function specializes on it. Four map-flat
 *   variants differ only in where the function and the list sit in the
 *   head, so one C generator builds all eight equations from a table of call
 *   shapes; the other functions are equations built as terms, and p1, the
 *   function one of them is handed, is C.
 * Guarantees: all eleven claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    metta *m = open_engine();
    static const struct { call_shape call; const char *typed; } mappers[] = {
        { shape1, NULL }, { shape2, NULL }, { shape3, "map-flat3" }, { shape4, "map-flat4" },
    };
    for (size_t i = 0; i < 4; i++) {
        if (mappers[i].typed)           /* (: map-flatN (-> Atom %Undefined%)) */
            require("declare its argument held", mt_add(m, E(":", mappers[i].typed, E("->", "Atom", "%Undefined%"))));
        define_mapper(m, mappers[i].call);
    }
    require("publish p1", mt_def(m, (mt_op){ .name = "p1", .arity = 1, .effect = MT_PURE, .fn = p1 }));

    check_answers("map-flat", mt_eval(m, shape1(E("+", 1), E(1, 2, 3))), E(2, 3, 4));
    check_answers("map-flat2", mt_eval(m, shape2(E("+", 1), E(1, 2, 3))), E(2, 3, 4));
    check_answers("map-flat3 over the C function p1", mt_eval(m, E("map-flat3", E("p1", E(1, 2)))), E(2, 3));
    check_answers("map-flat4", mt_eval(m, E("map-flat4", E("x", E("p1", E(1, 2))))), E(2, 3));

    require("(= (wrapper $f $list) (map-flat $f $list))",
            mt_add(m, E("=", E("wrapper", V("f"), V("list")), E("map-flat", V("f"), V("list")))));
    check_answers("a wrapper specializes through", mt_eval(m, E("wrapper", E("+", 1), E(1, 2, 3))), E(2, 3, 4));
    require("(= (wrapper2 $f) (id $f))", mt_add(m, E("=", E("wrapper2", V("f")), E("id", V("f")))));
    /* A partial application arrives in the wire grammar every seat reads,
       as the expression (partial F Args), so C compares it as a term. */
    mt_atom *partial = mt_one(mt_eval(m, E("wrapper2", E("+", 1))));
    check_atom("wrapper2 hands back the partial (+ 1) itself", mt_keep(partial), mt_one(mt_eval(m, E("+", 1))));
    check_atom("the expression C builds for it", partial, E("partial", "+", E(1)));

    /* (= (trickyspec $f) (if (= ($f 1) 2) (trickyspec (+ 2)) ($f 1))) */
    require("define trickyspec", mt_add(m, E("=", E("trickyspec", V("f")),
        E("if", E("=", E(V("f"), 1), 2), E("trickyspec", E("+", 2)), E(V("f"), 1)))));
    check_int("(trickyspec (+ 4)) is 5", mt_one_int(mt_eval(m, E("trickyspec", E("+", 4)))), 5);
    check_int("(trickyspec (+ 1)) is 3", mt_one_int(mt_eval(m, E("trickyspec", E("+", 1)))), 3);

    /* fold-nested over a nested list */
    require("fold-nested over ()", mt_add(m, E("=", E("fold-nested", V("f"), V("init"), mt_unit()), V("init"))));
    require("fold-nested over a cons", mt_add(m, E("=", E("fold-nested", V("f"), V("init"), E("cons", V("x"), V("xs"))),
        E("if", E("is-expr", V("x")),
          E("fold-nested", V("f"), E("fold-nested", V("f"), V("init"), V("x")), V("xs")),
          E("fold-nested", V("f"), E(V("f"), V("init"), V("x")), V("xs"))))));
    check_int("(fold-nested + 0 (1 (2 3))) is 6",
              mt_one_int(mt_eval(m, E("fold-nested", "+", 0, E(1, E(2, 3))))), 6);

    /* (= (higher-order-fun $a $b) (($a 1) ($b 1))) and two callers */
    require("define higher-order-fun", mt_add(m, E("=", E("higher-order-fun", V("a"), V("b")),
                                                   E(E(V("a"), 1), E(V("b"), 1)))));
    require("define fun2", mt_add(m, E("=", E("fun2"), E("higher-order-fun", E("+", 1), E("*", 1)))));
    require("define fun3", mt_add(m, E("=", E("fun3"), E("higher-order-fun", E("*", 1), E("+", 1)))));
    check_answers("fun2", mt_eval(m, E("fun2")), E(2, 1));
    check_answers("fun3", mt_eval(m, E("fun3")), E(1, 2));
    return done(m);
}
