/* Purpose: route requests and compose middleware through ordinary equations.
 * Owns resources: releases the engine after checked requests.
 * Guarantees: route precedence and missing routes are checked [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
int main(void)
{
    metta *m = open_engine();
    check("route definitions", mt_do(m,
      "(= (route home) (Page 200 \"Welcome\")) (= (route about) (Page 200 \"About us\")) "
      "(= (route $other) (NotFound 404 $other)) (= (handle $request) (once (route $request))) "
      "(= (logged $request) (let $response (handle $request) (Logged $request $response)))"));
    check_answers("known route", mt_run(m, "!(handle home)"), "(Page 200 \"Welcome\")");
    check_answers("missing route", mt_run(m, "!(handle nowhere)"), "(NotFound 404 nowhere)");
    check_answers("middleware composition", mt_run(m, "!(logged about)"), "(Logged about (Page 200 \"About us\"))");
    return done(m, "routing_equations");
}
