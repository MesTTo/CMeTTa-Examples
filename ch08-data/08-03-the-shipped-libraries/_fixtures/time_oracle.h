/* Purpose: time.h as the oracle for lib_datetime's names, shared by
 *   07-datetime and 16-the_prolog_rung: a timestamp's UTC fields from
 *   gmtime_r, and a weekday or month name from strftime, as the symbol the
 *   engine answers. POSIX alone, so an includer needs no feature macro for it.
 * Assumes: the includer defines MT_SHORTHAND before its first include.
 */
#ifndef TIME_ORACLE_H
#define TIME_ORACLE_H
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

static inline struct tm utc(time_t t)
{
    struct tm tm;
    require("gmtime_r", gmtime_r(&t, &tm) != NULL);
    return tm;
}

/* strftime's text for t in UTC, as a symbol, the kind the engine names a
   weekday or a month with. */
static inline mt_atom *named(time_t t, const char *format)
{
    char out[64];
    struct tm tm = utc(t);
    require("strftime", strftime(out, sizeof out, format, &tm) > 0);
    return mt_sym(out);
}

#endif
