/* Purpose: list structure, twice over. In C an expression's head and tail
 *   need no copy: the tail is mt_expr_ref() over the parent's own child
 *   vector, holding the parent alive until the tail goes. len is a C function
 *   counting children, and the engine's own let over (cons $Head $Tail) and
 *   cons agree with the C view.
 * Guarantees: (1 2 3 4 5 6) splits as 1 and (2 3 4 5 6) both ways, (len (1 2
 *   3)) is 3, and (cons 42 ()) is (42) [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
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

static void drop_parent(void *parent) { mt_drop(parent); }

/* The tail of a list as a view over its parent's children: no child is
   copied, and the parent lives as long as the view does. */
static mt_atom *tail_of(const mt_atom *list)
{
    mt_atom *parent = mt_keep(list);
    mt_atom *tail = mt_expr_ref(mt_len(list) - 1, mt_children(list) + 1, parent, drop_parent);
    if (!tail) mt_drop(parent);         /* a failed borrow leaves the owner here */
    return tail;
}

static mt_status len(mt_call *call, void *user)
{
    (void)user;
    const mt_atom *list = mt_arg(call, 0);
    if (mt_kind_of(list) != MT_EXPR) return MT_FAIL;
    return mt_answer(call, N((int64_t)mt_len(list)));
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *six = E(1, 2, 3, 4, 5, 6);
    assert(mt_int(mt_at(six, 0)) == 1 && "the head is 1");
    assert(atom_is(tail_of(six), E(2, 3, 4, 5, 6))
           && "the tail is (2 3 4 5 6), sharing the parent's children");
    /* (let (cons $Head $Tail) (1 2 3 4 5 6) ($Head $Tail)) */
    assert(answers_are(mt_eval(m, E("let", E("cons", V("Head"), V("Tail")), six, E(V("Head"), V("Tail")))), E(E(1, E(2, 3, 4, 5, 6))))
           && "the engine's cons pattern splits it the same way");

    require("publish len", mt_def(m, (mt_op){ .name = "len", .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = len }));
    assert(mt_one_int(mt_eval(m, E("len", E(1, 2, 3)))) == 3 && "(len (1 2 3)) is 3");
    assert(answers_are(mt_eval(m, E("cons", 42, mt_unit())), E(E(42))) && "(cons 42 ()) is (42)");
    mt_close(m);
    return 0;
}
