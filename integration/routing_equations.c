/* Purpose: a router is equations. route maps a request to a page with a
 *   fallback for any other request, handle commits to the first route that
 *   answers, and logged composes middleware around it; the table of routes
 *   is a C array turned into equations.
 * Guarantees: a known route, a missing one and the logged composition answer
 *   as a router should [tested: make check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

int main(void)
{
    metta *m = open_engine();
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

    check_answers("a known route", mt_eval(m, E("handle", "home")), E("Page", 200, T("Welcome")));
    check_answers("a missing one", mt_eval(m, E("handle", "nowhere")), E("NotFound", 404, "nowhere"));
    check_answers("middleware composes", mt_eval(m, E("logged", "about")),
                  E("Logged", "about", E("Page", 200, T("About us"))));
    return done(m);
}
