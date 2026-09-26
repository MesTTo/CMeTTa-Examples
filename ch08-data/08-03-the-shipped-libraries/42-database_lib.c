/* Purpose: lib_database, held against a store of C's own in SQLite, C's
 *   embedded database. A store is a directory holding one SQLite file whose
 *   table keeps each value in insertion order; SQLite's exclusive locking
 *   mode is the lifetime lock a second owner is refused by; removal takes
 *   the first alpha-identical row, 1 and 1.0 distinct; a file that is no
 *   store is refused whole and its bytes left for repair; and a sync policy
 *   with no name is refused before anything is created. The original's
 *   segment lets and unify are computed by C, with a matcher of its own that
 *   follows the engine's rules, numbers by =:= and a pattern's (:= X) as a
 *   ==/2 guard, and the equations read back are summed by C over the stored
 *   syntax, exactly, in GMP.
 * Build: cc 42-database_lib.c $(pkg-config --cflags --libs cmetta gmp sqlite3)
 * text: a row keeps a value as its MeTTa source, written by mt_write_dup
 *   and read back by mt_parsen, so a text holding NUL survives; a store
 *   keeping syntax in a file keeps it as text.
 * Owns resources: each store its SQLite connection, closed by closed(),
 *   which a scope calls however its callback ends, and its struct, freed by
 *   whoever opened it; the C work directory, removed by nftw at the end.
 * Guarantees: all seventy-one claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#if __has_include(<gmp.h>) && __has_include(<sqlite3.h>)
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_fixtures/exact_oracle.h"
#include <errno.h>
#include <ftw.h>
#include <libgen.h>
#include <limits.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>

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

/* A store: its connection, NULL once closed. */
typedef struct store {
    sqlite3 *db;
} store;

/* The sync policies by name, beside SQLite's synchronous setting for each:
   buffer, flush every write, or finish every write. */
static const struct {
    const char *name, *synchronous;
} policies[] = { { "none", "OFF" }, { "flush", "NORMAL" }, { "close", "FULL" } };

/* dir/name into a PATH_MAX buffer; a path that does not fit is refused, not
   cut short. */
static void join(char *out, const char *dir, const char *name)
{
    int n = snprintf(out, PATH_MAX, "%s/%s", dir, name);
    require("the path fits", n > 0 && n < PATH_MAX);
}

/* The file a store keeps in its directory. */
static const char journal_name[] = "journal.sqlite";

/* A store opened or created; NULL for a policy with no name, before the
   directory exists, for a store another owner holds, and for a file that is
   no store. */
static store *opened(const char *dir, const char *policy)
{
    const char *synchronous = NULL;
    for (size_t i = 0; i < sizeof policies / sizeof *policies; i++)
        if (strcmp(policies[i].name, policy) == 0) synchronous = policies[i].synchronous;
    if (!synchronous || (mkdir(dir, 0777) != 0 && errno != EEXIST)) return NULL;
    store *s = calloc(1, sizeof *s);
    char setup[256], journal[PATH_MAX];
    require("room for a store", s != NULL);
    join(journal, dir, journal_name);
    snprintf(setup, sizeof setup,
             "PRAGMA locking_mode=EXCLUSIVE; PRAGMA synchronous=%s; BEGIN EXCLUSIVE;"
             " CREATE TABLE IF NOT EXISTS atoms(value BLOB NOT NULL); COMMIT;",
             synchronous);
    if (sqlite3_open(journal, &s->db) != SQLITE_OK || sqlite3_exec(s->db, setup, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(s->db);
        free(s);
        return NULL;
    }
    return s;
}

/* Values are syntax: nothing that is a live resource of any seat. */
static bool syntax(const mt_atom *a)
{
    mt_kind kind = mt_kind_of(a);
    if (kind == MT_HANDLE || kind == MT_OBJECT || kind == MT_SPACE) return false;
    for (size_t i = 0; kind == MT_EXPR && i < mt_len(a); i++)
        if (!syntax(mt_at(a, i))) return false;
    return true;
}

static bool added(store *s, const mt_atom *value)
{
    if (!s || !s->db || !syntax(value)) return false;
    mt_string source = mt_write_dup(value);
    sqlite3_stmt *st = NULL;
    bool fine = source.data && sqlite3_prepare_v2(s->db, "INSERT INTO atoms(value) VALUES(?)", -1, &st, NULL) == SQLITE_OK &&
                sqlite3_bind_blob64(st, 1, source.data, source.len, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    mt_free(source.data);
    return fine;
}

/* A doubling array with room for item n: the same array, or a larger one. */
static void *room(void *items, size_t *cap, size_t n, size_t size)
{
    if (n < *cap) return items;
    *cap = *cap ? 2 * *cap : 8;
    items = realloc(items, *cap * size);
    require("room to grow", items != NULL);
    return items;
}

/* Atoms gathered one at a time, then taken whole as one expression. */
typedef struct gathered {
    mt_atom **items;
    size_t n, cap;
} gathered;

static void gather(gathered *g, mt_atom *a)
{
    g->items = room(g->items, &g->cap, g->n, sizeof *g->items);
    g->items[g->n++] = a;
}

static mt_atom *gathered_expr(gathered *g)
{
    mt_atom *out = mt_exprv(g->n, g->items);
    free(g->items);
    return out;
}

/* A statement over a store's rows, each its rowid and its value, oldest
   first. */
static sqlite3_stmt *rows_of(store *s)
{
    sqlite3_stmt *st = NULL;
    require("the rows query", sqlite3_prepare_v2(s->db, "SELECT rowid, value FROM atoms ORDER BY rowid", -1, &st, NULL) == SQLITE_OK);
    return st;
}

/* The value the current row holds, read back from its source. */
static mt_atom *row_value(sqlite3_stmt *st)
{
    mt_atom *value = mt_parsen(sqlite3_column_blob(st, 1), (size_t)sqlite3_column_bytes(st, 1));
    require("a stored value reads back", value != NULL);
    return value;
}

/* Every value in insertion order; NULL when closed. */
static mt_atom *snapshot(store *s)
{
    if (!s || !s->db) return NULL;
    sqlite3_stmt *st = rows_of(s);
    gathered values = { 0 };
    int step;
    while ((step = sqlite3_step(st)) == SQLITE_ROW) gather(&values, row_value(st));
    require("the rows read to the end", step == SQLITE_DONE);
    sqlite3_finalize(st);
    return gathered_expr(&values);
}

/* The first alpha-identical row removed; false when none is. */
static bool removed(store *s, const mt_atom *value)
{
    if (!s || !s->db) return false;
    sqlite3_stmt *st = rows_of(s), *drop = NULL;
    sqlite3_int64 row = 0;
    bool found = false;
    int step = SQLITE_DONE;
    while (!found && (step = sqlite3_step(st)) == SQLITE_ROW) {
        mt_atom *stored = row_value(st);
        found = mt_alpha_eq(stored, value);
        row = sqlite3_column_int64(st, 0);
        mt_drop(stored);
    }
    require("the rows read to the end", found || step == SQLITE_DONE);
    sqlite3_finalize(st);
    if (found)
        require("the row is removed", sqlite3_prepare_v2(s->db, "DELETE FROM atoms WHERE rowid = ?", -1, &drop, NULL) == SQLITE_OK &&
                                          sqlite3_bind_int64(drop, 1, row) == SQLITE_OK && sqlite3_step(drop) == SQLITE_DONE);
    sqlite3_finalize(drop);
    return found;
}

/* SQLite finishes every statement, so a sync has nothing left to flush and
   keeps the lock; true while the store is open. */
static bool synced(store *s) { return s && s->db; }

/* Close releases the lock once, however often it is asked. */
static bool closed(store *s)
{
    if (s && s->db) require("the store closes", sqlite3_close(s->db) == SQLITE_OK);
    if (s) s->db = NULL;
    return true;
}

/* The matcher the original's segment lets and unify use, the engine's
   metta_match_atoms/2 over two sides, a variable known by its side and name
   so pattern and stored variables stay apart. A variable binds, expressions
   match pointwise, and two leaves match when identical or, both numbers,
   when =:= holds; identity comes first, as the engine tries L == R before
   anything else, so a NaN matches itself though NaN =:= NaN fails. A
   pattern's (:= X) is lifted out first, to a position that matches anything
   and a ==/2 guard settled after the whole match
   [source: engine/spaces/bounded_matching.pl, metta_match_atoms/2, and
   engine/translator/runtime.pl, seam:pattern_modifier/3;
   commit=2803a3ecf877ad410ab19a9168e09096eb50ef5a]. Bindings and guards are
   only ever appended, so a search backtracks by cutting both back to a mark,
   as the WAM unwinds its trail to the boundary a choice point saved
   [source: Hassan Ait-Kaci, "Warren's Abstract Machine: A Tutorial
   Reconstruction", MIT Press 1991, section 4.2]. */
typedef struct term {
    const mt_atom *atom;
    int side;
} term;

typedef struct binding {
    int side;
    const char *name;
    term value;
} binding;

/* A lifted (:= X): the value at its position must be identical to X. */
typedef struct guard {
    term wanted, value;
} guard;

typedef struct env {
    binding *at;
    size_t n, cap;
    guard *guards;
    size_t n_guards, cap_guards;
} env;

/* How far an env had got, to cut it back to. */
typedef struct mark {
    size_t n, n_guards;
} mark;

static mark marked(const env *e) { return (mark){ e->n, e->n_guards }; }
static void undo(env *e, mark m) { e->n = m.n, e->n_guards = m.n_guards; }
static void forget(env *e) { free(e->at), free(e->guards); }

/* A variable's value under the bindings, followed to its end. Time: at most
   n binding comparisons per step of the chain, n = bindings. */
static term walk(const env *e, term t)
{
    for (size_t i = 0; mt_kind_of(t.atom) == MT_VARIABLE && i < e->n;)
        if (e->at[i].side == t.side && strcmp(e->at[i].name, mt_name(t.atom)) == 0) t = e->at[i].value, i = 0;
        else i++;
    return t;
}

static bool same_variable(term a, term b) { return a.side == b.side && strcmp(mt_name(a.atom), mt_name(b.atom)) == 0; }

/* ==/2 under the bindings: an unbound variable is identical only to itself,
   and 1 and 1.0 differ. */
static bool identical(const env *e, term a, term b)
{
    a = walk(e, a);
    b = walk(e, b);
    bool av = mt_kind_of(a.atom) == MT_VARIABLE, bv = mt_kind_of(b.atom) == MT_VARIABLE;
    if (av || bv) return av && bv && same_variable(a, b);
    if (mt_kind_of(a.atom) != MT_EXPR || mt_kind_of(b.atom) != MT_EXPR) return mt_eq(a.atom, b.atom);
    if (mt_len(a.atom) != mt_len(b.atom)) return false;
    for (size_t i = 0; i < mt_len(a.atom); i++)
        if (!identical(e, (term){ mt_at(a.atom, i), a.side }, (term){ mt_at(b.atom, i), b.side })) return false;
    return true;
}

/* =:= as SWI decides it: two exact numbers by exact value, and with a float
   on either side both as the nearest double, ties to even, which is SWI's
   float_rounding=to_nearest [measured on the patched host, 2026-09-24:
   2**70+2**17+1 =:= 2.0**70+2.0**18 and 1r10 =:= 0.1 hold, and
   2**70+2**17+1 =:= 2.0**70 does not]. */
static bool same_number(const mt_atom *a, const mt_atom *b)
{
    if (mt_kind_of(a) != MT_FLOAT && mt_kind_of(b) != MT_FLOAT) return mt_compare(a, b) == 0;
    return nearest(a) == nearest(b);
}

/* An expression whose head is the symbol `symbol`. */
static bool headed(const mt_atom *x, const char *symbol)
{
    return mt_kind_of(x) == MT_EXPR && mt_len(x) > 0 && mt_kind_of(mt_at(x, 0)) == MT_SYMBOL && strcmp(mt_name(mt_at(x, 0)), symbol) == 0;
}

/* X of a (:= X) the pattern wrote, the symbol := and one operand; NULL for
   anything else, a stored (:= X) included. */
static const mt_atom *modifier(term t)
{
    return t.side == 0 && headed(t.atom, ":=") && mt_len(t.atom) == 2 ? mt_at(t.atom, 1) : NULL;
}

static bool unify(env *e, term a, term b)
{
    const mt_atom *wanted = modifier(a);
    if (wanted) {
        e->guards = room(e->guards, &e->cap_guards, e->n_guards, sizeof *e->guards);
        e->guards[e->n_guards++] = (guard){ { wanted, 0 }, b };
        return true;
    }
    a = walk(e, a);
    b = walk(e, b);
    bool av = mt_kind_of(a.atom) == MT_VARIABLE, bv = mt_kind_of(b.atom) == MT_VARIABLE;
    if (av && bv && same_variable(a, b)) return true;
    if (av || bv) {
        e->at = room(e->at, &e->cap, e->n, sizeof *e->at);
        term var = av ? a : b, value = av ? b : a;
        e->at[e->n++] = (binding){ var.side, mt_name(var.atom), value };
        return true;
    }
    if (mt_kind_of(a.atom) == MT_EXPR && mt_kind_of(b.atom) == MT_EXPR) {
        if (mt_len(a.atom) != mt_len(b.atom)) return false;
        for (size_t i = 0; i < mt_len(a.atom); i++)
            if (!unify(e, (term){ mt_at(a.atom, i), a.side }, (term){ mt_at(b.atom, i), b.side })) return false;
        return true;
    }
    return mt_eq(a.atom, b.atom) || (is_number(a.atom) && is_number(b.atom) && same_number(a.atom, b.atom));
}

/* The guards of a finished match, each under all of its bindings. */
static bool settled(const env *e)
{
    for (size_t i = 0; i < e->n_guards; i++)
        if (!identical(e, e->guards[i].wanted, e->guards[i].value)) return false;
    return true;
}

/* A term with every bound variable replaced. */
static mt_atom *resolved(const env *e, term t)
{
    t = walk(e, t);
    if (mt_kind_of(t.atom) != MT_EXPR) return mt_keep(t.atom);
    mt_atom **kids = malloc((mt_len(t.atom) + 1) * sizeof *kids);
    require("room for the term", kids != NULL);
    for (size_t i = 0; i < mt_len(t.atom); i++) kids[i] = resolved(e, (term){ mt_at(t.atom, i), t.side });
    mt_atom *out = mt_exprv(mt_len(t.atom), kids);
    free(kids);
    return out;
}

/* What (let ((:seg $b) P (:seg $a)) snapshot (quote T)) collapses to: T for
   every element P unifies with, in order. */
static mt_atom *projected(const mt_atom *all, const mt_atom *pattern, const mt_atom *template)
{
    gathered out = { 0 };
    env e = { 0 };
    for (size_t i = 0; i < mt_len(all); i++) {
        undo(&e, (mark){ 0, 0 });
        if (unify(&e, (term){ pattern, 0 }, (term){ mt_at(all, i), 1 }) && settled(&e)) gather(&out, resolved(&e, (term){ template, 0 }));
    }
    forget(&e);
    return gathered_expr(&out);
}

/* Every split of each (head x ...) element's tail into a prefix and a
   suffix, the two gaps of (head (:seg $left) (:seg $right)). */
static mt_atom *splits(const mt_atom *all, const char *head)
{
    gathered out = { 0 };
    for (size_t i = 0; i < mt_len(all); i++) {
        const mt_atom *x = mt_at(all, i);
        if (!headed(x, head)) continue;
        size_t tail = mt_len(x) - 1;
        mt_atom **parts = malloc((tail + 1) * sizeof *parts);
        require("room for a split", parts != NULL);
        for (size_t k = 0; k <= tail; k++) {
            for (size_t j = 0; j < tail; j++) parts[j] = mt_keep(mt_at(x, 1 + j));
            gather(&out, E(mt_exprv(k, parts), mt_exprv(tail - k, parts + k)));
        }
        free(parts);
    }
    return gathered_expr(&out);
}

/* (supports L R), (edge L S M) and (edge R M T) over one snapshot: (S M T)
   for every way the three patterns agree. */
static mt_atom *joined(const mt_atom *all)
{
    mt_atom *supports = E("supports", V("left"), V("right")), *first = E("edge", V("left"), V("source"), V("middle")),
            *second = E("edge", V("right"), V("middle"), V("target")), *path = E(V("source"), V("middle"), V("target"));
    gathered out = { 0 };
    env e = { 0 };
    for (size_t i = 0; i < mt_len(all); i++) {
        undo(&e, (mark){ 0, 0 });
        if (!unify(&e, (term){ supports, 0 }, (term){ mt_at(all, i), 1 })) continue;
        mark after_supports = marked(&e);
        for (size_t j = 0; j < mt_len(all); j++) {
            undo(&e, after_supports);
            if (!unify(&e, (term){ first, 0 }, (term){ mt_at(all, j), 1 })) continue;
            mark after_first = marked(&e);
            for (size_t k = 0; k < mt_len(all); k++) {
                undo(&e, after_first);
                if (unify(&e, (term){ second, 0 }, (term){ mt_at(all, k), 1 }) && settled(&e)) gather(&out, resolved(&e, (term){ path, 0 }));
            }
        }
    }
    forget(&e);
    mt_atom *held[] = { supports, first, second, path };
    for (size_t i = 0; i < 4; i++) mt_drop(held[i]);
    return gathered_expr(&out);
}

/* C's arithmetic over stored syntax: + over exact numbers, summed in GMP so
   a total past int64 stays exact, as the engine's does. */
static void arithmetic(mpq_t out, const mt_atom *a)
{
    if (is_number(a) && mt_kind_of(a) != MT_FLOAT) {
        exact(out, a);
        return;
    }
    require("a sum", headed(a, "+") && mt_len(a) == 3);
    mpq_t right;
    mpq_init(right);
    arithmetic(out, mt_at(a, 1));
    arithmetic(right, mt_at(a, 2));
    mpq_add(out, out, right);
    mpq_clear(right);
}

/* Each stored (= (stored-twice P) B) whose P unifies with `argument`, B's
   value; an unbound argument asks whether P is still a variable. */
static mt_atom *applied(const mt_atom *all, const mt_atom *argument, bool ask_variable)
{
    mt_atom *pattern = E("=", E("stored-twice", V("parameter")), V("body"));
    gathered out = { 0 };
    env e = { 0 };
    for (size_t i = 0; i < mt_len(all); i++) {
        undo(&e, (mark){ 0, 0 });
        if (!unify(&e, (term){ pattern, 0 }, (term){ mt_at(all, i), 1 }) || !settled(&e)) continue;
        if (ask_variable) {
            gather(&out, B(mt_kind_of(walk(&e, (term){ mt_at(mt_at(pattern, 1), 1), 0 }).atom) == MT_VARIABLE));
            continue;
        }
        require("the argument binds", unify(&e, (term){ mt_at(mt_at(pattern, 1), 1), 0 }, (term){ argument, 0 }));
        mt_atom *body = resolved(&e, (term){ mt_at(pattern, 2), 0 });
        mpq_t total;
        mpq_init(total);
        arithmetic(total, body);
        gather(&out, rational_of(total));
        mpq_clear(total);
        mt_drop(body);
    }
    forget(&e);
    mt_drop(pattern);
    return gathered_expr(&out);
}

/* Whether a store opens; one that does is closed again at once. */
static bool opens(const char *dir, const char *policy)
{
    store *s = opened(dir, policy);
    if (s) closed(s), free(s);
    return s != NULL;
}

/* A scope: open, apply, close whatever happens; false when opening or the
   callback refuses. */
typedef bool scope_fn(store *s, gathered *answers);

static bool scoped(const char *dir, const char *policy, scope_fn *fn, gathered *answers)
{
    store *s = opened(dir, policy);
    if (!s) return false;
    bool fine = fn(s, answers);
    closed(s);
    free(s);
    return fine;
}

static bool items(store *s, gathered *answers)
{
    mt_atom *all = snapshot(s), *pattern = E("item", V("name"), V("price")), *template = E(V("name"), V("price"));
    gather(answers, projected(all, pattern, template));
    mt_drop(all), mt_drop(pattern), mt_drop(template);
    return true;
}

static bool left_and_right(store *s, gathered *answers)
{
    (void)s;
    gather(answers, S("left"));
    gather(answers, S("right"));
    return true;
}

static bool no_answers(store *s, gathered *answers)
{
    (void)s, (void)answers;
    return true;
}

static bool store_itself(store *s, gathered *answers)
{
    (void)answers;
    mt_atom *self = mt_object(s, "store", NULL), *bad = E("bad", self);
    bool fine = added(s, bad);
    mt_drop(bad);
    return fine;
}

static bool close_early(store *s, gathered *answers)
{
    gather(answers, B(closed(s)));
    return true;
}

static bool everything(store *s, gathered *answers)
{
    gather(answers, snapshot(s));
    return true;
}

static bool original(store *s, gathered *answers)
{
    mt_atom *value = S("original");
    gather(answers, B(added(s, value)));
    mt_drop(value);
    return true;
}

static mt_atom *scope_answers(const char *dir, const char *policy, scope_fn *fn)
{
    gathered answers = { 0 };
    require("the scope ran", scoped(dir, policy, fn, &answers));
    return gathered_expr(&answers);
}

static int unlink_one(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void)st, (void)flag, (void)ftw;
    return remove(path);
}

static bool exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* The engine's (collapse (let ((:seg $before) P (:seg $after)) G (quote T))). */
static mt_atom *projection(const mt_atom *pattern, mt_atom *snapshot_goal, const mt_atom *template)
{
    return E("collapse", E("let", E(E(":seg", V("before")), mt_keep(pattern), E(":seg", V("after"))), snapshot_goal, E("quote", mt_keep(template))));
}

static mt_atom *atoms_of(const mt_atom *handle) { return E("database-atoms", mt_keep(handle)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    require("import lib_database", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_database")))));
    char joined_path[PATH_MAX];
    join(joined_path, "store", "journal.pl");
    assert(answers_are(mt_eval(m, E("path-join", T("store"), T("journal.pl"))), E(T(joined_path))) && "a path join");

    /* Two stores, the engine's and C's side by side. */
    mt_atom *work = value_of(m, E("temp-dir!", T("database-lib")));
    char c_work[PATH_MAX], parent[PATH_MAX], first_path[PATH_MAX], second_path[PATH_MAX], c_first_path[PATH_MAX], c_second_path[PATH_MAX];
    snprintf(parent, sizeof parent, "%s", mt_name(work));
    snprintf(c_work, sizeof c_work, "%s/database-lib-c-XXXXXX", dirname(parent));
    require("C mints its own directory", mkdtemp(c_work) != NULL);
    join(first_path, mt_name(work), "first");
    join(second_path, mt_name(work), "second");
    join(c_first_path, c_work, "first");
    join(c_second_path, c_work, "second");
    mt_atom *first = value_of(m, E("database-open!", T(first_path), "none")), *second = value_of(m, E("database-open!", T(second_path), "close"));
    store *c_first = opened(c_first_path, "none"), *c_second = opened(c_second_path, "close");
    require("C opens its stores", c_first && c_second);
    mt_atom *all = snapshot(c_first);
    assert(answers_are(mt_eval(m, atoms_of(first)), E(all)) && "a new store is empty");
    assert(answers_are(guarded(m, E("database-open!", T(first_path), "flush")), E(verdict(opens(c_first_path, "flush")))) && "a second owner is refused");

    /* Values in order, duplicates kept, selected by unification. */
    mt_atom *apple = E("item", "apple", 3), *pear = E("item", "pear", 5), *item = E("item", V("name"), V("price")),
            *name_price = E(V("name"), V("price"));
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(apple))), E(B(added(c_first, apple)))) && "added");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(pear))), E(B(added(c_first, pear)))) && "added again");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(apple))), E(B(added(c_first, apple)))) && "a duplicate kept");
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(item, atoms_of(first), name_price)), E(projected(all, item, name_price))) && "the items in order");
    mt_atom *priced_3 = E("item", V("name"), 3), *name_only = V("name");
    assert(answers_are(mt_eval(m, projection(priced_3, atoms_of(first), name_only)), E(projected(all, priced_3, name_only))) && "a constant selects");
    mt_drop(all);
    assert(answers_are(mt_eval(m, E("database-remove!", mt_keep(first), mt_keep(apple))), E(B(removed(c_first, apple)))) && "one occurrence removed");
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(item, atoms_of(first), name_price)), E(projected(all, item, name_price))) && "the first one");
    mt_atom *missing = E("item", "missing", 0), *missing_value = E("missing", V("value")), *value = V("value");
    assert(answers_are(mt_eval(m, E("database-remove!", mt_keep(first), mt_keep(missing))), E(B(removed(c_first, missing)))) && "nothing to remove");
    assert(answers_are(mt_eval(m, projection(missing_value, atoms_of(first), value)), E(projected(all, missing_value, value))) && "nothing to select");
    mt_atom *pear_constraint = E("item", E(":=", "pear"), V("price")), *price = V("price"), *row = V("row");
    mt_atom *engine_goal = E("collapse", E("let", E(E(":seg", V("before")), V("row"), E(":seg", V("after"))), atoms_of(first),
                                            E("let", B(true), E("unify", mt_keep(pear_constraint), V("row"), B(true), B(false)), E("quote", V("price")))));
    assert(answers_are(mt_eval(m, engine_goal), E(projected(all, pear_constraint, price))) && "a constraint selects");
    mt_drop(all);

    /* The second store is its own. */
    mt_atom *banana = E("item", "banana", 8);
    all = snapshot(c_second);
    assert(answers_are(mt_eval(m, atoms_of(second)), E(all)) && "the second is empty");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(second), mt_keep(banana))), E(B(added(c_second, banana)))) && "added to it");
    all = snapshot(c_second);
    assert(answers_are(mt_eval(m, projection(item, atoms_of(second), name_price)), E(projected(all, item, name_price))) && "its items");
    mt_drop(all);
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(item, atoms_of(first), name_price)), E(projected(all, item, name_price))) && "the first unchanged");
    mt_drop(all);

    /* Held syntax, numbers matched by value, texts with NUL, gaps. */
    mt_atom *sum = E("+", 1, 2), *sum_pattern = E("+", 1, V("value"));
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(sum))), E(B(added(c_first, sum)))) && "a sum held");
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(sum_pattern, atoms_of(first), value)), E(projected(all, sum_pattern, value))) && "and selected");
    mt_drop(all);
    mt_atom *one = E("number", 1), *one_float = E("number", 1.0), *hit = S("hit"), *number_value = E("number", V("value"));
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(one))), E(B(added(c_first, one)))) && "an integer");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(one_float))), E(B(added(c_first, one_float)))) && "a float");
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(one, atoms_of(first), hit)), E(projected(all, one, hit))) && "both match by value");
    mt_drop(all);
    assert(answers_are(mt_eval(m, E("database-remove!", mt_keep(first), mt_keep(one))), E(B(removed(c_first, one)))) && "but 1 removes only 1");
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(number_value, atoms_of(first), value)), E(projected(all, number_value, value))) && "so 1.0 stays");
    mt_drop(all);
    static const unsigned char nul_bytes[] = { 'a', '\0', 'b' };
    mt_atom *nul = mt_textn((const char *)nul_bytes, sizeof nul_bytes);
    assert(answers_are(mt_eval(m, E("string-codes", mt_keep(nul))), E(mt_array(sizeof nul_bytes, nul_bytes))) && "a NUL in a text");
    mt_atom *texts = E("text", T("café"), mt_keep(nul), mt_unit()), *texts_pattern = E("text", V("left"), V("right"), mt_unit()),
            *left_right = E(V("left"), V("right"));
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(texts))), E(B(added(c_first, texts)))) && "texts held");
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(texts_pattern, atoms_of(first), left_right)), E(projected(all, texts_pattern, left_right))) && "and read back whole");
    mt_drop(all);
    mt_atom *path = E("path", "a", "b", "c");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(path))), E(B(added(c_first, path)))) && "a path held");
    all = snapshot(c_first);
    mt_atom *gaps = E("path", E(":seg", V("left")), E(":seg", V("right")));
    assert(answers_are(mt_eval(m, projection(gaps, atoms_of(first), left_right)), E(splits(all, "path"))) && "every split of its gaps");
    mt_drop(all);
    mt_atom *unit = mt_unit(), *empty_value = S("empty-value");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_unit())), E(B(added(c_first, unit)))) && "an empty expression held");
    all = snapshot(c_first);
    assert(answers_are(mt_eval(m, projection(unit, atoms_of(first), empty_value)), E(projected(all, unit, empty_value))) && "and selected");
    mt_drop(all);
    mt_atom *unbound = E("unbound", V("value")), *renamed = E("unbound", V("renamed"));
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(unbound))), E(B(added(c_first, unbound)))) && "a variable held");
    assert(answers_are(mt_eval(m, E("database-remove!", mt_keep(first), mt_keep(renamed))), E(B(removed(c_first, renamed)))) && "removed under another name");
    assert(answers_are(guarded(m, E("database-add!", mt_keep(first), mt_keep(second))), E(verdict(added(c_first, second)))) && "a store is no value");

    /* Equations are data. */
    mt_atom *twice = E("=", E("stored-twice", V("x")), E("+", V("x"), V("x"))),
            *twice_plus = E("=", E("stored-twice", V("x")), E("+", E("+", V("x"), V("x")), 1));
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(twice))), E(B(added(c_first, twice)))) && "an equation held");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(twice_plus))), E(B(added(c_first, twice_plus)))) && "and another");
    gathered bodies = { 0 };
    mt_rows (found, mt_match(m, E("=", E("stored-twice", V("x")), V("body")))) gather(&bodies, mt_keep(mt_bound(found, "body")));
    assert(answers_are(mt_eval(m, E("collapse", E("match", "&self", E("=", E("stored-twice", V("x")), V("body")), V("body")))), E(gathered_expr(&bodies)))
           && "neither reached the space");

    /* A join is three patterns over one snapshot. */
    mt_atom *edge1 = E("edge", "e1", "alice", "bob"), *edge2 = E("edge", "e2", "bob", "carol"), *supports = E("supports", "e1", "e2");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(edge1))), E(B(added(c_first, edge1)))) && "an edge");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(edge2))), E(B(added(c_first, edge2)))) && "another");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(supports))), E(B(added(c_first, supports)))) && "an edge about edges");
    all = snapshot(c_first);
    mt_atom *join_goal = E("let", V("graph"), atoms_of(first),
                           E("collapse", E("let", E(E(":seg", V("before")), E("supports", V("left"), V("right")), E(":seg", V("after"))), E("quote", V("graph")),
                                           E("let", E(E(":seg", V("a")), E("edge", V("left"), V("source"), V("middle")), E(":seg", V("b"))), E("quote", V("graph")),
                                             E("let", E(E(":seg", V("c")), E("edge", V("right"), V("middle"), V("target")), E(":seg", V("d"))), E("quote", V("graph")),
                                               E("quote", E(V("source"), V("middle"), V("target"))))))));
    assert(answers_are(mt_eval(m, join_goal), E(joined(all))) && "the join");
    mt_drop(all);

    /* Sync keeps the lock; close releases it. */
    mt_atom *after_sync = S("after-sync");
    assert(answers_are(mt_eval(m, E("database-sync!", mt_keep(first))), E(B(synced(c_first)))) && "synced");
    char journal[PATH_MAX], c_journal[PATH_MAX];
    join(journal, first_path, "journal.pl");
    join(c_journal, c_first_path, journal_name);
    assert(answers_are(mt_eval(m, E("file-exists", T(journal))), E(B(exists(c_journal)))) && "the journal exists");
    assert(answers_are(guarded(m, E("database-open!", T(first_path), "close")), E(verdict(opens(c_first_path, "close")))) && "still locked");
    assert(answers_are(mt_eval(m, E("database-add!", mt_keep(first), mt_keep(after_sync))), E(B(added(c_first, after_sync)))) && "written after the sync");
    assert(answers_are(mt_eval(m, E("database-close!", mt_keep(first))), E(B(closed(c_first)))) && "closed");
    assert(answers_are(mt_eval(m, E("database-close!", mt_keep(first))), E(B(closed(c_first)))) && "closed again");
    assert(answers_are(mt_eval(m, E("database-close!", mt_keep(second))), E(B(closed(c_second)))) && "the second closed");
    all = snapshot(c_first);
    assert(answers_are(guarded(m, atoms_of(first)), E(verdict(all != NULL))) && "a closed store answers nothing");

    /* Reopened, the journal replays. */
    mt_atom *reopened = value_of(m, E("database-open!", T(first_path), "flush"));
    store *c_reopened = opened(c_first_path, "flush");
    require("C reopens its store", c_reopened != NULL);
    all = snapshot(c_reopened);
    assert(answers_are(mt_eval(m, projection(item, atoms_of(reopened), name_price)), E(projected(all, item, name_price))) && "the items replayed");
    assert(answers_are(mt_eval(m, projection(texts_pattern, atoms_of(reopened), left_right)), E(projected(all, texts_pattern, left_right))) && "the texts replayed");
    mt_atom *present = S("present");
    assert(answers_are(mt_eval(m, projection(after_sync, atoms_of(reopened), present)), E(projected(all, after_sync, present))) && "the write after the sync");
    mt_atom *equation = E("=", E("stored-twice", V("parameter")), V("body"));
    mt_atom *applied_goal = E("collapse", E("let", E(E(":seg", V("before")), mt_keep(equation), E(":seg", V("after"))), atoms_of(reopened),
                                             E("let", V("syntax"), E("quote", E("|->", E(V("parameter")), V("body"))), E("let", V("function"), E("eval", V("syntax")), E(V("function"), 7)))));
    mt_atom *seven = mt_num(7), *nine = mt_num(9), *four = mt_num(4);
    assert(answers_are(mt_eval(m, applied_goal), E(applied(all, seven, false))) && "each equation a function");
    mt_atom *at_nine = E("=", E("stored-twice", 9), V("body"));
    assert(answers_are(mt_eval(m, E("collapse", E("let", E(E(":seg", V("before")), mt_keep(at_nine), E(":seg", V("after"))), atoms_of(reopened), E("eval", V("body"))))), E(applied(all, nine, false)))
           && "each specialized");
    assert(answers_are(mt_eval(m, E("collapse", E("let", E(E(":seg", V("before")), mt_keep(equation), E(":seg", V("after"))), atoms_of(reopened), E("is-var", V("parameter"))))), E(applied(all, seven, true)))
           && "with its own variable");
    mt_drop(all);
    mt_atom *fresh = E("=", E("stored-twice", V("fresh")), E("+", V("fresh"), V("fresh")));
    assert(answers_are(mt_eval(m, E("database-remove!", mt_keep(reopened), mt_keep(fresh))), E(B(removed(c_reopened, fresh)))) && "an equation removed under other names");
    all = snapshot(c_reopened);
    mt_atom *at_four = E("=", E("stored-twice", 4), V("body"));
    assert(answers_are(mt_eval(m, E("collapse", E("let", E(E(":seg", V("before")), mt_keep(at_four), E(":seg", V("after"))), atoms_of(reopened), E("eval", V("body"))))), E(applied(all, four, false)))
           && "the other remains");
    mt_drop(all);
    assert(answers_are(mt_eval(m, E("database-close!", mt_keep(reopened))), E(B(closed(c_reopened)))) && "the reopened store closed");
    free(c_reopened);

    /* Scopes close their store however the callback ends. */
    mt_atom *store_var = E(V("store")), *answered = scope_answers(c_first_path, "flush", items);
    assert(answers_are(mt_eval(m, E("with-database", T(first_path), "flush",
                                    E("|->", mt_keep(store_var), projection(item, E("database-atoms", V("store")), name_price)))), E(mt_keep(mt_at(answered, 0))))
           && "a scope's answer");
    mt_drop(answered);
    mt_atom *two = scope_answers(c_first_path, "none", left_and_right);
    assert(answers_are(mt_eval(m, E("collapse", E("with-database", T(first_path), "none", E("|->", mt_keep(store_var), E("superpose", E("left", "right")))))), E(two)) && "every answer");
    assert(answers_are(mt_eval(m, E("collapse", E("with-database", T(first_path), "none", E("|->", mt_keep(store_var), E("superpose", mt_unit()))))), E(scope_answers(c_first_path, "none", no_answers)))
           && "no answer");
    gathered none = { 0 };
    assert(answers_are(guarded(m, E("with-database", T(first_path), "none", E("|->", mt_keep(store_var), E("database-add!", V("store"), E("bad", V("store")))))), E(verdict(scoped(c_first_path, "none", store_itself, &none))))
           && "the store is no value inside either");
    mt_drop(gathered_expr(&none));
    mt_atom *early = scope_answers(c_first_path, "close", close_early);
    assert(answers_are(mt_eval(m, E("with-database", T(first_path), "close", E("|->", mt_keep(store_var), E("database-close!", V("store"))))), E(mt_keep(mt_at(early, 0))))
           && "a scope may close early");
    mt_drop(early);
    mt_atom *seconds = scope_answers(c_second_path, "close", everything);
    assert(answers_are(mt_eval(m, E("with-database", T(second_path), "close", E("|->", mt_keep(store_var), E("database-atoms", V("store"))))), E(mt_keep(mt_at(seconds, 0))))
           && "the second store's values");
    mt_drop(seconds);
    char invalid[PATH_MAX], c_invalid[PATH_MAX];
    join(invalid, mt_name(work), "invalid");
    join(c_invalid, c_work, "invalid");
    assert(answers_are(guarded(m, E("database-open!", T(invalid), "invented")), E(verdict(opens(c_invalid, "invented")))) && "a policy with no name");
    assert(answers_are(mt_eval(m, E("dir-exists", T(invalid))), E(B(exists(c_invalid)))) && "creates nothing");

    /* A damaged store is refused whole, its bytes kept for repair. */
    char bad[PATH_MAX], c_bad[PATH_MAX], engine_journal[PATH_MAX], c_bad_journal[PATH_MAX];
    join(bad, mt_name(work), "bad");
    join(c_bad, c_work, "bad");
    join(engine_journal, bad, "journal.pl");
    join(c_bad_journal, c_bad, journal_name);
    mt_atom *stored = scope_answers(c_bad, "close", original);
    assert(answers_are(mt_eval(m, E("with-database", T(bad), "close", E("|->", mt_keep(store_var), E("database-add!", V("store"), "original")))), E(mt_keep(mt_at(stored, 0))))
           && "a store with one value");
    mt_drop(stored);
    static const char damaged[] = "created(0).\nassert(row(original)).\ninvented(corruption).\n";
    FILE *f = fopen(c_bad_journal, "wb");
    bool written = f && fputs(damaged, f) >= 0;
    written = f && fclose(f) == 0 && written;
    assert(answers_are(mt_eval(m, E("write-file!", T(engine_journal), T(damaged))), E(B(written))) && "damaged");
    assert(answers_are(guarded(m, E("database-open!", T(bad), "flush")), E(verdict(opens(c_bad, "flush")))) && "refused whole");
    char kept[sizeof damaged];
    f = fopen(c_bad_journal, "rb");
    size_t got = f ? fread(kept, 1, sizeof kept - 1, f) : 0;
    if (f) fclose(f);
    kept[got] = '\0';
    assert(answers_are(mt_eval(m, E("read-file!", T(engine_journal))), E(T(kept))) && "its bytes kept");
    assert(answers_are(mt_eval(m, E("delete-tree!", mt_keep(work))), E(B(nftw(c_work, unlink_one, 16, FTW_DEPTH | FTW_PHYS) == 0))) && "the directory removed");

    free(c_first);
    free(c_second);
    mt_atom *held[] = { work, first, second, apple, pear, item, name_price, priced_3, name_only, missing, missing_value, value, pear_constraint, price,
                        row, banana, sum, sum_pattern, one, one_float, hit, number_value, nul, texts, texts_pattern, left_right, path, gaps, unit,
                        empty_value, unbound, renamed, twice, twice_plus, edge1, edge2, supports, after_sync, reopened, present, equation, seven,
                        nine, four, at_nine, fresh, at_four, store_var };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
#else
#include <stdio.h>

/* Without GMP and SQLite's headers the program only says what it needs. */
int main(void)
{
    fputs("42-database_lib.c needs GMP and SQLite: install its development files, then build with\n"
          "cc 42-database_lib.c $(pkg-config --cflags --libs cmetta gmp sqlite3)\n", stderr);
    return 77;
}
#endif
