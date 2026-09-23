<!-- Purpose: record what changed in the corpus and why. -->

# Changelog

## Unreleased

- Replace the 311 generated language twins with twins written by hand in C,
  each at its MeTTa original's path. The generated twins pasted the original's
  source into string arrays and ran it; `tools/generate.py` and `corpus.json`
  are gone with them, and the fixtures they carried stay as files.
- Add `tools/twin_lane.py`, which runs each original through the C seat and
  its twin in its own process and requires the twin to prove as many claims,
  to define what the original defines, and to leave the same atoms in `&self`;
  `tools/twin_lane_selftest.py` plants eight twins to prove it refuses drift.
- Rewrite the embedding examples to build terms with the constructors rather
  than parse MeTTa text, and split `require`, a status a program needs, from
  the claims it proves.
- Write chapter 5's twenty-six twins in C: equations built as terms or
  lowered from one-body macros beside the C function they mirror, the -math
  family and the bit operations checked row by row against libm and C's own
  operators, seeded draws compared like two runs after srand, and arithmetic
  run backwards through CMeTTa's new mt_solve, which reads each unknown by
  name. The lane no longer compares clauses the specializer derives, whose
  keys differ between a source call and a C-built one; its selftest proves
  that exclusion reaches only a clause whose own head carries the mark.

## Initial corpus

- Add 362 asserted C programs in the eight Python example directories: 311
  language twins and 51 handwritten host examples.
- Add explicit lifetime, allocation, callback reentry, SQLite, BLAS, POSIX,
  shared extension and installed consumer examples.
- Add discovery-based builds, an always-enabled assertion helper, per-program
  results, fixture and source drift checks, and generated coverage tables.
- Document CMeTTa repairs, host-specific coverage limits and the independently
  reproduced SWI-Prolog cleanup failure.
