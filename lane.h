/* Purpose: the space report both sides of the twin lane print, so a twin and
 *   the runner of its original canonicalise atoms through one function.
 * Guarantees: see lane.c.
 */
#ifndef EXAMPLES_LANE_H
#define EXAMPLES_LANE_H
#include <cmetta.h>

/* Print LANE-HEADS, LANE-OPS, LANE-HELD, LANE-HASH, LANE-EQUATIONS,
   LANE-DATA-HASH and, up to a cap, LANE-ATOM and LANE-EQUATION lines
   describing &self. Leaves the error state clear. */
void lane_report(metta *runtime);
#endif
