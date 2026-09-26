/* Purpose: the hook judges chapter 15's two hook twins publish, the model a
 *   claimed space is held against, and the checks both make of a claimed
 *   write door. A judge is C data, a name and one rule
 *   per head of the offered (head x), each rule one of the four verdicts
 *   below. An atom no rule covers gets no answer, which the engine
 *   reports as a stuck hook. What lands follows from the verdicts alone: the
 *   offered atom on (Accept), its transform on (Accept <atom>), nothing
 *   otherwise, and with both slots claimed the post judge sees only an atom
 *   the pre judge let in as offered [source: engine/metta/space_hooks.pl,
 *   metta_space_hooked_add/4, metta_hook_post_phase/2 and
 *   metta_hook_invalid_verdict/5, whose arity refusal a judge published at
 *   one arity never meets; commit=5bae989dfe8735448d575856e72f735bb2043e24].
 * Assumes: one runtime, opened with mt_open(); a judge outlives the runtime
 *   that publishes it.
 */
#ifndef CH15_JUDGES_H
#define CH15_JUDGES_H
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

/* The verdict that lets the offered atom in, or `as` in its place; takes `as`. */
static inline mt_atom *accepting(mt_atom *as) { return as ? mt_expr("Accept", as) : mt_expr("Accept"); }

/* The verdict that raises with the words; takes them. */
static inline mt_atom *refusing(mt_atom *words) { return mt_expr("Refuse", words); }

/* The verdict that writes nothing. */
static inline mt_atom *dropping(void) { return mt_expr("Drop"); }

/* The two write hooks a space has, named as the engine names them. */
typedef enum { PRE_ADD, POST_ADD } slot;
static const char *const slot_names[] = { "pre-add", "post-add" };

typedef enum { ACCEPT, TRANSFORM, REFUSE, DROP } verdict;

/* One equation of a judge, (= (judge (head $x)) <verdict>), or with no head
   (= (judge $x) <verdict>), which covers every atom. A transform answers
   (Accept (into $x)) and a refusal (Refuse words). */
typedef struct rule {
    const char *head;
    verdict verdict;
    const char *into, *words;
} rule;

typedef struct judge {
    const char *name;
    const rule *rules;
    size_t n;
} judge;

#define JUDGE(judge_name, rule_array) \
    ((judge){ .name = (judge_name), .rules = (rule_array), .n = sizeof (rule_array) / sizeof *(rule_array) })

/* The rule covering an offered atom, or NULL. */
static inline const rule *rule_for(const judge *j, const mt_atom *offered)
{
    bool pair = mt_kind_of(offered) == MT_EXPR && mt_len(offered) == 2 && mt_kind_of(mt_at(offered, 0)) == MT_SYMBOL;
    for (size_t i = 0; i < j->n; i++)
        if (!j->rules[i].head || (pair && strcmp(j->rules[i].head, mt_name(mt_at(offered, 0))) == 0)) return &j->rules[i];
    return NULL;
}

static inline mt_atom *transformed(const rule *r, const mt_atom *offered) { return E(r->into, mt_keep(mt_at(offered, 1))); }

static inline mt_atom *verdict_atom(const rule *r, const mt_atom *offered)
{
    switch (r->verdict) {
    case ACCEPT: return accepting(NULL);
    case TRANSFORM: return accepting(transformed(r, offered));
    case REFUSE: return refusing(T(r->words));
    case DROP: break;
    }
    return dropping();
}

/* The judge as the function of one atom the engine calls. */
static inline mt_status judge_fn(mt_call *call, void *user)
{
    const mt_atom *offered = mt_arg(call, 0);
    const rule *r = rule_for(user, offered);
    return r ? mt_answer(call, verdict_atom(r, offered)) : MT_FAIL;
}

static inline void publish(metta *m, const judge *j)
{
    require(j->name, mt_def(m, (mt_op){ .name = j->name, .arity = 1, .effect = MT_EFFECT_CLASS_PURE_STRUCTURAL, .fn = judge_fn, .user = (void *)j }));
}

/* (declare-pre-add! space judge) and its kin: the claim and its release. */
static inline mt_atom *declaration(slot s, mt_space *space, const judge *j)
{
    char op[32];
    snprintf(op, sizeof op, "declare-%s!", slot_names[s]);
    return E(op, mt_spaceref(mt_space_name(space)), j->name);
}

static inline mt_answers *claim(metta *m, slot s, mt_space *space, const judge *j)
{
    return mt_eval(m, declaration(s, space, j));
}

static inline mt_answers *release(metta *m, slot s, mt_space *space)
{
    char op[32];
    snprintf(op, sizeof op, "undeclare-%s!", slot_names[s]);
    return mt_eval(m, E(op, mt_spaceref(mt_space_name(space))));
}

/* What lands when a judge sees an offered atom: the atom, its transform, or
   NULL. */
static inline mt_atom *lands(const judge *j, const mt_atom *offered)
{
    const rule *r = j ? rule_for(j, offered) : NULL;
    if (!j || (r && r->verdict == ACCEPT)) return mt_keep(offered);
    return r && r->verdict == TRANSFORM ? transformed(r, offered) : NULL;
}

/* What lands with a pre judge and a post judge, either NULL when that slot
   is unclaimed: the post judge revises only what landed as offered. */
static inline mt_atom *landing(const judge *pre, const judge *post, const mt_atom *offered)
{
    mt_atom *in = lands(pre, offered);
    if (!in || !post || !mt_eq(in, offered)) return in;
    mt_drop(in);
    return lands(post, offered);
}

/* The error a write raises when its judge refuses or covers nothing, as catch
   answers it; NULL when the write lands or drops. */
static inline mt_atom *write_error(const judge *j, slot s, mt_space *space, const mt_atom *offered)
{
    const rule *r = rule_for(j, offered);
    mt_atom *where = mt_spaceref(mt_space_name(space));
    if (!r) return E("Error", E("metta_hook_stuck", where, slot_names[s], j->name, mt_keep(offered)), "none");
    if (r->verdict == REFUSE) return E("Error", E("metta_add_refused", where, mt_keep(offered), T(r->words)), "none");
    mt_drop(where);
    return NULL;
}

/* A second judge claiming a claimed slot is refused, both judges named. */
static inline void check_conflict(metta *m, slot s, mt_space *space, const judge *prior, const judge *claimant)
{
    mt_atom *conflict = E("metta_hook_conflict", mt_spaceref(mt_space_name(space)), slot_names[s], prior->name, claimant->name);
    assert(answers_are(mt_eval(m, E("catch", declaration(s, space, claimant))), E(E("Error", conflict, "none"))) && "one claimant per slot");
}

/* The atoms a space should hold, in the order they landed. */
typedef struct model {
    mt_atom **atoms;
    size_t n, cap;
} model;

/* TAKES the atom; NULL, a write that landed nothing, leaves the model as it is. */
static inline void model_add(model *s, mt_atom *atom)
{
    if (!atom) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? 2 * s->cap : 8;
        s->atoms = realloc(s->atoms, s->cap * sizeof *s->atoms);
        require("room for the model", s->atoms != NULL);
    }
    s->atoms[s->n++] = atom;
}

static inline void model_free(model *s)
{
    for (size_t i = 0; i < s->n; i++) mt_drop(s->atoms[i]);
    free(s->atoms);
}

/* The claim that the space holds exactly the model's atoms, in order. */
static inline void check_holds(const char *claim, mt_space *space, const model *s)
{
    mt_atom **want = malloc((s->n + 1) * sizeof *want);
    require("room for the expectation", want != NULL);
    for (size_t i = 0; i < s->n; i++) want[i] = mt_keep(s->atoms[i]);
    assert(answers_are(mt_atoms(space), mt_exprv(s->n, want)) && claim);
    free(want);
}

/* Each write through mt_add with the claimed judges, NULL for an unclaimed
   slot, and the claim that the space then holds the model. TAKES the atoms. */
#define write_all(space, held, pre, post, ...) \
    write_all_((space), (held), (pre), (post), (mt_atom *[]){ __VA_ARGS__ }, MT_NARG(__VA_ARGS__))
static inline void write_all_(mt_space *space, model *held, const judge *pre, const judge *post, mt_atom **atoms, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        assert(mt_add(space, mt_keep(atoms[i])) && "the caller sees a write succeed");
        model_add(held, landing(pre, post, atoms[i]));
        check_holds("the space holds what the judges let stay", space, held);
        mt_drop(atoms[i]);
    }
}

/* Each write the judge in slot s refuses or covers nothing of, under catch:
   the Error C builds for it, and the claim that the space still holds the
   model. TAKES the atoms. */
#define raise_all(m, space, held, j, s, ...) \
    raise_all_((m), (space), (held), (j), (s), (mt_atom *[]){ __VA_ARGS__ }, MT_NARG(__VA_ARGS__))
static inline void raise_all_(metta *m, mt_space *space, const model *held, const judge *j, slot s, mt_atom **atoms, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        mt_atom *write = E("add-atom", mt_spaceref(mt_space_name(space)), mt_keep(atoms[i]));
        assert(answers_are(mt_eval(m, E("catch", write)), E(write_error(j, s, space, atoms[i]))) && "a refused or uncovered write raises");
        check_holds("and leaves the space as it was", space, held);
        mt_drop(atoms[i]);
    }
}

/* A refusal as the C caller meets it: mt_add fails, and the failure carries
   the rule's own words. TAKES the atom. */
static inline void check_refused_words(mt_space *space, const judge *j, mt_atom *offered)
{
    const rule *r = rule_for(j, offered);
    mt_clear();
    assert(r && r->verdict == REFUSE && !mt_add(space, mt_keep(offered)) &&
mt_error() == MT_ERROR && strstr(mt_errmsg(), r->words) != NULL
           && "mt_add fails with the rule's own words");
    mt_clear();
    mt_drop(offered);
}
#endif
