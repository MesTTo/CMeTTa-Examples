/* Purpose: a space whose atoms live in Redis, attached from C when the box
 *   has both halves: lib_redis's provider, which a platform without
 *   library(redis) refuses by name, and a server at 127.0.0.1:6379, which
 *   refuses a connection. C asks each once, as the original does, through
 *   catch, and keeps the two answers as booleans where the original keeps
 *   atoms in &redis-status. With both, it writes through the seam and reads
 *   back, joins shared facts with a local one, removes exactly, detaches
 *   leaving the facts, reattaches and finds them, is refused a second
 *   attach, and leaves the store as it found it; C holds the reads to its
 *   own table of cities, sorted by mt_order. Without either it says so, as
 *   the original does, and proves the refusal it skipped on.
 * Guarantees: the original states no claim outside its guards; the twin runs
 *   the guarded ones exactly when the original does, and otherwise proves
 *   why it skipped [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
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

static const char *const address = "127.0.0.1:6379";

typedef struct city {
    const char *name, *country;
} city;

static const city cities[] = { { "paris", "france" }, { "lyon", "france" }, { "berlin", "germany" } };
#define CITIES (sizeof cities / sizeof *cities)

/* Whether an answer is the engine's refusal, (Error ...). */
static bool refused(const mt_atom *answer)
{
    return answer && mt_kind_of(answer) == MT_EXPR && mt_len(answer) > 0 && mt_kind_of(mt_at(answer, 0)) == MT_SYMBOL &&
           strcmp(mt_name(mt_at(answer, 0)), "Error") == 0;
}

/* One guarded ask: its answer, caught. TAKES goal. */
static mt_atom *caught(metta *m, mt_atom *goal) { return mt_first(mt_eval(m, E("catch", goal))); }

/* A guarded ask the program needs to succeed. TAKES goal. */
static void must(metta *m, const char *what, mt_atom *goal)
{
    mt_atom *answer = caught(m, goal);
    require(what, answer && !refused(answer));
    mt_drop(answer);
}

/* The head of a refusal's ball, (Error (head ...) context), or NULL. */
static const char *ball_head(const mt_atom *answer)
{
    if (!refused(answer) || mt_len(answer) < 2) return NULL;
    const mt_atom *ball = mt_at(answer, 1);
    return mt_kind_of(ball) == MT_EXPR && mt_len(ball) > 0 && mt_kind_of(mt_at(ball, 0)) == MT_SYMBOL
               ? mt_name(mt_at(ball, 0))
               : NULL;
}

static mt_atom *attach(metta *m) { return caught(m, E("redis-attach", mt_spaceref("&shared"), mt_text(address))); }

/* The cities of one country C holds, stored or not, sorted by mt_order. */
static mt_list in(const char *country, const bool *stored)
{
    mt_list names = { mt_alloc(CITIES * sizeof *names.items), 0 };
    require("room", names.items != NULL);
    for (size_t i = 0; i < CITIES; i++)
        if (stored[i] && strcmp(cities[i].country, country) == 0) names.items[names.len++] = S(cities[i].name);
    qsort(names.items, names.len, sizeof *names.items, mt_order);
    return names;
}

/* What the shared space answers for (city $c country), sorted. */
static mt_list read_back(mt_space *shared, const char *country)
{
    mt_list got = { NULL, 0 };
    mt_rows (row, mt_query(shared, E("city", V("c"), country), NULL)) {
        mt_atom **grown = mt_resize(got.items, (got.len + 1) * sizeof *got.items);
        require("room", grown != NULL);
        got.items = grown;
        got.items[got.len++] = mt_keep(mt_bound(row, "c"));
    }
    qsort(got.items, got.len, sizeof *got.items, mt_order);
    return got;
}

static void agree(const char *claim, mt_list got, mt_list want)
{
    assert(list_is(got, mt_exprv(want.len, want.items)) && claim);
    mt_free(want.items);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    mt_atom *imported = caught(m, E("import!", "&self", E("library", "lib_redis")));
    const bool available = imported && !refused(imported);
    mt_atom *attached_answer = available ? attach(m) : NULL;
    const bool attached = attached_answer && !refused(attached_answer);
    if (!attached) {
        printf("SKIPPED a-shared-space-on-redis: no Redis provider or no server at %s\n", address);
        assert(refused(available ? attached_answer : imported) && "the skip rests on a refusal the engine gave");
        mt_drop(imported);
        mt_drop(attached_answer);
        mt_close(m);
        return 0;
    }
    mt_drop(imported);
    mt_drop(attached_answer);

    mt_space *shared = mt_space_open(m, "&shared");
    require("open &shared", shared != NULL);
    bool stored[CITIES];
    for (size_t i = 0; i < CITIES; i++) {
        require("write through the seam", mt_add(shared, E("city", cities[i].name, cities[i].country)));
        stored[i] = true;
    }
    agree("a write reads back through the seam", read_back(shared, "france"), in("france", stored));

    mt_space *local = mt_space_open(m, "&local");
    require("open &local", local != NULL);
    require("a local fact", mt_add(local, E("capital", "france", "paris")));
    assert(answers_are(mt_eval(m, E("match", mt_spaceref("&local"), E("capital", V("country"), V("city")),
                                    E("match", mt_spaceref("&shared"), E("city", V("city"), V("country")), V("city")))), E("paris"))
           && "a join is half a Redis read and half a local match");

    for (size_t i = 0; i < CITIES; i++)
        if (strcmp(cities[i].name, "berlin") == 0) {
            require("remove berlin", mt_del(shared, E("city", cities[i].name, cities[i].country)));
            stored[i] = false;
        }
    agree("removal reaches the shared set", read_back(shared, "germany"), in("germany", stored));

    must(m, "detach, leaving the facts in Redis", E("redis-detach", mt_spaceref("&shared")));
    must(m, "reattach", E("redis-attach", mt_spaceref("&shared"), mt_text(address)));
    agree("the next attach finds them", read_back(shared, "france"), in("france", stored));

    mt_atom *second = attach(m);
    const char *head = ball_head(second);
    assert(head && strcmp(head, "permission_error") == 0 && "a second attach is refused rather than raced");
    mt_drop(second);

    for (size_t i = 0; i < CITIES; i++)
        if (stored[i]) require("leave the store as found", mt_del(shared, E("city", cities[i].name, cities[i].country)));
    assert(!mt_first(mt_atoms(shared)) && mt_ok() && "the shared store is empty again");
    must(m, "detach", E("redis-detach", mt_spaceref("&shared")));
    mt_space_close(local);
    mt_space_close(shared);
    mt_close(m);
    return 0;
}
