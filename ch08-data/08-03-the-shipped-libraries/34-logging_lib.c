/* Purpose: lib_logging, held against a logger written in C beside it. The
 *   levels are RFC 5424's severities by the names syslog(3) gives
 *   LOG_DEBUG, LOG_INFO, LOG_WARNING and LOG_ERR; a topic is a C row with an
 *   exact name and a switch, so a name with a suffix is another topic; a
 *   line is "topic [level] payload" with the payload shown unevaluated; and
 *   an event reaches a handler only while its topic is on, though its level
 *   is checked either way. A handler answers one Bool, and anything else is
 *   refused. The record-log handler the original defines is the equation C
 *   builds and adds, and C keeps its own record of what that handler stores.
 * Guarantees: all twenty-eight claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

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

enum { MOST = 8 };

/* RFC 5424's severities the library names, beside syslog(3)'s priority for
   each [source: https://www.rfc-editor.org/rfc/rfc5424#section-6.2.1]. */
static const struct {
    const char *name;
    int priority;
} levels[] = { { "debug", LOG_DEBUG }, { "informational", LOG_INFO }, { "warning", LOG_WARNING }, { "error", LOG_ERR } };

static bool a_level(const char *name)
{
    for (size_t i = 0; i < sizeof levels / sizeof *levels; i++)
        if (strcmp(levels[i].name, name) == 0) return true;
    return false;
}

/* Topics, by exact name. */
static struct {
    char name[64];
    bool on;
} topics[MOST];
static size_t n_topics;

static bool enabled(const char *name)
{
    for (size_t i = 0; i < n_topics; i++)
        if (strcmp(topics[i].name, name) == 0) return topics[i].on;
    return false;
}

static void set_topic(const char *name, bool on)
{
    for (size_t i = 0; i < n_topics; i++)
        if (strcmp(topics[i].name, name) == 0) {
            topics[i].on = on;
            return;
        }
    require("room for the topics", n_topics < MOST);
    snprintf(topics[n_topics].name, sizeof topics[n_topics].name, "%s", name);
    topics[n_topics++].on = on;
}

static mt_atom *topic_rows(void)
{
    mt_atom *rows[MOST];
    for (size_t i = 0; i < n_topics; i++) rows[i] = E("log-topic", T(topics[i].name), B(topics[i].on));
    return mt_exprv(n_topics, rows);
}

/* "topic [level] payload", the payload as written; NULL for an unknown
   level. */
static mt_atom *line(const char *topic, const char *level, const mt_atom *payload)
{
    if (!a_level(level)) return NULL;
    char out[256];
    snprintf(out, sizeof out, "%s [%s] %s", topic, level, mt_kind_of(payload) == MT_TEXT ? mt_name(payload) : mt_show(payload));
    return T(out);
}

/* What storing an event does, as the record-log handler stores it. */
static mt_atom *records[MOST];
static size_t n_records;

static mt_atom *event(const char *topic, const char *level, const mt_atom *payload) { return E("log-event", T(topic), S(level), mt_keep(payload)); }

/* A delivery: the level is checked first, a switched-off topic applies no
   handler, and a handler must answer exactly one Bool. `verdicts` counts the
   handler's answers and `boolean` whether the one it gave is a Bool; false
   for a refusal. */
static bool delivered(const char *topic, const char *level, size_t verdicts, bool boolean)
{
    if (!a_level(level)) return false;
    return !enabled(topic) || (verdicts == 1 && boolean);
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }

static mt_answers *refused_or_not(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static mt_atom *handler(bool answer) { return E("|->", E(V("event")), B(answer)); }

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_logging", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_logging")))));
    require("record-log's type", mt_add(m, E(":", "record-log", E("->", "String", "Expression", "Bool"))));
    require("record-log", mt_add(m, E("=", E("record-log", V("label"), V("event")),
                                        E("add-atom", "&log-records", E("captured", V("label"), V("event"))))));
    const char *topic = "library-example";
    mt_atom *sum = E("+", 1, 2);

    /* The levels, and a topic nothing has switched on. */
    mt_atom *names[4];
    for (size_t i = 0; i < 4; i++) names[i] = S(levels[i].name);
    assert(answers_are(mt_eval(m, E("log-levels")), E(mt_exprv(4, names))) && "the levels");
    assert(answers_are(mt_eval(m, E("log-enabled", T(topic))), E(B(enabled(topic)))) && "a topic starts off");
    assert(answers_are(mt_eval(m, E("log-topics")), E(topic_rows())) && "no topics");
    require("switched off, no handler runs", delivered(topic, "error", 0, false));
    assert(answers_are(mt_eval(m, E("log-to!", "missing-handler", T(topic), "error", mt_keep(sum))), E(mt_unit())) && "so a missing handler is never asked");
    assert(answers_are(mt_eval(m, E("log-format", T(topic), "debug", mt_keep(sum))), E(line(topic, "debug", sum))) && "the line");
    set_topic(topic, true);
    assert(answers_are(mt_eval(m, E("log-topic!", T(topic), B(true))), E(mt_unit())) && "switching a topic on");
    assert(answers_are(mt_eval(m, E("log-enabled", T(topic))), E(B(enabled(topic)))) && "is on");
    assert(answers_are(mt_eval(m, E("log-topics")), E(topic_rows())) && "and listed");

    /* Deliveries. */
    require("a printed event is delivered", delivered(topic, "informational", 1, true));
    assert(answers_are(mt_eval(m, E("log!", T(topic), "informational", T("hello"))), E(mt_unit())) && "log! prints");
    records[n_records++] = E("captured", T("first"), event(topic, "debug", sum));
    assert(answers_are(mt_eval(m, E("log-to!", E("record-log", T("first")), T(topic), "debug", mt_keep(sum))), E(mt_unit())) && "a handler captures");
    const mt_atom *first = mt_at(mt_at(records[0], 2), 3);
    assert(answers_are(mt_eval(m, E("match", "&log-records", E("captured", T("first"), E("log-event", V("topic"), V("level"), V("payload"))),
                                    E("size-atom", V("payload")))), E((int64_t)mt_len(first)))
           && "the payload unevaluated");
    assert(answers_are(mt_eval(m, E("match", "&log-records", E("captured", T("first"), E("log-event", V("topic"), V("level"), V("payload"))),
                                    E("==", E("quote", V("payload")), E("quote", mt_keep(sum))))), E(B(mt_eq(first, sum))))
           && "the payload itself");
    mt_atom *loaded = E("loaded", T("data.csv")), *retry = E("retry", 2), *failed = E("failed", T("input"));
    records[n_records++] = E("captured", T("second"), event(topic, "informational", loaded));
    assert(answers_are(mt_eval(m, E("log-to!", E("record-log", T("second")), T(topic), "informational", mt_keep(loaded))), E(mt_unit())) && "another capture");
    assert(answers_are(mt_eval(m, E("log-to!", handler(true), T(topic), "warning", mt_keep(retry))), E(mt_unit())) && "a lambda handler");
    assert(answers_are(mt_eval(m, E("log-to!", handler(true), T(topic), "error", mt_keep(failed))), E(mt_unit())) && "at every level");
    assert(answers_are(mt_eval(m, E("size-atom", E("collapse", E("get-atoms", "&log-records")))), E((int64_t)n_records)) && "two captured");
    assert(answers_are(mt_eval(m, E("log-to!", handler(false), T(topic), "informational", T("visible"))), E(mt_unit())) && "a handler may decline");

    /* Refusals. */
    assert(answers_are(refused_or_not(m, E("log-to!", E("|->", E(V("event")), 7), T(topic), "debug", "x")), E(verdict(delivered(topic, "debug", 1, false))))
           && "a verdict that is no Bool");
    assert(answers_are(refused_or_not(m, E("log-to!", E("|->", E(V("event")), E("empty")), T(topic), "debug", "x")), E(verdict(delivered(topic, "debug", 0, false))))
           && "no verdict at all");
    assert(answers_are(refused_or_not(m, E("log-to!", "missing-handler", T(topic), "debug", "x")), E(verdict(delivered(topic, "debug", 1, false))))
           && "a handler that is not there");
    assert(answers_are(refused_or_not(m, E("log!", T(topic), "fatal", "x")), E(verdict(delivered(topic, "fatal", 1, true)))) && "an unknown level");
    assert(answers_are(refused_or_not(m, E("log-format", T(topic), "fatal", "x")), E(verdict(a_level("fatal")))) && "which formats nothing");
    set_topic(topic, false);
    assert(answers_are(mt_eval(m, E("log-topic!", T(topic), B(false))), E(mt_unit())) && "switching it off");
    assert(answers_are(mt_eval(m, E("log-enabled", T(topic))), E(B(enabled(topic)))) && "is off");
    require("off, no handler runs", delivered(topic, "error", 0, false));
    assert(answers_are(mt_eval(m, E("log-to!", "missing-handler", T(topic), "error", "x")), E(mt_unit())) && "so the missing handler is not asked");
    assert(answers_are(mt_eval(m, E("log-topics")), E(topic_rows())) && "and it is listed off");
    assert(answers_are(refused_or_not(m, E("log!", T(topic), "fatal", "x")), E(verdict(delivered(topic, "fatal", 1, true)))) && "the level is checked all the same");
    assert(answers_are(mt_eval(m, E("log-enabled", T("library-example.child"))), E(B(enabled("library-example.child")))) && "names are exact");

    for (size_t i = 0; i < n_records; i++) mt_drop(records[i]);
    mt_atom *held[] = { sum, loaded, retry, failed };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
