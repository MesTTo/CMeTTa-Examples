/* Purpose: moving a file handle's cursor, and asking how big the file is
 *   rather than how much is left. C sequences each step itself, holding the
 *   path and the handle lib_file answers, and keeps its own model of the
 *   cursor over its copy of the text: the size is the text's length and
 *   never moves, a read to the end answers the text from the cursor and
 *   leaves it at the end, a seek sets it, and an exact read takes at most n
 *   characters from it. Every answer the handle gives is held to the
 *   model's.
 * Guarantees: all five claims of the original hold, each tuple checked part
 *   by part [tested 2026-09-27T00:35:58+10:00:
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

static const char *const text = "hello world";

/* C's model of a read cursor over TEXT. */
typedef struct cursor {
    size_t at, size;
} cursor;

static mt_atom *read_to_end(cursor *c)
{
    mt_atom *rest = T(text + c->at);
    c->at = c->size;
    return rest;
}

static mt_atom *read_exact(cursor *c, size_t n)
{
    size_t taken = c->size - c->at < n ? c->size - c->at : n;
    char *piece = malloc(taken + 1);
    require("room", piece != NULL);
    memcpy(piece, text + c->at, taken);
    piece[taken] = '\0';
    c->at += taken;
    mt_atom *read = T(piece);
    free(piece);
    return read;
}

/* One file holding the text, opened for reading. */
typedef struct opened {
    mt_atom *path, *handle;
    cursor model;
} opened;

static opened open_text(metta *m)
{
    opened o = { .model = { 0, strlen(text) } };
    o.path = mt_first(mt_eval(m, E("temp-path!", T("seek"))));
    require("a temporary path", o.path != NULL);
    mt_list_free(mt_all(mt_eval(m, E("write-file!", mt_keep(o.path), T(text)))));
    require("write the text", mt_ok());
    o.handle = mt_first(mt_eval(m, E("file-open!", mt_keep(o.path), T("r"))));
    require("a handle", o.handle != NULL);
    return o;
}

static void close_text(metta *m, opened *o)
{
    mt_list_free(mt_all(mt_eval(m, E("file-close!", o->handle))));
    mt_list_free(mt_all(mt_eval(m, E("delete-file!", o->path))));
    require("close and delete", mt_ok());
}

static void size_is(metta *m, const char *claim, opened *o)
{
    assert(mt_one_int(mt_eval(m, E("file-get-size!", mt_keep(o->handle)))) == (int64_t)o->model.size && claim);
}

static void reads_to_end(metta *m, const char *claim, opened *o)
{
    assert(answers_are(mt_eval(m, E("file-read-to-string!", mt_keep(o->handle))), E(read_to_end(&o->model))) && claim);
}

static void seek(metta *m, opened *o, size_t at)
{
    mt_list_free(mt_all(mt_eval(m, E("file-seek!", mt_keep(o->handle), (int64_t)at))));
    require("seek", mt_ok());
    o->model.at = at;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));

    opened o = open_text(m);
    size_is(m, "the size before reading", &o);
    reads_to_end(m, "the whole text", &o);
    size_is(m, "and the same size after", &o);
    close_text(m, &o);

    o = open_text(m);
    reads_to_end(m, "a first read", &o);
    seek(m, &o, 0);
    reads_to_end(m, "and a second after rewinding", &o);
    close_text(m, &o);

    o = open_text(m);
    seek(m, &o, 6);
    reads_to_end(m, "the tail from the middle", &o);
    close_text(m, &o);

    o = open_text(m);
    size_is(m, "the size", &o);
    seek(m, &o, o.model.size);
    reads_to_end(m, "nothing left at the end", &o);
    size_is(m, "and the size unchanged", &o);
    close_text(m, &o);

    o = open_text(m);
    assert(answers_are(mt_eval(m, E("file-read-exact!", mt_keep(o.handle), 5)), E(read_exact(&o.model, 5)))
           && "an exact read takes n characters");
    seek(m, &o, 6);
    assert(answers_are(mt_eval(m, E("file-read-exact!", mt_keep(o.handle), 5)), E(read_exact(&o.model, 5)))
           && "and another from where the seek put it");
    close_text(m, &o);
    mt_close(m);
    return 0;
}
