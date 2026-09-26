/* Purpose: C threads share one engine. Each worker attaches, evaluates on
 *   its own, and keeps its own error state: a conversion error one worker
 *   records is invisible to the others and to the main thread.
 * Owns resources: four joined threads.
 * Guarantees: every worker's answer is right and no error crosses threads
 *   [tested 2026-09-27T00:35:58+10:00: make -C extensions/cmetta corpus-check].
 */
#define MT_SHORTHAND
#include <assert.h>
#include <cmetta.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* A door the program needs before it can go on: on refusal, say why and stop. */
#define require(what, ok) \
    ((ok) ? (void)0 : (fprintf(stderr, "%s: %s\n", (what), mt_errmsg()), exit(EXIT_FAILURE)))

typedef struct job { metta *runtime; int64_t input, output; bool own_error; } job;

static void *work(void *user)
{
    job *j = user;
    if (!mt_thread_attach()) return NULL;
    mt_clear();
    j->output = mt_one_int(mt_eval(j->runtime, E("+", j->input, 10)));
    if (j->input % 2) {                    /* odd workers make a mistake of their own */
        mt_atom *word = S("not-a-number");
        (void)mt_int(word);
        mt_drop(word);
        j->own_error = mt_error() == MT_MISUSE;
    } else {
        j->own_error = mt_ok();
    }
    mt_thread_detach();
    return NULL;
}

int main(void)
{
    metta *m = mt_open(NULL);
    if (!m) return fprintf(stderr, "boot: %s\n", mt_errmsg()), 1;
    job jobs[4];
    pthread_t threads[4];
    for (int64_t i = 0; i < 4; i++) {
        jobs[i] = (job){ .runtime = m, .input = i + 1 };
        require("start a worker", pthread_create(&threads[i], NULL, work, &jobs[i]) == 0);
    }
    for (size_t i = 0; i < 4; i++) require("join a worker", pthread_join(threads[i], NULL) == 0);
    for (size_t i = 0; i < 4; i++) {
        assert(jobs[i].output == jobs[i].input + 10 && "each worker's answer");
        assert(jobs[i].own_error && "each worker saw only its own error state");
    }
    assert(mt_ok() && "no worker's error reached the main thread");
    assert(mt_one_int(mt_eval(m, E("+", 20, 22))) == 42 && "and the main thread still answers");
    mt_close(m);
    return 0;
}
