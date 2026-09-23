/* Purpose: lib_datetime, held against C's own calendar, time.h. A weekday
 *   or a month name is strftime over gmtime_r, a parse is strptime then
 *   timegm, a date record's timestamp is timegm, and its fields are what
 *   gmtime_r fills in. Calendar addition is timegm normalising a struct tm
 *   whose day or month overflowed, which is how January 31 plus one month
 *   lands on March 3. The clock itself is checked against C's time(), read
 *   either side of the engine's.
 * Guarantees: all twenty-five claims of the original hold [tested: make
 *   twins; commit=WORKTREE].
 */
/* timegm is glibc's default set and strptime X/Open's, at the X/Open level
   that matches the POSIX 2008 the Makefile asks for. */
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#define MT_SHORTHAND
#include "common.h"
#include <math.h>
#include <time.h>

static struct tm utc(time_t t)
{
    struct tm tm;
    require("gmtime_r", gmtime_r(&t, &tm) != NULL);
    return tm;
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

/* strftime's text for t in UTC, as a symbol, the kind the engine names a
   weekday or a month with. */
static mt_atom *named(time_t t, const char *format)
{
    char out[64];
    struct tm tm = utc(t);
    require("strftime", strftime(out, sizeof out, format, &tm) > 0);
    return mt_sym(out);
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
    metta *m = open_engine();
    require("import lib_datetime", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_datetime")))));

    /* The clock is C's clock: it reads between two time() calls. */
    const time_t new_year = timestamp(2025, 1, 1);
    time_t before = time(NULL);
    check_answers("now is past a date already gone", mt_eval(m, E("let", V("ts"), E("now"), E("<", (int64_t)new_year, V("ts")))),
                  B(new_year < before));
    double now = mt_one_float(mt_eval(m, E("now")));
    time_t after = time(NULL);
    check("the engine's now lies between two readings of C's", (double)before <= now + 1 && now <= (double)after + 1);
    char once[16], twice[16];
    struct tm reading = utc((time_t)now);
    strftime(once, sizeof once, "%Y-%m-%d", &reading);
    strftime(twice, sizeof twice, "%Y-%m-%d", &reading);
    check_answers("one reading formats alike twice",
                  mt_eval(m, E("let", V("ts"), E("now"), E("==", E("format-date", V("ts"), T("%Y-%m-%d")),
                                                           E("format-date", V("ts"), T("%Y-%m-%d"))))),
                  B(strcmp(once, twice) == 0));

    const time_t saturday = timestamp(2025, 12, 20), week_on = timestamp(2025, 1, 8);
    check_answers("day-of-week", mt_eval(m, E("day-of-week", (int64_t)saturday)), named(saturday, "%A"));
    check_answers("a week in seconds", mt_eval(m, E("-", (int64_t)week_on, (int64_t)new_year)),
                  (int64_t)difftime(week_on, new_year));
    check_answers("a month's name", mt_eval(m, E("format-date", (int64_t)new_year, T("%B"))), named(new_year, "%B"));
    check_answers("format_date", mt_eval(m, E("format_date", (int64_t)new_year, T("%B"))), named(new_year, "%B"));
    check_answers("day_of_week", mt_eval(m, E("day_of_week", (int64_t)saturday)), named(saturday, "%A"));
    char shifted[32];
    struct tm east = utc(new_year + 3600);
    strftime(shifted, sizeof shifted, "%Y-%m-%d %H:%M", &east);
    check_answers("an offset west of UTC is negative east",
                  mt_eval(m, E("format-datetime", (int64_t)new_year, T("%Y-%m-%d %H:%M"), -3600)), T(shifted));
    check_answers("ISO 8601", mt_eval(m, E("parse-date", T("2025-01-01T00:00:00Z"))),
                  parsed("2025-01-01T00:00:00Z", "%Y-%m-%dT%H:%M:%SZ"));
    check_answers("RFC 1123", mt_eval(m, E("parse-date", T("Wed, 01 Jan 2025 00:00:00 GMT"), "rfc_1123")),
                  parsed("Wed, 01 Jan 2025 00:00:00 GMT", "%a, %d %b %Y %H:%M:%S GMT"));

    /* Records. */
    check_answers("a date-only record is UTC midnight", mt_eval(m, E("date-timestamp", E("date", 2025, 1, 1))),
                  (double)new_year);
    check_answers("a record keeps its fraction and offset", mt_eval(m, E("timestamp-date", 1735689600.25, -3600)),
                  date_record(1735689600.25, -3600));
    struct tm before_epoch = utc((time_t)floor(-0.25));
    check_answers("and round-trips a moment before the epoch",
                  mt_eval(m, E("date-timestamp", E("timestamp-date", -0.25, "UTC"))),
                  (double)timegm(&before_epoch) + (-0.25 - floor(-0.25)));
    struct tm day = utc(new_year);
    check_answers("a field", mt_eval(m, E("date-field", E("date", 2025, 1, 1), "year")), (int64_t)day.tm_year + 1900);
    check_answers("the date part", mt_eval(m, E("date-field", E("date", 2025, 1, 1), "date")),
                  E("date", (int64_t)day.tm_year + 1900, (int64_t)day.tm_mon + 1, (int64_t)day.tm_mday));
    check_none("a date-only record has no zone", mt_eval(m, E("date-field", E("date", 2025, 1, 1), "time_zone")));
    check_answers("every field, as gmtime_r fills them",
                  mt_eval(m, E("collapse", E("date-fields", E("date", 2025, 1, 1)))),
                  E(E("year", (int64_t)day.tm_year + 1900), E("month", (int64_t)day.tm_mon + 1),
                    E("day", (int64_t)day.tm_mday), E("hour", (int64_t)day.tm_hour),
                    E("minute", (int64_t)day.tm_min), E("second", (int64_t)day.tm_sec), E("utc_offset", 0),
                    E("date", E("date", (int64_t)day.tm_year + 1900, (int64_t)day.tm_mon + 1, (int64_t)day.tm_mday)),
                    E("time", E("time", (int64_t)day.tm_hour, (int64_t)day.tm_min, (int64_t)day.tm_sec))));
    check_answers("the ISO weekday, Monday first", mt_eval(m, E("date-weekday", E("date", 2025, 1, 1))),
                  (int64_t)(day.tm_wday ? day.tm_wday : 7));
    struct tm last = utc(timestamp(2024, 12, 31));
    check_answers("the day of the year", mt_eval(m, E("date-year-day", E("date", 2024, 12, 31))), (int64_t)last.tm_yday + 1);
    check_answers("2000 is a leap year", mt_eval(m, E("leap-year", 2000)), B(leap(2000)));
    check_answers("1900 is not", mt_eval(m, E("leap-year", 1900)), B(leap(1900)));
    check_answers("February 2024", mt_eval(m, E("month-days", 2024, 2)), month_days(2024, 2));
    check_answers("February 2025", mt_eval(m, E("month-days", 2025, 2)), month_days(2025, 2));

    /* Calendar addition carries through timegm's normalisation. */
    const time_t january_31 = timestamp(2025, 1, 31);
    check_answers("January 31 plus one month",
                  mt_eval(m, E("date-add", (int64_t)january_31, E(0, 1, 0, 0, 0, 0), "UTC")),
                  added(january_31, 0, 1, 0, 0, 0, 0));
    check_answers("a day back and half a second on",
                  mt_eval(m, E("date-add", (int64_t)new_year, E(0, 0, -1, 0, 0, 0.5), "UTC")),
                  added(new_year, 0, 0, -1, 0, 0, 0.5));
    return done(m);
}
