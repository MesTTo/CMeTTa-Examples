/* Purpose: run one MeTTa original through the C seat, the way its twin runs,
 *   and print what the twin lane compares: every answer row, the inferences
 *   the program spent, and lane_report()'s description of &self.
 * Assumes: the working directory is the engine tree, because originals name
 *   their fixtures relative to it, and argv[1] is an original's path.
 * Guarantees: exits 0 only when the whole program loaded, which is when every
 *   assert-family form in it held, since a failing one raises
 *   [source: engine/metta/runtime.pl, test/3 throws metta_test_failed;
 *   commit=49e2b250d451d01715f78ab91465e2d1842dee8d].
 * Owns resources: the runtime, closed before exit.
 */
#include "../lane.h"
#include <inttypes.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s examples/<chapter>/.../<name>.metta\n", argv[0]);
        return 2;
    }
    metta *m = mt_open(NULL);
    if (!m) {
        fprintf(stderr, "LANE-ERROR the engine did not open: %s\n", mt_errmsg());
        return 1;
    }
    uint64_t before = mt_stats_now(m).inferences;
    mt_clear();
    mt_answers *answers = mt_load(m, argv[1]);
    if (!answers) {
        fprintf(stderr, "LANE-ERROR %s\n", mt_errmsg() ? mt_errmsg() : "load refused");
        mt_close(m);
        return 1;
    }
    mt_rows (row, answers) printf("LANE-ROW %zu %s\n", row->group, row->text);
    if (!mt_ok()) {
        fprintf(stderr, "LANE-ERROR %s\n", mt_errmsg());
        mt_close(m);
        return 1;
    }
    printf("LANE-INFERENCES %" PRIu64 "\n", mt_stats_now(m).inferences - before);
    lane_report(m);
    mt_close(m);
    return mt_ok() ? 0 : 1;
}
