/* Purpose: lib_datetime, held against C's own calendar, time.h. A weekday
 *   or a month name is strftime over gmtime_r, a parse is strptime then
 *   timegm, a date record's timestamp is timegm, and its fields are what
 *   gmtime_r fills in. Calendar addition is timegm normalising a struct tm
 *   whose day or month overflowed, which is how January 31 plus one month
 *   lands on March 3. The clock itself is checked against C's time(), read
 *   either side of the engine's.
 * Guarantees: all twenty-five claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 * Build: cc 07-datetime.c $(pkg-config --cflags --libs cmetta) -lm
 */
/* timegm is glibc's default set and strptime X/Open's, at the X/Open level
   that matches the POSIX 2008 the Makefile asks for. */
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "_fixtures/time_oracle.h"

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

static struct tm civil(int year, int month, int day)
{
    return (struct tm){ .tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day };
}

static time_t timestamp(int year, int month, int day)
{
    struct tm tm = civil(year, month, day);
    return timegm(&tm);
}

static double parsed(const char *text, const char *format)
{
    struct tm tm = { 0 };
    const char *rest = strptime(text, format, &tm);
    require("strptime reads the whole text", rest && *rest == '\0');
    return (double)timegm(&tm);
}

/* The engine's date record for a timestamp at an offset west of UTC, the
   way SWI signs it: the civil time is the timestamp less the offset, and the
   seconds keep their fraction. */
static mt_atom *date_record(double stamp, int offset)
{
    double whole = floor(stamp);
    struct tm tm = utc((time_t)whole - offset);
    return E("date", (int64_t)tm.tm_year + 1900, (int64_t)tm.tm_mon + 1, (int64_t)tm.tm_mday,
             (int64_t)tm.tm_hour, (int64_t)tm.tm_min, tm.tm_sec + (stamp - whole), (int64_t)offset, "-", "-");
}

static bool leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

static int64_t month_days(int year, int month)
{
    return (int64_t)((timestamp(year, month + 1, 1) - timestamp(year, month, 1)) / 86400);
}

/* A calendar step: the fields added to a struct tm and timegm left to carry
   whatever overflowed. */
static double added(time_t from, int years, int months, int days, int hours, int minutes, double seconds)
{
    struct tm tm = utc(from);
    tm.tm_year += years;
    tm.tm_mon += months;
    tm.tm_mday += days;
    tm.tm_hour += hours;
    tm.tm_min += minutes;
    double whole = floor(seconds);
    tm.tm_sec += (int)whole;
    return (double)timegm(&tm) + (seconds - whole);
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_datetime", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datetime")))));

    /* The clock is C's clock: it reads between two time() calls. */
    const time_t new_year = timestamp(2025, 1, 1);
    time_t before = time(NULL);
    assert(answers_are(mt_eval(m, E("let", V("ts"), E("now"), E("<", (int64_t)new_year, V("ts")))), E(B(new_year < before)))
           && "now is past a date already gone");
    double now = mt_one_float(mt_eval(m, E("now")));
    time_t after = time(NULL);
    assert((double)before <= now + 1 && now <= (double)after + 1 && "the engine's now lies between two readings of C's");
    char once[16], twice[16];
    struct tm reading = utc((time_t)now);
    strftime(once, sizeof once, "%Y-%m-%d", &reading);
    strftime(twice, sizeof twice, "%Y-%m-%d", &reading);
    assert(answers_are(mt_eval(m, E("let", V("ts"), E("now"), E("==", E("format-date", V("ts"), T("%Y-%m-%d")),
                                                                E("format-date", V("ts"), T("%Y-%m-%d"))))), E(B(strcmp(once, twice) == 0)))
           && "one reading formats alike twice");

    const time_t saturday = timestamp(2025, 12, 20), week_on = timestamp(2025, 1, 8);
    assert(answers_are(mt_eval(m, E("day-of-week", (int64_t)saturday)), E(named(saturday, "%A"))) && "day-of-week");
    assert(answers_are(mt_eval(m, E("-", (int64_t)week_on, (int64_t)new_year)), E((int64_t)difftime(week_on, new_year)))
           && "a week in seconds");
    assert(answers_are(mt_eval(m, E("format-date", (int64_t)new_year, T("%B"))), E(named(new_year, "%B"))) && "a month's name");
    assert(answers_are(mt_eval(m, E("format_date", (int64_t)new_year, T("%B"))), E(named(new_year, "%B"))) && "format_date");
    assert(answers_are(mt_eval(m, E("day_of_week", (int64_t)saturday)), E(named(saturday, "%A"))) && "day_of_week");
    char shifted[32];
    struct tm east = utc(new_year + 3600);
    strftime(shifted, sizeof shifted, "%Y-%m-%d %H:%M", &east);
    assert(answers_are(mt_eval(m, E("format-datetime", (int64_t)new_year, T("%Y-%m-%d %H:%M"), -3600)), E(T(shifted)))
           && "an offset west of UTC is negative east");
    assert(answers_are(mt_eval(m, E("parse-date", T("2025-01-01T00:00:00Z"))), E(parsed("2025-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ")))
           && "ISO 8601");
    assert(answers_are(mt_eval(m, E("parse-date", T("Wed, 01 Jan 2025 00:00:00 GMT"), "rfc_1123")), E(parsed("Wed, 01 Jan 2025 00:00:00 GMT", "%a, %d %b %Y %H:%M:%S GMT")))
           && "RFC 1123");

    /* Records. */
    assert(answers_are(mt_eval(m, E("date-timestamp", E("date", 2025, 1, 1))), E((double)new_year))
           && "a date-only record is UTC midnight");
    assert(answers_are(mt_eval(m, E("timestamp-date", 1735689600.25, -3600)), E(date_record(1735689600.25, -3600)))
           && "a record keeps its fraction and offset");
    struct tm before_epoch = utc((time_t)floor(-0.25));
    assert(answers_are(mt_eval(m, E("date-timestamp", E("timestamp-date", -0.25, "UTC"))), E((double)timegm(&before_epoch) + (-0.25 - floor(-0.25))))
           && "and round-trips a moment before the epoch");
    struct tm day = utc(new_year);
    assert(answers_are(mt_eval(m, E("date-field", E("date", 2025, 1, 1), "year")), E((int64_t)day.tm_year + 1900)) && "a field");
    assert(answers_are(mt_eval(m, E("date-field", E("date", 2025, 1, 1), "date")), E(E("date", (int64_t)day.tm_year + 1900, (int64_t)day.tm_mon + 1, (int64_t)day.tm_mday)))
           && "the date part");
    assert(!mt_first(mt_eval(m, E("date-field", E("date", 2025, 1, 1), "time_zone"))) && mt_ok() && "a date-only record has no zone");
    assert(answers_are(mt_eval(m, E("collapse", E("date-fields", E("date", 2025, 1, 1)))), E(E(E("year", (int64_t)day.tm_year + 1900), E("month", (int64_t)day.tm_mon + 1),
                                                                                               E("day", (int64_t)day.tm_mday), E("hour", (int64_t)day.tm_hour),
                                                                                               E("minute", (int64_t)day.tm_min), E("second", (int64_t)day.tm_sec), E("utc_offset", 0),
                                                                                               E("date", E("date", (int64_t)day.tm_year + 1900, (int64_t)day.tm_mon + 1, (int64_t)day.tm_mday)),
                                                                                               E("time", E("time", (int64_t)day.tm_hour, (int64_t)day.tm_min, (int64_t)day.tm_sec)))))
           && "every field, as gmtime_r fills them");
    assert(answers_are(mt_eval(m, E("date-weekday", E("date", 2025, 1, 1))), E((int64_t)(day.tm_wday ? day.tm_wday : 7)))
           && "the ISO weekday, Monday first");
    struct tm last = utc(timestamp(2024, 12, 31));
    assert(answers_are(mt_eval(m, E("date-year-day", E("date", 2024, 12, 31))), E((int64_t)last.tm_yday + 1)) && "the day of the year");
    assert(answers_are(mt_eval(m, E("leap-year", 2000)), E(B(leap(2000)))) && "2000 is a leap year");
    assert(answers_are(mt_eval(m, E("leap-year", 1900)), E(B(leap(1900)))) && "1900 is not");
    assert(answers_are(mt_eval(m, E("month-days", 2024, 2)), E(month_days(2024, 2))) && "February 2024");
    assert(answers_are(mt_eval(m, E("month-days", 2025, 2)), E(month_days(2025, 2))) && "February 2025");

    /* Calendar addition carries through timegm's normalisation. */
    const time_t january_31 = timestamp(2025, 1, 31);
    assert(answers_are(mt_eval(m, E("date-add", (int64_t)january_31, E(0, 1, 0, 0, 0, 0), "UTC")), E(added(january_31, 0, 1, 0, 0, 0, 0)))
           && "January 31 plus one month");
    assert(answers_are(mt_eval(m, E("date-add", (int64_t)new_year, E(0, 0, -1, 0, 0, 0.5), "UTC")), E(added(new_year, 0, 0, -1, 0, 0, 0.5)))
           && "a day back and half a second on");
    mt_close(m);
    return 0;
}
