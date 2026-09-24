/* Purpose: the reader's token classes as an extension seam. register-token!
 *   binds a pattern to a constructor, and a token the pattern matches whole
 *   reads as (constructor "token") where it would read as a symbol. C keeps
 *   its own model of the classes, each pattern compiled by POSIX regcomp
 *   and anchored at both ends; registering a pattern again replaces its
 *   constructor and unregistering one takes it out, answering True either
 *   way. Every text C reads is a form or token it builds from its own
 *   tokens, and what the engine's reader builds must be what C builds by
 *   classifying each token. A pattern that is not text, and a constructor
 *   that is not a symbol, are refused with the balls C builds.
 * text: the original is about what the reader builds from text, so C reads
 *   the text it builds through mt_parse, the engine's own reader.
 * Guarantees: all nineteen claims of the original hold [tested: make twins;
 *   commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"
#include <regex.h>

/* C's model of the registered classes. */
typedef struct token_class {
    char *pattern, *constructor;
    regex_t anchored;
} token_class;

static token_class *classes;
static size_t nclasses, cap;

static char *copied(const char *text)
{
    char *copy = malloc(strlen(text) + 1);
    require("room", copy != NULL);
    return strcpy(copy, text);
}

static token_class *find(const char *pattern)
{
    for (size_t i = 0; i < nclasses; i++)
        if (strcmp(classes[i].pattern, pattern) == 0) return &classes[i];
    return NULL;
}

static void model_register(const char *pattern, const char *constructor)
{
    token_class *c = find(pattern);
    if (!c) {
        if (nclasses == cap) {
            cap = cap ? 2 * cap : 4;
            classes = realloc(classes, cap * sizeof *classes);
            require("room for a class", classes != NULL);
        }
        c = &classes[nclasses++];
        c->pattern = copied(pattern);
        c->constructor = NULL;
        char *anchored = malloc(strlen(pattern) + 5);
        require("room", anchored != NULL);
        sprintf(anchored, "^(%s)$", pattern);
        require("a POSIX pattern", regcomp(&c->anchored, anchored, REG_EXTENDED | REG_NOSUB) == 0);
        free(anchored);
    }
    free(c->constructor);
    c->constructor = copied(constructor);
}

static void model_unregister(const char *pattern)
{
    token_class *c = find(pattern);
    if (!c) return;
    regfree(&c->anchored);
    free(c->pattern);
    free(c->constructor);
    *c = classes[--nclasses];
}

/* What the reader builds from one token. */
static mt_atom *classified(const char *token)
{
    for (size_t i = 0; i < nclasses; i++)
        if (regexec(&classes[i].anchored, token, 0, NULL, 0) == 0) return E(classes[i].constructor, T(token));
    return S(token);
}

/* The engine reads the text of N tokens, a form when there is more than
   one, and must build what C classifies them into. */
static void reads(const char *claim, size_t n, const char *const *tokens)
{
    size_t length = 3;
    for (size_t i = 0; i < n; i++) length += strlen(tokens[i]) + 1;
    char *text = malloc(length);
    mt_atom **items = malloc(n * sizeof *items);
    require("room", text && items);
    size_t at = 0;
    for (size_t i = 0; i < n; i++) {
        at += (size_t)sprintf(text + at, "%s%s", i ? " " : n > 1 ? "(" : "", tokens[i]);
        items[i] = classified(tokens[i]);
    }
    sprintf(text + at, "%s", n > 1 ? ")" : "");
    mt_atom *want = n > 1 ? mt_exprv(n, items) : items[0];
    check_atom(claim, mt_parse(text), want);
    free(items);
    free(text);
}
#define READS(claim, ...) reads((claim), MT_NARG(__VA_ARGS__), (const char *const[]){ __VA_ARGS__ })

static void registers(metta *m, const char *pattern, const char *constructor)
{
    model_register(pattern, constructor);
    check_answers("registering answers True", mt_eval(m, E("register-token!", T(pattern), constructor)), B(true));
}

static void unregisters(metta *m, const char *pattern)
{
    model_unregister(pattern);
    check_answers("unregistering answers True", mt_eval(m, E("unregister-token!", T(pattern))), B(true));
}

static void refused(metta *m, const char *claim, mt_atom *goal, mt_atom *want)
{
    check_answers(claim, mt_eval(m, E("catch", goal)), want);
}

int main(void)
{
    metta *m = open_engine();
    static const char *const pixels = "[0-9]+px", *const percent = "[0-9]+%";
    READS("before registering, a symbol", "12px");
    registers(m, pixels, "Pixels");
    READS("a matching token reads through its constructor", "12px");
    READS("wherever a token appears", "width", "12px");
    READS("each token on its own", "box", "12px", "4px");
    READS("anchored at both ends", "12pxy");
    READS("so a part is not a match", "px");
    registers(m, percent, "Percent");
    READS("a second class beside the first", "size", "50%", "12px");
    registers(m, pixels, "Device");
    READS("registering again replaces the constructor", "12px");
    unregisters(m, pixels);
    READS("unregistered, a symbol again", "12px");
    unregisters(m, pixels);
    READS("the other class stays", "50%");
    unregisters(m, percent);
    READS("and goes when it is taken out", "50%");
    free(classes);

    refused(m, "a pattern that is not text", E("register-token!", 12, "Pixels"),
            E("Error", E("type_error", "text", 12), E("context", "register-token!", S("a token pattern is text"))));
    refused(m, "a constructor that is not a symbol", E("register-token!", T("[0-9]+em"), T("not a symbol")),
            E("Error", E("domain_error", "metta_reader_constructor", T("not a symbol")),
              E("context", E("/", "register-token!", 3), S("the constructor must be a readable symbol"))));
    return done(m);
}
