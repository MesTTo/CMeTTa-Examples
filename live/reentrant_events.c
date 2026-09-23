/* Purpose: callbacks that write. Each subscription answers a ping with a pong
 *   and a pong with the next ping by adding to the space it watches, so the
 *   exchange runs through reentrant notifications until ping 3 ends it.
 * Guarantees: the transcript is ping1 pong1 ping2 pong2 ping3 [tested: make
 *   check; commit=WORKTREE].
 */
#define MT_SHORTHAND
#include "common.h"

typedef struct exchange {
    metta *runtime;
    int transcript[5];      /* 10*n for ping n, 10*n+1 for pong n */
    size_t length;
} exchange;

static mt_status receive(void *user, bool added, const mt_atom *message)
{
    exchange *x = user;
    if (!added || x->length == 5) return MT_OK;
    int64_t n = mt_int(mt_at(message, 1));
    bool ping = strcmp(mt_name(mt_at(message, 0)), "ping") == 0;
    x->transcript[x->length++] = (int)(10 * n + (ping ? 0 : 1));
    if (ping && n == 3) return MT_OK;
    mt_atom *reply = ping ? E("pong", n) : E("ping", n + 1);
    return mt_add(x->runtime, reply) ? MT_OK : mt_error();
}

int main(void)
{
    metta *m = open_engine();
    exchange x = { .runtime = m };
    static const char *const actors[] = { "ping", "pong" };
    for (size_t i = 0; i < 2; i++)
        require("subscribe an actor", mt_subscribe(m, actors[i], (mt_subscription){
            .space = "&self", .pattern = E(actors[i], V("n")), .notify = receive, .user = &x }));

    require("serve the first ping", mt_add(m, E("ping", 1)));
    static const int expected[] = { 10, 11, 20, 21, 30 };
    check("the exchange runs to ping 3",
          x.length == 5 && memcmp(x.transcript, expected, sizeof expected) == 0);
    for (size_t i = 0; i < 2; i++) require("cancel an actor", mt_unsubscribe(m, actors[i]));
    return done(m);
}
