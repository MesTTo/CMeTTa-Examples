/* Purpose: Serve repeated local requests through one owned engine lifetime.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=6022c3f48b6dc64752c6e49cfe9d985c7ac7a4e9].
 * Open Obligations: None.
 */
#include "common.h"
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
typedef struct { metta *runtime; int fd; unsigned served; } service;
static void *serve(void *user)
{
    service *s = user;
    check("attach service worker", mt_thread_attach());
    FILE *channel = fdopen(s->fd, "r+");
    check("open service channel", channel != NULL);
    char *line = NULL; size_t capacity = 0;
    while (getline(&line, &capacity, channel) >= 0) {
        char *end; errno = 0;
        long value = strtol(line, &end, 10);
        if (errno || end == line || strcmp(end, "\n") != 0) {
            check("send input refusal", fputs("ERR\n", channel) >= 0);
        } else {
            mt_clear();
            int64_t answer = mt_one_int(mt_eval(s->runtime, mt_expr("+", (int64_t)value, 1)));
            check("evaluate service request", mt_ok());
            check("send answer", fprintf(channel, "%lld\n", (long long)answer) > 0);
        }
        check("flush response", fflush(channel) == 0); ++s->served;
    }
    check("request stream ended normally", !ferror(channel));
    free(line); check("close service channel", fclose(channel) == 0);
    mt_thread_detach(); return NULL;
}
int main(void)
{
    metta *m = open_engine();
    int sockets[2]; check("local stream socket", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    service s = {m, sockets[1], 0}; pthread_t thread;
    check("start service", pthread_create(&thread, NULL, serve, &s) == 0);
    FILE *client = fdopen(sockets[0], "r+"); check("open client", client != NULL);
    const char *requests[] = {"41\n", "invalid\n", "6\n"};
    const char *expected[] = {"42\n", "ERR\n", "7\n"};
    char response[64];
    for (size_t i = 0; i < sizeof(requests)/sizeof(requests[0]); ++i) {
        check("send request", fputs(requests[i], client) >= 0 && fflush(client) == 0);
        check("response preserves service after refusal", fgets(response, sizeof(response), client) != NULL &&
              strcmp(response, expected[i]) == 0);
    }
    check("send EOF", shutdown(sockets[0], SHUT_WR) == 0);
    check("join service", pthread_join(thread, NULL) == 0);
    check("all requests served by one engine", s.served == 3);
    check("close client", fclose(client) == 0);
    return done(m, "daemon");
}
