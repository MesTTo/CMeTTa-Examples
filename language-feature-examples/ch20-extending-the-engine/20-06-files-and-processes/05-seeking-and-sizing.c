/* Purpose: moving a file handle's cursor, and asking how big the file is
 *   rather than how much is left. C sequences each step itself, holding the
 *   path and the handle lib_file answers, and keeps its own model of the
 *   cursor over its copy of the text: the size is the text's length and
 *   never moves, a read to the end answers the text from the cursor and
 *   leaves it at the end, a seek sets it, and an exact read takes at most n
 *   characters from it. Every answer the handle gives is held to the
 *   model's.
 * Guarantees: all five claims of the original hold, each tuple checked part
 *   by part [tested: make twins; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

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
    check_int(claim, mt_one_int(mt_eval(m, E("file-get-size!", mt_keep(o->handle)))), (int64_t)o->model.size);
}

static void reads_to_end(metta *m, const char *claim, opened *o)
{
    check_answers(claim, mt_eval(m, E("file-read-to-string!", mt_keep(o->handle))), read_to_end(&o->model));
}

static void seek(metta *m, opened *o, size_t at)
{
    mt_list_free(mt_all(mt_eval(m, E("file-seek!", mt_keep(o->handle), (int64_t)at))));
    require("seek", mt_ok());
    o->model.at = at;
}

int main(void)
{
    metta *m = open_engine();
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
    check_answers("an exact read takes n characters", mt_eval(m, E("file-read-exact!", mt_keep(o.handle), 5)),
                  read_exact(&o.model, 5));
    seek(m, &o, 6);
    check_answers("and another from where the seek put it", mt_eval(m, E("file-read-exact!", mt_keep(o.handle), 5)),
                  read_exact(&o.model, 5));
    close_text(m, &o);
    return done(m);
}
