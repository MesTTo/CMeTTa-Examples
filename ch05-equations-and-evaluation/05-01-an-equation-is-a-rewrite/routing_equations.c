/* Purpose: a router is equations. route maps a request to a page with a
 *   fallback for any other request, handle commits to the first route that
 *   answers, and logged composes middleware around it; the table of routes
 *   is a C array turned into equations.
 * Guarantees: a known route, a missing one and the logged composition answer
 *   as a router should [tested 2026-09-27T00:35:58+10:00:
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

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    static const struct { const char *path, *title; } pages[] = {
        { "home", "Welcome" }, { "about", "About us" },
    };
    for (size_t i = 0; i < 2; i++)     /* (= (route home) (Page 200 "Welcome")) */
        require("a route", mt_add(m, E("=", E("route", pages[i].path), E("Page", 200, T(pages[i].title)))));
    /* (= (route $other) (NotFound 404 $other)) */
    require("the fallback", mt_add(m, E("=", E("route", V("other")), E("NotFound", 404, V("other")))));
    /* (= (handle $r) (once (route $r))) */
    require("handle", mt_add(m, E("=", E("handle", V("r")), E("once", E("route", V("r"))))));
    /* (= (logged $r) (let $response (handle $r) (Logged $r $response))) */
    require("logged", mt_add(m, E("=", E("logged", V("r")),
        E("let", V("response"), E("handle", V("r")), E("Logged", V("r"), V("response"))))));

    assert(answers_are(mt_eval(m, E("handle", "home")), E(E("Page", 200, T("Welcome")))) && "a known route");
    assert(answers_are(mt_eval(m, E("handle", "nowhere")), E(E("NotFound", 404, "nowhere"))) && "a missing one");
    assert(answers_are(mt_eval(m, E("logged", "about")), E(E("Logged", "about", E("Page", 200, T("About us")))))
           && "middleware composes");
    mt_close(m);
    return 0;
}
