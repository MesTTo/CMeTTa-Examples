/* Purpose: one engine serving requests over a socket. A worker thread
 *   attaches to the runtime, reads one integer per line from a Unix socket,
 *   answers (+ n 1) from the engine, answers ERR to a malformed line and
 *   carries on, and detaches when the client closes its end.
 * Owns resources: the socket pair, the worker thread (joined), and both
 *   stdio streams (closed).
 * Guarantees: three requests, one refused, are answered in order by one
 *   engine [tested 2026-09-27T00:35:58+10:00:
 *   make -C extensions/cmetta corpus-check].
 */
#define _POSIX_C_SOURCE 200809L
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

typedef struct service { metta *runtime; int fd; unsigned served; bool ok; } service;

static void *serve(void *user)
{
    service *s = user;
    s->ok = mt_thread_attach();
    FILE *channel = fdopen(s->fd, "r+");
    char *line = NULL;
    size_t capacity = 0;
    while (s->ok && channel && getline(&line, &capacity, channel) >= 0) {
        char *end;
        errno = 0;
        long n = strtol(line, &end, 10);
        if (errno || end == line || strcmp(end, "\n") != 0) {
            fputs("ERR\n", channel);
        } else {
            mt_clear();
            int64_t answer = mt_one_int(mt_eval(s->runtime, E("+", (int64_t)n, 1)));
            s->ok = mt_ok();
            fprintf(channel, "%lld\n", (long long)answer);
        }
        s->ok = s->ok && fflush(channel) == 0;
        s->served++;
    }
    s->ok = s->ok && channel && !ferror(channel);
    free(line);
    if (channel) fclose(channel);
    mt_thread_detach();
    return NULL;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    int sockets[2];
    require("open a socket pair", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    service s = { .runtime = m, .fd = sockets[1] };
    pthread_t worker;
    require("start the service", pthread_create(&worker, NULL, serve, &s) == 0);
    FILE *client = fdopen(sockets[0], "r+");
    require("open the client stream", client != NULL);

    static const char *const requests[] = { "41\n", "invalid\n", "6\n" };
    static const char *const replies[] = { "42\n", "ERR\n", "7\n" };
    char reply[64];
    for (size_t i = 0; i < 3; i++) {
        require("send a request", fputs(requests[i], client) >= 0 && fflush(client) == 0);
        require("read the reply", fgets(reply, sizeof reply, client) != NULL);
        assert(strcmp(reply, replies[i]) == 0 && requests[i]);
    }
    require("close the request stream", shutdown(sockets[0], SHUT_WR) == 0);
    require("join the service", pthread_join(worker, NULL) == 0);
    assert(s.ok && s.served == 3
           && "one engine served every request, the refused one included");
    require("close the client", fclose(client) == 0);
    mt_close(m);
    return 0;
}
