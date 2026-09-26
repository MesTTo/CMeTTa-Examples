/* Purpose: lib_process, held against POSIX process control in C: C runs the
 *   same program with the same argument vector itself, through posix_spawnp,
 *   with pipes for its three streams, reads both outputs with poll(2) so
 *   neither can block the other, and waits with waitpid(2). An exit is its
 *   status; a signal is the negative of its number, as every shell reports.
 *   A started process is watched the same way: C starts its own, asks
 *   waitpid with WNOHANG, signals it with kill(2) and waits, and a second
 *   wait on a reaped child fails with ECHILD, which is the library's refusal.
 *   The engine's own children belong to this process too, so C never waits
 *   on or signals one of them, and signal names are a table C holds.
 * Guarantees: all twenty-four claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

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

extern char **environ;

enum { MOST = 8, OUTPUT = 256 };

/* A child C started, with its stdin writable and both outputs readable. */
typedef struct child {
    pid_t pid;
    int in, out, err;
} child;

/* 0 or the errno posix_spawnp answered, ENOENT for a program not found. */
static int spawned(const char *program, const char *const *args, size_t n, child *c)
{
    int in[2], out[2], err[2];
    require("pipes", pipe(in) == 0 && pipe(out) == 0 && pipe(err) == 0);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, in[0], 0);
    posix_spawn_file_actions_adddup2(&actions, out[1], 1);
    posix_spawn_file_actions_adddup2(&actions, err[1], 2);
    int ends[] = { in[0], in[1], out[0], out[1], err[0], err[1] };
    for (size_t i = 0; i < 6; i++) posix_spawn_file_actions_addclose(&actions, ends[i]);
    char *argv[MOST + 2];
    argv[0] = (char *)program;
    for (size_t i = 0; i < n; i++) argv[i + 1] = (char *)args[i];
    argv[n + 1] = NULL;
    int failed = posix_spawnp(&c->pid, program, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(in[0]), close(out[1]), close(err[1]);
    c->in = in[1], c->out = out[0], c->err = err[0];
    if (failed) close(c->in), close(c->out), close(c->err);
    return failed;
}

/* An exit is its code, a signal the negative of its number. */
static int64_t status_of(int status) { return WIFEXITED(status) ? WEXITSTATUS(status) : -(int64_t)WTERMSIG(status); }

/* Feed the input, close it, read both streams to their ends, wait. */
static mt_atom *finished(child *c, const char *input)
{
    char text[2][OUTPUT] = { "", "" };
    size_t used[2] = { 0, 0 };
    if (input) require("the input goes in", write(c->in, input, strlen(input)) == (ssize_t)strlen(input));
    close(c->in);
    struct pollfd fds[2] = { { c->out, POLLIN, 0 }, { c->err, POLLIN, 0 } };
    for (int open = 2; open;) {
        require("poll", poll(fds, 2, -1) >= 0);
        for (int k = 0; k < 2; k++)
            if (fds[k].fd >= 0 && fds[k].revents) {
                ssize_t got = read(fds[k].fd, text[k] + used[k], OUTPUT - 1 - used[k]);
                if (got > 0) used[k] += (size_t)got;
                else close(fds[k].fd), fds[k].fd = -1, open--;
            }
    }
    int status;
    require("waitpid", waitpid(c->pid, &status, 0) == c->pid);
    return E("process-result", status_of(status), mt_textn(text[0], used[0]), mt_textn(text[1], used[1]));
}

/* What running a program answers; NULL when it cannot be launched. */
static mt_atom *ran(const char *program, const char *const *args, size_t n, const char *input)
{
    child c;
    return spawned(program, args, n, &c) ? NULL : finished(&c, input);
}

static mt_atom *texts(const char *const *args, size_t n)
{
    mt_atom *kids[MOST];
    for (size_t i = 0; i < n; i++) kids[i] = T(args[i]);
    return mt_exprv(n, kids);
}

/* The signals the library sends, as C names them. */
static const struct {
    const char *name;
    int number;
} signals[] = { { "term", SIGTERM }, { "kill", SIGKILL }, { "int", SIGINT }, { "hup", SIGHUP } };

static int signal_named(const char *name)
{
    for (size_t i = 0; i < sizeof signals / sizeof *signals; i++)
        if (strcmp(signals[i].name, name) == 0) return signals[i].number;
    return 0;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_atom *guarded(mt_atom *goal) { return E("if-error", E("catch", goal), "refused", "fine"); }

static bool computed(mt_atom *value)
{
    mt_drop(value);
    return value != NULL;
}

static bool all_texts(const mt_atom *args)
{
    if (mt_kind_of(args) != MT_EXPR) return false;
    for (size_t i = 0; i < mt_len(args); i++)
        if (mt_kind_of(mt_at(args, i)) != MT_TEXT) return false;
    return true;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_process", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_process")))));

    /* A run answers its code and both streams, as C's own run does. */
    static const struct {
        const char *claim, *program, *args[3];
        size_t n;
    } runs[] = {
        { "echo", "echo", { "hi" }, 1 },
        { "two arguments", "echo", { "one", "two" }, 2 },
        { "no output", "true", { NULL }, 0 },
        { "a nonzero exit is a status", "false", { NULL }, 0 },
        { "both streams, apart", "sh", { "-c", "echo out; echo err 1>&2; exit 3" }, 2 },
        { "an argument is never parsed", "echo", { "; echo hacked" }, 1 },
        { "a shell is the caller's choice", "sh", { "-c", "echo shell" }, 2 },
    };
    for (size_t i = 0; i < sizeof runs / sizeof *runs; i++)
        assert(answers_are(mt_eval(m, E("process-run!", T(runs[i].program), texts(runs[i].args, runs[i].n))), E(ran(runs[i].program, runs[i].args, runs[i].n, NULL)))
               && runs[i].claim);
    static const char *const exit7[] = { "-c", "exit 7" };
    mt_atom *seven = ran("sh", exit7, 2, NULL);
    assert(answers_are(mt_eval(m, E("index-atom", E("process-run!", T("sh"), texts(exit7, 2)), 1)), E(mt_keep(mt_at(seven, 1))))
           && "the code is the second field");
    mt_drop(seven);

    /* Input fed through a pipe, closed so the program sees its end. */
    static const char *const bytes[] = { "-c" };
    assert(answers_are(mt_eval(m, E("process-run-input!", T("cat"), mt_unit(), T("fed"))), E(ran("cat", NULL, 0, "fed"))) && "cat is fed");
    assert(answers_are(mt_eval(m, E("process-run-input!", T("wc"), texts(bytes, 1), T("1234"))), E(ran("wc", bytes, 1, "1234"))) && "wc counts it");

    /* A program that cannot be launched is the one refusal. */
    static const char *const unlaunched[] = { "no_such_program_anywhere", "/no/such/binary" };
    for (size_t i = 0; i < 2; i++)
        assert(answers_are(mt_eval(m, guarded(E("process-run!", T(unlaunched[i]), mt_unit()))), E(verdict(computed(ran(unlaunched[i], NULL, 0, NULL)))))
               && unlaunched[i]);

    /* A started process, watched: C starts its own beside the engine's. */
    mt_atom *quick = mt_one(mt_eval(m, E("process-start!", T("true"), mt_unit())));
    require("the engine starts true", quick && mt_kind_of(quick) == MT_INT);
    child mine;
    require("C starts true", spawned("true", NULL, 0, &mine) == 0);
    close(mine.in), close(mine.out), close(mine.err);
    int status;
    assert(answers_are(mt_eval(m, E(">", mt_keep(quick), 0)), E(B(mine.pid > 0 && mt_int(quick) > 0))) && "an identifier");
    require("C waits on its own", waitpid(mine.pid, &status, 0) == mine.pid);
    assert(answers_are(mt_eval(m, E("process-wait!", mt_keep(quick))), E(status_of(status))) && "process-wait! answers the code");
    bool reaped = waitpid(mine.pid, &status, 0) < 0 && errno == ECHILD;
    assert(answers_are(mt_eval(m, guarded(E("process-wait!", mt_keep(quick)))), E(verdict(!reaped))) && "a second wait is refused");

    static const char *const thirty[] = { "30" };
    mt_atom *sleeper = mt_one(mt_eval(m, E("process-start!", T("sleep"), texts(thirty, 1))));
    require("the engine starts sleep", sleeper != NULL);
    require("C starts sleep", spawned("sleep", thirty, 1, &mine) == 0);
    close(mine.in), close(mine.out), close(mine.err);
    assert(answers_are(mt_eval(m, E("process-status", mt_keep(sleeper))), E(S(waitpid(mine.pid, &status, WNOHANG) == 0 ? "running" : "exited"))) && "running");
    assert(answers_are(mt_eval(m, E("process-signal!", mt_keep(sleeper), "term")), E(B(kill(mine.pid, signal_named("term")) == 0))) && "process-signal!");
    require("C waits on its own", waitpid(mine.pid, &status, 0) == mine.pid);
    assert(answers_are(mt_eval(m, E("process-wait!", mt_keep(sleeper))), E(status_of(status))) && "a signal is the negative of its number");

    /* The signals, and the refusals. */
    mt_atom *names[4];
    for (size_t i = 0; i < 4; i++) names[i] = S(signals[i].name);
    assert(answers_are(mt_eval(m, E("process-signals")), E(mt_exprv(4, names))) && "process-signals");
    assert(answers_are(mt_eval(m, guarded(E("process-signal!", 1, "nosuch"))), E(verdict(signal_named("nosuch") != 0)))
           && "an unknown signal is refused, and nothing is sent");
    mt_atom *loose = T("not a collection"), *nested = E(E("nested")), *number = mt_num(7), *text = T("not a process");
    assert(answers_are(mt_eval(m, guarded(E("process-run!", T("echo"), mt_keep(loose)))), E(verdict(all_texts(loose)))) && "arguments are a collection");
    assert(answers_are(mt_eval(m, guarded(E("process-run!", T("echo"), mt_keep(nested)))), E(verdict(all_texts(nested)))) && "of texts");
    assert(answers_are(mt_eval(m, guarded(E("process-run!", mt_keep(number), mt_unit()))), E(verdict(mt_kind_of(number) == MT_TEXT))) && "a program is a text");
    assert(answers_are(mt_eval(m, guarded(E("process-wait!", mt_keep(text)))), E(verdict(mt_kind_of(text) == MT_INT))) && "a process is a number");

    mt_atom *held[] = { quick, sleeper, loose, nested, number, text };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
