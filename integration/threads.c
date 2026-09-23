/* Purpose: Attach joined C workers and isolate their error states.
 * Owns resources: local handles and host storage are released before success;
 *   a failed assertion terminates this example process.
 * Guarantees: results are asserted [tested: make check; commit=WORKTREE].
 * Open Obligations: None.
 */
#include "common.h"
#include <pthread.h>
typedef struct { metta *runtime; int input; int64_t output; } job;
static void *worker(void *user)
{
    job *j = user;
    check("attach worker", mt_thread_attach());
    mt_clear();
    j->output = mt_one_int(mt_eval(j->runtime, mt_expr("+", j->input, 10)));
    check("worker result", mt_ok() && j->output == j->input + 10);
    if (j->input % 2) {
        mt_atom *wrong = mt_sym("not-an-integer"); (void)mt_int(wrong); mt_drop(wrong);
        check("worker owns its conversion error", mt_error() == MT_MISUSE);
    }
    mt_thread_detach();
    return NULL;
}
int main(void)
{
    metta *m = open_engine();
    job jobs[] = {{m, 1, 0}, {m, 2, 0}, {m, 3, 0}, {m, 4, 0}};
    pthread_t threads[sizeof(jobs)/sizeof(jobs[0])];
    for (size_t i = 0; i < sizeof(jobs)/sizeof(jobs[0]); ++i)
        check("start worker", pthread_create(&threads[i], NULL, worker, &jobs[i]) == 0);
    for (size_t i = 0; i < sizeof(jobs)/sizeof(jobs[0]); ++i)
        check("join worker", pthread_join(threads[i], NULL) == 0);
    check("worker errors did not cross threads", mt_ok());
    check("main thread remains usable", mt_one_int(mt_run(m, "!(+ 20 22)")) == 42);
    return done(m, "threads");
}
