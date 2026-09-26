/* Purpose: lib_socket, held against POSIX sockets. C runs the same program
 *   on sockets of its own beside the engine's, a loopback listener, a client
 *   and the connection it accepts, and an IPv6 datagram socket, and every
 *   engine answer is held against what C's own sockets say: a kind from
 *   SO_TYPE and SO_ACCEPTCONN, an endpoint from getsockname and getpeername
 *   through inet_ntop, readiness from poll(2) in input order, duplicates
 *   kept, and bytes from read, write, sendto and recvfrom. Ports differ
 *   between the two programs, so where the engine answers one of its own
 *   endpoints, C proves the relation on its sockets and expects the
 *   engine's endpoint that relation names. Refusals are what the kernel
 *   answers C for the same operation, and where the library refuses before
 *   any call, the precondition it states. The transaction refusal runs
 *   inside mt_transaction.
 * Guarantees: all fifty-three claims of the original hold
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
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

/* Whether an atom is want up to renaming variables; takes both, and shows
   them when they differ. */
static inline bool atom_is(mt_atom *got, mt_atom *want)
{
    bool holds = got && want && mt_alpha_eq(got, want);
    if (!holds) fprintf(stderr, "  got %s\n  want %s\n", got ? mt_show(got) : "nothing",
                        want ? mt_show(want) : "nothing");
    mt_drop(got);
    mt_drop(want);
    return holds;
}

/* A socket's kind as the descriptor says; NULL for a closed one. */
static const char *kind_of(int fd)
{
    int type, listening;
    socklen_t n = sizeof type, m = sizeof listening;
    if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &n) != 0 || getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &listening, &m) != 0)
        return NULL;
    return listening ? "listener" : type == SOCK_STREAM ? "tcp" : "udp";
}

/* A socket address as the library spells it: family, numeric host and
   port. */
typedef struct endpoint {
    const char *family;
    char host[INET6_ADDRSTRLEN];
    int port;
} endpoint;

static bool endpoint_of(const struct sockaddr_storage *a, endpoint *e)
{
    const void *address = a->ss_family == AF_INET ? (const void *)&((const struct sockaddr_in *)a)->sin_addr
                                                  : (const void *)&((const struct sockaddr_in6 *)a)->sin6_addr;
    if (a->ss_family != AF_INET && a->ss_family != AF_INET6) return false;
    e->family = a->ss_family == AF_INET ? "ipv4" : "ipv6";
    e->port = ntohs(a->ss_family == AF_INET ? ((const struct sockaddr_in *)a)->sin_port : ((const struct sockaddr_in6 *)a)->sin6_port);
    return inet_ntop(a->ss_family, address, e->host, sizeof e->host) != NULL;
}

/* The local or peer endpoint; false when the socket has none. */
static bool side_of(int fd, bool local, endpoint *e)
{
    struct sockaddr_storage a;
    socklen_t n = sizeof a;
    return (local ? getsockname(fd, (struct sockaddr *)&a, &n) : getpeername(fd, (struct sockaddr *)&a, &n)) == 0 && endpoint_of(&a, e);
}

static bool same(const endpoint *a, const endpoint *b)
{
    return strcmp(a->family, b->family) == 0 && strcmp(a->host, b->host) == 0 && a->port == b->port;
}

/* Two sides of C's sockets that name one endpoint. */
static bool same_side(int a, bool a_local, int b, bool b_local)
{
    endpoint x, y;
    return side_of(a, a_local, &x) && side_of(b, b_local, &y) && same(&x, &y);
}

/* The families the library names, and a host in one as the numeric address
   it must be; false for anything else, as the refusal says. */
static bool address_of(const char *family, const char *host, int port, struct sockaddr_storage *a, socklen_t *n)
{
    memset(a, 0, sizeof *a);
    if (port < 0 || port > 65535) return false;
    if (strcmp(family, "ipv4") == 0) {
        struct sockaddr_in *v4 = (struct sockaddr_in *)a;
        v4->sin_family = AF_INET;
        v4->sin_port = htons((uint16_t)port);
        *n = sizeof *v4;
        return inet_pton(AF_INET, host, &v4->sin_addr) == 1;
    }
    if (strcmp(family, "ipv6") == 0) {
        struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)a;
        v6->sin6_family = AF_INET6;
        v6->sin6_port = htons((uint16_t)port);
        *n = sizeof *v6;
        return inet_pton(AF_INET6, host, &v6->sin6_addr) == 1;
    }
    return false;
}

/* A bound socket of the given type; -1 when the endpoint is refused. */
static int bound(int type, const char *family, const char *host, int port)
{
    struct sockaddr_storage a;
    socklen_t n;
    if (!address_of(family, host, port, &a, &n)) return -1;
    int fd = socket(a.ss_family, type, 0);
    require("a socket", fd >= 0);
    if (bind(fd, (struct sockaddr *)&a, n) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Which of the sockets poll(2) finds readable, in input order with
   duplicates, waiting `seconds`, INFINITY for as long as it takes, or not
   at all for an empty set; a negative wait is refused before any call. */
static bool ready(const int *fds, size_t n, double seconds, bool *readable)
{
    if (!(seconds >= 0)) return false;
    if (n == 0) return true;
    int millis = isinf(seconds) ? -1 : (int)(seconds * 1000);
    struct pollfd *p = malloc(n * sizeof *p);
    require("room for the poll", p != NULL);
    for (size_t i = 0; i < n; i++) p[i] = (struct pollfd){ fds[i], POLLIN, 0 };
    require("poll", poll(p, (nfds_t)n, millis) >= 0);
    for (size_t i = 0; i < n; i++) readable[i] = (p[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    free(p);
    return true;
}

/* The most sockets one wait here names, and the most bytes one message
   here carries. */
enum { WAITED = 4, MESSAGE = 64 };

/* The engine's handles at the positions C found ready. */
static mt_atom *ready_handles(mt_atom *const *handles, const int *fds, size_t n, double seconds)
{
    bool readable[WAITED] = { false };
    mt_atom *kids[WAITED];
    size_t found = 0;
    require("a wait C can make", n <= WAITED && ready(fds, n, seconds, readable));
    for (size_t i = 0; i < n; i++)
        if (readable[i]) kids[found++] = mt_keep(handles[i]);
    return mt_exprv(found, kids);
}

/* Up to `most` bytes, or through EOF for 0. */
static mt_atom *read_from(int fd, size_t most)
{
    unsigned char buffer[MESSAGE];
    size_t got = 0;
    ssize_t r;
    do {
        r = read(fd, buffer + got, (most ? most : sizeof buffer) - got);
        if (r > 0) got += (size_t)r;
    } while (r > 0 && (most ? got < most : got < sizeof buffer));
    return mt_array(got, buffer);
}

static bool wrote(int fd, const unsigned char *bytes, size_t n) { return write(fd, bytes, n) == (ssize_t)n; }

/* A send refuses a byte past 255 before any call; the kernel refuses a
   destination of another family. */
static bool sent(int fd, const int *values, size_t n, const char *family, const char *host, int port)
{
    unsigned char bytes[MESSAGE];
    struct sockaddr_storage a;
    socklen_t length;
    for (size_t i = 0; i < n; i++) {
        if (values[i] < 0 || values[i] > 255 || n > sizeof bytes) return false;
        bytes[i] = (unsigned char)values[i];
    }
    if (port < 1 || !address_of(family, host, port, &a, &length)) return false;
    return sendto(fd, bytes, n, 0, (struct sockaddr *)&a, length) == (ssize_t)n;
}

/* One datagram and whether its sender is `from`. */
static mt_atom *received(int fd, const endpoint *from, bool *from_there)
{
    unsigned char bytes[MESSAGE];
    struct sockaddr_storage a;
    socklen_t n = sizeof a;
    endpoint sender;
    ssize_t got = recvfrom(fd, bytes, sizeof bytes, 0, (struct sockaddr *)&a, &n);
    require("a datagram", got >= 0 && endpoint_of(&a, &sender));
    *from_there = same(&sender, from);
    return mt_array((size_t)got, bytes);
}

/* Closing releases the descriptor once, however often it is asked. */
static bool closed(int *fd)
{
    if (*fd >= 0) close(*fd);
    *fd = -1;
    return true;
}

static mt_atom *verdict(bool holds) { return S(holds ? "fine" : "refused"); }
static mt_answers *guarded(metta *m, mt_atom *goal) { return mt_eval(m, E("if-error", E("catch", goal), "refused", "fine")); }

static mt_atom *value_of(metta *m, mt_atom *goal)
{
    mt_atom *v = mt_one(mt_eval(m, goal));
    require("a value", v != NULL);
    return v;
}

/* An opener may not run inside a transaction, whose rollback would lose
   the handle's record without closing its socket; a connection needs a port
   of 1 to 65535, and a listener a backlog of at least zero. */
static bool opener_allowed(bool in_transaction) { return !in_transaction; }
static bool connectable(int64_t port) { return port >= 1 && port <= 65535; }
static bool listenable(int64_t backlog) { return backlog >= 0; }

typedef struct inside {
    mt_atom *goal, *answer;
} inside;

static mt_status in_transaction(metta *m, void *user)
{
    inside *t = user;
    t->answer = mt_one(guarded(m, mt_keep(t->goal)));
    return MT_FAIL;
}

static mt_atom *transacted(metta *m, mt_atom *goal)
{
    inside t = { goal, NULL };
    require("the transaction ran", mt_transaction(m, in_transaction, &t) == MT_FAIL);
    mt_drop(goal);
    require("an answer inside it", t.answer != NULL);
    return t.answer;
}

/* What socket-answers answers, as C's callback. */
static size_t socket_answers(int64_t *out)
{
    out[0] = 1, out[1] = 2;
    return 2;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    require("import lib_file", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_file")))));
    require("import lib_socket", mt_one_truth(mt_eval(m, E("import!", "&self", E("library", "lib_socket")))));
    require("socket-answers' type", mt_add(m, E(":", "socket-answers", E("->", "Number", "Number"))));
    require("socket-answers", mt_add(m, E("=", E("socket-answers", V("handle")), E("superpose", E(1, 2)))));

    char joined[16];
    snprintf(joined, sizeof joined, "%s/%s", "one", "two");
    assert(answers_are(mt_eval(m, E("path-join", T("one"), T("two"))), E(T(joined))) && "a path join");
    assert(answers_are(mt_eval(m, E("socket-wait!", mt_unit(), "infinite")), E(ready_handles(NULL, NULL, 0, INFINITY))) && "waiting on nothing");

    /* A listener, C's beside the engine's. */
    int listener = bound(SOCK_STREAM, "ipv4", "127.0.0.1", 0);
    require("C listens", listener >= 0 && listen(listener, 8) == 0);
    mt_atom *engine_listener = value_of(m, E("tcp-listen!", E("endpoint", "ipv4", T("127.0.0.1"), 0), 8));
    mt_atom *address = value_of(m, E("socket-endpoint", mt_keep(engine_listener), "local"));
    endpoint here;
    require("C's listener has an address", side_of(listener, true, &here));
    assert(answers_are(mt_eval(m, E("socket-kind", mt_keep(engine_listener))), E(S(kind_of(listener)))) && "a listener");
    assert(atom_is(mt_keep(mt_at(address, 1)), S(here.family)) && "its family");
    assert(atom_is(mt_keep(mt_at(address, 2)), T(here.host)) && "its host");
    assert(mt_int(mt_at(address, 3)) > 0 && here.port > 0 && "an actual port");
    mt_atom *listeners[] = { engine_listener };
    int listener_fds[] = { listener };
    assert(answers_are(mt_eval(m, E("socket-wait!", E(mt_keep(engine_listener)), 0)), E(ready_handles(listeners, listener_fds, 1, 0)))
           && "nothing to accept yet");

    int client = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_storage to;
    socklen_t to_n;
    require("C connects", client >= 0 && address_of(here.family, here.host, here.port, &to, &to_n) &&
                              connect(client, (struct sockaddr *)&to, to_n) == 0);
    mt_atom *engine_client = value_of(m, E("tcp-connect!", mt_keep(address)));
    assert(answers_are(mt_eval(m, E("socket-wait!", E(mt_keep(engine_listener)), "infinite")), E(ready_handles(listeners, listener_fds, 1, INFINITY)))
           && "a connection to accept");
    int accepted = accept(listener, NULL, NULL);
    require("C accepts", accepted >= 0);
    mt_atom *engine_accepted = value_of(m, E("tcp-accept!", mt_keep(engine_listener)));
    assert(answers_are(mt_eval(m, E("socket-kind", mt_keep(engine_client))), E(S(kind_of(client)))) && "the client is tcp");
    assert(answers_are(mt_eval(m, E("socket-kind", mt_keep(engine_accepted))), E(S(kind_of(accepted)))) && "so is the accepted end");
    require("C's accepted end is local at the listener's endpoint", same_side(accepted, true, listener, true));
    assert(answers_are(mt_eval(m, E("socket-endpoint", mt_keep(engine_accepted), "local")), E(mt_keep(address))) && "accepted at the listener's endpoint");
    require("C's client's peer is the listener's endpoint", same_side(client, false, listener, true));
    assert(answers_are(mt_eval(m, E("socket-endpoint", mt_keep(engine_client), "peer")), E(mt_keep(address))) && "the client's peer");
    require("C's accepted peer is its client's local endpoint", same_side(accepted, false, client, true));
    assert(answers_are(mt_eval(m, E("socket-endpoint", mt_keep(engine_accepted), "peer")), E(value_of(m, E("socket-endpoint", mt_keep(engine_client), "local"))))
           && "the accepted end's peer");

    /* Bytes both ways, and a half close. */
    static const unsigned char message[] = { 0, 128, 255, 10 }, answer[] = { 42 };
    assert(answers_are(mt_eval(m, E("file-write-bytes!", mt_keep(engine_client), E(0, 128, 255, 10))), E(B(wrote(client, message, 4)))) && "written");
    mt_atom *twice[] = { engine_accepted, engine_accepted };
    int twice_fds[] = { accepted, accepted };
    assert(answers_are(mt_eval(m, E("socket-wait!", E(mt_keep(engine_accepted), mt_keep(engine_accepted)), "infinite")), E(ready_handles(twice, twice_fds, 2, INFINITY)))
           && "readable, duplicates kept");
    assert(answers_are(mt_eval(m, E("file-read-bytes!", mt_keep(engine_accepted), 2)), E(read_from(accepted, 2))) && "two bytes");
    assert(answers_are(mt_eval(m, E("file-read-bytes!", mt_keep(engine_accepted), 2)), E(read_from(accepted, 2))) && "two more");
    assert(answers_are(mt_eval(m, E("socket-shutdown!", mt_keep(engine_client), "write")), E(B(shutdown(client, SHUT_WR) == 0))) && "the write side shut");
    assert(answers_are(mt_eval(m, E("file-read-bytes!", mt_keep(engine_accepted))), E(read_from(accepted, 0))) && "so EOF");
    assert(answers_are(mt_eval(m, E("file-write-bytes!", mt_keep(engine_accepted), E(42))), E(B(wrote(accepted, answer, 1)))) && "the other way still works");
    assert(answers_are(mt_eval(m, E("file-read-bytes!", mt_keep(engine_client), 1)), E(read_from(client, 1))) && "and arrives");
    assert(answers_are(mt_eval(m, E("socket-shutdown!", mt_keep(engine_accepted), "both")), E(B(shutdown(accepted, SHUT_RDWR) == 0))) && "both sides shut");
    assert(answers_are(mt_eval(m, E("file-close!", mt_keep(engine_listener))), E(B(closed(&listener)))) && "the listener closed");
    assert(answers_are(mt_eval(m, E("file-close!", mt_keep(engine_client))), E(B(closed(&client)))) && "the client closed");
    assert(answers_are(mt_eval(m, E("file-close!", mt_keep(engine_accepted))), E(B(closed(&accepted)))) && "the accepted end closed");
    assert(answers_are(mt_eval(m, E("file-close!", mt_keep(engine_accepted))), E(B(closed(&accepted)))) && "closing again is harmless");

    /* Datagrams, one packet a send, the sender's endpoint beside its bytes. */
    int udp = bound(SOCK_DGRAM, "ipv6", "::1", 0);
    require("C binds a datagram socket", udp >= 0);
    mt_atom *engine_udp = value_of(m, E("udp-bind!", E("endpoint", "ipv6", T("::1"), 0)));
    mt_atom *udp_address = value_of(m, E("socket-endpoint", mt_keep(engine_udp), "local"));
    endpoint udp_here;
    require("C's datagram socket has an address", side_of(udp, true, &udp_here));
    mt_atom *udps[] = { engine_udp };
    int udp_fds[] = { udp };
    assert(answers_are(mt_eval(m, E("socket-kind", mt_keep(engine_udp))), E(S(kind_of(udp)))) && "a datagram socket");
    assert(atom_is(mt_keep(mt_at(udp_address, 1)), S(udp_here.family)) && "IPv6");
    assert(atom_is(mt_keep(mt_at(udp_address, 2)), T(udp_here.host)) && "on the loopback");
    assert(answers_are(mt_eval(m, E("socket-wait!", E(mt_keep(engine_udp)), 0)), E(ready_handles(udps, udp_fds, 1, 0))) && "nothing arrived yet");
    static const int packet[] = { 0, 255, 1 }, past_a_byte[] = { 256 };
    assert(answers_are(mt_eval(m, E("udp-send!", mt_keep(engine_udp), mt_keep(udp_address), E(0, 255, 1))), E(B(sent(udp, packet, 3, udp_here.family, udp_here.host, udp_here.port))))
           && "a packet sent");
    assert(answers_are(mt_eval(m, E("udp-send!", mt_keep(engine_udp), mt_keep(udp_address), mt_unit())), E(B(sent(udp, NULL, 0, udp_here.family, udp_here.host, udp_here.port))))
           && "an empty one too");
    assert(answers_are(mt_eval(m, E("socket-wait!", E(mt_keep(engine_udp)), "infinite")), E(ready_handles(udps, udp_fds, 1, INFINITY))) && "a packet to read");
    bool from_here;
    mt_atom *bytes = received(udp, &udp_here, &from_here);
    require("C's packet came from itself", from_here);
    assert(answers_are(mt_eval(m, E("udp-receive!", mt_keep(engine_udp))), E(E("datagram", mt_keep(udp_address), bytes))) && "the packet and its sender");
    bytes = received(udp, &udp_here, &from_here);
    require("so did the empty one", from_here);
    assert(answers_are(mt_eval(m, E("udp-receive!", mt_keep(engine_udp))), E(E("datagram", mt_keep(udp_address), bytes))) && "the empty packet");

    /* Refusals, before anything reaches the network. */
    assert(answers_are(guarded(m, E("udp-send!", mt_keep(engine_udp), mt_keep(udp_address), E(256))), E(verdict(sent(udp, past_a_byte, 1, udp_here.family, udp_here.host, udp_here.port))))
           && "a byte past 255");
    assert(answers_are(mt_eval(m, E("socket-wait!", E(mt_keep(engine_udp)), 0)), E(ready_handles(udps, udp_fds, 1, 0))) && "and nothing was sent");
    assert(answers_are(guarded(m, E("udp-send!", mt_keep(engine_udp), E("endpoint", "ipv4", T("127.0.0.1"), 1), mt_unit())), E(verdict(sent(udp, NULL, 0, "ipv4", "127.0.0.1", 1))))
           && "an IPv4 destination");
    endpoint none;
    assert(answers_are(guarded(m, E("socket-endpoint", mt_keep(engine_udp), "peer")), E(verdict(side_of(udp, false, &none)))) && "no peer to name");
    int nobody = accept(udp, NULL, NULL);
    assert(answers_are(guarded(m, E("tcp-accept!", mt_keep(engine_udp))), E(verdict(nobody >= 0))) && "nothing to accept on");
    assert(answers_are(guarded(m, E("socket-shutdown!", mt_keep(engine_udp), "write")), E(verdict(shutdown(udp, SHUT_WR) == 0))) && "nothing to shut");
    assert(answers_are(mt_eval(m, E("file-close!", mt_keep(engine_udp))), E(B(closed(&udp)))) && "the datagram socket closed");
    assert(answers_are(guarded(m, E("socket-kind", mt_keep(engine_udp))), E(verdict(kind_of(udp) != NULL))) && "no kind once closed");
    struct sockaddr_storage scratch;
    socklen_t scratch_n;
    assert(answers_are(guarded(m, E("udp-bind!", E("endpoint", "unknown", T("127.0.0.1"), 0))), E(verdict(address_of("unknown", "127.0.0.1", 0, &scratch, &scratch_n))))
           && "an unknown family");
    assert(answers_are(guarded(m, E("udp-bind!", E("endpoint", "ipv4", T(""), 0))), E(verdict(address_of("ipv4", "", 0, &scratch, &scratch_n)))) && "no host");
    assert(answers_are(guarded(m, E("udp-bind!", E("endpoint", "ipv4", T("127.0.0.1"), 65536))), E(verdict(address_of("ipv4", "127.0.0.1", 65536, &scratch, &scratch_n))))
           && "a port past 65535");
    assert(answers_are(guarded(m, E("tcp-connect!", E("endpoint", "ipv4", T("127.0.0.1"), 0))), E(verdict(connectable(0)))) && "port zero connects nowhere");
    assert(answers_are(guarded(m, E("tcp-listen!", E("endpoint", "ipv4", T("127.0.0.1"), 0), -1)), E(verdict(listenable(-1)))) && "a negative backlog");
    assert(answers_are(guarded(m, E("socket-wait!", mt_unit(), -1)), E(verdict(ready(NULL, 0, -1, NULL)))) && "a negative wait");
    assert(atom_is(transacted(m, E("udp-bind!", E("endpoint", "ipv4", T("127.0.0.1"), 0))), verdict(opener_allowed(true)))
           && "no opening inside a transaction");

    /* A scope keeps its socket until every answer is out, then closes it. */
    int64_t answers[2];
    size_t n_answers = socket_answers(answers);
    mt_atom *expected[2];
    for (size_t i = 0; i < n_answers; i++) expected[i] = mt_num(answers[i]);
    assert(answers_are(mt_eval(m, E("with-socket", E("udp-bind!", E("endpoint", "ipv4", T("127.0.0.1"), 0)), "socket-answers")), mt_exprv(n_answers, expected))
           && "every answer of the scope");
    int scoped = bound(SOCK_DGRAM, "ipv4", "127.0.0.1", 0);
    require("C's scoped socket", scoped >= 0);
    assert(answers_are(mt_eval(m, E("with-socket", E("udp-bind!", E("endpoint", "ipv4", T("127.0.0.1"), 0)), "socket-kind")), E(S(kind_of(scoped))))
           && "the scoped kind");
    closed(&scoped);
    mt_atom *escaped = value_of(m, E("with-socket", E("udp-bind!", E("endpoint", "ipv4", T("127.0.0.1"), 0)), E("lambda", V("handle"), V("handle"))));
    assert(answers_are(guarded(m, E("socket-kind", mt_keep(escaped))), E(verdict(kind_of(scoped) != NULL))) && "closed when the scope ends");

    mt_atom *held[] = { engine_listener, address, engine_client, engine_accepted, engine_udp, udp_address, escaped };
    for (size_t i = 0; i < sizeof held / sizeof *held; i++) mt_drop(held[i]);
    mt_close(m);
    return 0;
}
