/* Purpose: C's model of the matespace programs: a space of num atoms grown
 *   by expand and mate, and the order the engine evaluates them in, which is
 *   what fixes how many answers the final match gives. A term is kept as its
 *   C string, the letters of its constructors, outermost first, then its
 *   base's name, so (M (W Z)) is "MWZ"; the constructors are the capitals
 *   M, W and C and neither base, Z or collapse, starts with one, so every
 *   string reads one way. The space is those strings in the order they were
 *   added, with an ordered index for add-atom-no-duplicate's question
 *   [source: POSIX <search.h>, tsearch]. The evaluation order the model
 *   follows, each rule measured against the engine rather than assumed:
 *   - a match walks the atoms present when it is called, whatever is added
 *     while its answers are used;
 *   - a nondeterministic answer runs the rest of the program to its end
 *     before the next answer is asked for;
 *   - expand's pair of additions answers only when both atoms are new, and
 *     a second is not attempted after the first finds its atom there;
 *   - (superpose (collapse (match ...))) is not a snapshot: superpose holds
 *     its argument as written and walks its elements, so the symbol collapse
 *     is its first answer, a term like any other, and the match runs only
 *     when superpose reaches it, after the first answer's branch is done.
 *   [measured 2026-09-24: this model and the engine agree at 390 rounds of
 *   03 and at 1, 2, 3, 4, 5, 6, 8, 10 and 80 rounds of 04, where a
 *   per-round snapshot would answer 4 at one round against the engine's 50]
 * Assumes: the includer includes common.h first.
 * Guarantees: the count each driver leaves is the number of answers the
 *   original's final match gives over every branch [tested: make twins;
 *   commit=WORKTREE].
 * Owns resources: a term_space owns its strings, its array and its index;
 *   term_space_free() releases them.
 */
#ifndef CH22_MATESPACE_H
#define CH22_MATESPACE_H
#include <search.h>

typedef struct term_space {
    char **terms;
    size_t len;
    void *index;
    int64_t answers;
} term_space;

static inline int term_order(const void *a, const void *b) { return strcmp(a, b); }

static inline bool term_held(const term_space *s, const char *term) { return tfind(term, &s->index, term_order) != NULL; }

/* add-atom-no-duplicate: the term added, or false where it is there. */
static inline bool term_add(term_space *s, const char *term)
{
    if (term_held(s, term)) return false;
    char *copy = strdup(term);
    char **grown = realloc(s->terms, (s->len + 1) * sizeof *grown);
    require("room", copy && grown && tsearch(copy, &s->index, term_order));
    s->terms = grown;
    s->terms[s->len++] = copy;
    return true;
}

static inline void term_space_free(term_space *s)
{
    for (size_t i = 0; i < s->len; i++) {
        tdelete(s->terms[i], &s->index, term_order);
        free(s->terms[i]);
    }
    free(s->terms);
    *s = (term_space){ NULL, 0, NULL, 0 };
}

/* (OP TERM) as a string: the constructor's letter before the chain. */
static inline char *wrapped(char op, const char *term)
{
    size_t n = strlen(term);
    char *out = malloc(n + 2);
    require("room", out != NULL);
    out[0] = op;
    memcpy(out + 1, term, n + 1);
    return out;
}

/* The chain under a term's outermost OP, or NULL where it has another. */
static inline const char *unwrapped(char op, const char *term) { return term[0] == op ? term + 1 : NULL; }

/* What runs after an answer: the rest of the program, with the rounds left. */
typedef void (*then)(term_space *, int64_t);

/* expand's answer for T: both of (M T) and (W T) new. */
static inline bool expanded(term_space *s, const char *t)
{
    char *m = wrapped('M', t), *w = wrapped('W', t);
    bool answered = term_add(s, m) && term_add(s, w);
    free(m), free(w);
    return answered;
}

/* mate's answer for T: (W T) there and (C T) new. */
static inline bool mated(term_space *s, const char *t)
{
    char *w = wrapped('W', t), *c = wrapped('C', t);
    bool answered = term_held(s, w) && term_add(s, c);
    free(w), free(c);
    return answered;
}

/* A round of expand or mate, each answer running NEXT: LEADING, where
   given, is the symbol superpose answers before it reaches the match, and
   the match walks the atoms there when it is reached, each under the
   constructor UNDER, or every atom where UNDER is 0. */
static inline void round_of(term_space *s, const char *leading, char under, bool (*answer)(term_space *, const char *), int64_t n,
                            then next)
{
    if (leading && answer(s, leading)) next(s, n);
    size_t present = s->len;
    for (size_t i = 0; i < present; i++) {
        const char *t = under ? unwrapped(under, s->terms[i]) : s->terms[i];
        if (t && answer(s, t)) next(s, n);
    }
}

/* The final match: one answer for every num atom. */
static inline void count_answers(term_space *s, int64_t n)
{
    (void)n;
    s->answers += (int64_t)s->len;
}
#endif
