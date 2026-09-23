<!-- Purpose: record what changed in the corpus and why. -->

# Changelog

## Unreleased

- Write chapter 8's system twin in C over the process the engine shares
  with it: C reads the engine's environment writes with `getenv`, the whole
  environment as `environ`, the working directory with `getcwd`, the cores
  with SWI's own `sysconf` call, and runs each write before it looks.

- Write chapter 8's encoding twin in C: UTF-8 through utf8proc, hex by hand
  in C, and base64 through libcrypto in the standard and url alphabets. The
  base64 twin 06 decoded privately now lives in `crypto_oracle.h` beside its
  hex, for both twins.

- Write chapter 8's markup twin in C against libxml2: a strict parse that
  refuses what a recovering one would repair and any external entity,
  libxml2's serializer for writing, and the selector language walked in the
  order SWI's `xpath/3` enumerates. A bare HTML fragment is read the way SWI
  reads one, nesting unclosed tags, which libxml2's recovering XML reader
  also does; its HTML parser follows HTML's tree builder instead.

- Write chapter 8's YAML twin in C against libyaml, the library under SWI's
  yaml package: event parsing so an untagged plain scalar is told from a
  tagged or quoted one, the YAML 1.2 core schema's resolution table as the
  POSIX regular expressions the specification gives, and the event emitter
  with the settings SWI's writer passes.

- Write chapter 8's parsing twin in C: a list-of-successes parser, Hutton and
  Meijer's model, run by a table of forms that C extends at run time as the
  original adds a `parsing-form` row, with functions inside grammars resolved
  through a C table and a stalled repetition raised as a flag the parse
  reads. `check_value`, the check that a value Empty is no answer at all, now
  lives once in `common.h` for every program.

- Write chapter 8's shipped-library twins 25 and 26 in C: graphs as a sorted
  vertex array and an adjacency matrix rebuilt from vertex and edge lists,
  with Warshall's closure and Kahn's layered topological order; and Unicode
  against utf8proc, the library the engine's own binding links, through its
  named normalization functions, `utf8proc_map` with the same refusals, the
  property table read as SWI's binding reads it, and its grapheme breaks.

- Write chapter 8's shipped-library twins 23 and 24 in C: sets as children
  kept in the standard order, with membership by `bsearch` and the four
  combinations as one merge keeping chosen Venn regions; and pairs as a
  relation read with projections, a stable insertion sort, groups as runs of
  equal keys and lookup by term identity. Twins 21 and 22 no longer leak the
  temporaries they handed to borrowing helpers.

- Write chapter 8's shipped-library twins 21 and 22 in C: the sorted map and
  the priority queue against rows C keeps in the standard order with
  `mt_compare`, Okasaki's two-stack queue, and the finger tree's sequence as
  an array; and the functional utilities against C operations over functions
  that answer any number of values, walked depth first so a branching
  function gives C the engine's alternatives in the engine's order.

- Write chapter 8's shipped-library twins 18 to 20 in C: strings against
  utf8proc over codepoints with Wagner-Fischer, SWI's ISub, its paragraph
  filler and its interpolation spelled in C; files against POSIX, with C's
  own `FILE*` beside each engine handle, `fts(3)` for walks, `fnmatch(3)`
  under CPython's glob selector for globs, `realpath(3)` and posixpath's
  lexical rules; and spaces against C's model of each space matched with
  `mt_unify`, under the multiset and remove-every-copy rules measured on the
  engine.

- Write chapter 8's shipped-library twins 14 to 17 in C: reflection with C
  sets over the engine's enumerations, cJSON over `surface-json` and a C
  rewrite walk for `atom-replace`; the finger tree's internals against the
  Hinze-Paterson operations in C; the libraries' underscore spellings against
  the same oracles as their libraries' own twins; and CSV against an RFC 4180
  reader and writer in C over the bytes on disk. The oracles two twins share
  now live once, in `pcre_oracle.h`, `crypto_oracle.h`, `time_oracle.h` and
  `conformance_report.h` beside them.

- Write chapter 8's first thirteen shipped-library twins in C, each library
  held against the C facility that does the same job: `string.h` and `stdio`
  for text and files, PCRE2 for regex with `pcre2demo.c`'s global loop, cJSON
  for JSON documents, paths, lines and files, libcrypto for digests, HMACs
  and the PBKDF2 password records C verifies from their own fields, `time.h`
  for calendars, a C ring buffer for the finger tree, C tables for
  documentation, the textbook algorithms and a decimal bignum for
  combinatorics, a C map for dictionaries, and GMP for vectors rounded once
  exactly as the library rounds them.

- Link a program's C libraries through one rule: a program names its
  pkg-config packages in `PACKAGES`, and the build rule compiles against and
  links them, so the per-program flags the OpenBLAS example spelled out are
  one line each. The library twins of chapter 8 link PCRE2, cJSON, libcrypto
  and GMP this way.

- Run each twin from the engine tree, where its original runs and where the
  Python lane runs its twins, so a path an original writes relative to the
  tree, a fixture beside it, names the same file in C. Twins ran from this
  repository's root, where `09-conformance`'s `./examples/...` provider could
  not be found.

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
- Write chapter 6's ten twins in C: a C function returning MT_FAIL is
  (empty), collapse is mt_all, once is mt_first, the permutation facts and the
  triangular join are generated by loops and the answers counted off a lazy
  cursor, the stream algebra is computed a second time by C over arrays, and
  the nondeterministic write is one mt_add_all batch read back by name.
- Write chapter 7's forty-two twins in C. Conditionals are checked against
  C's own ?:, && and ||, truth tables are C loops, case is a switch, =? and
  if-equal2 are held against mt_unify and mt_eq, let* is a block of
  declarations and the walrus an assignment expression, generators are C
  iterators behind mt_answer_iter, take is a loop abandoning a lazy cursor,
  and the recursive functions are one macro body expanded both to a C
  function and to the equation mt_lower installs. The lane now reads the
  corpus's claim helpers from common.h instead of listing them, and a twin
  rebuilds when a header it shares with its neighbours changes.
- Write chapter 8's first section, twenty-one twins over atoms, lists and
  folds. Folds, maps and filters are held against C loops over the same
  arrays through function pointers; alpha membership and dedupe against
  mt_unify and mt_alpha_eq; sorting against qsort with mt_order; atom-subst
  against mt_unify and mt_substitute; closures are mt_function values
  carrying their context; and the partial applications print through the
  handles cmetta holds them as.
- Write chapter 8's sequence-variable section, five twins sharing
  segments.h, where seg("x") spells (:seg $x) and a run is a slice view of
  the children. The splits a two-gap pattern enumerates are checked against
  the separator positions a C loop finds, and a caught refusal is read apart
  as the original reads it.
- Read partial applications, refusal payloads and residual goals as the
  expressions CMeTTa now decodes them as, in the wire grammar every seat
  shares: the twins compare them against terms C builds and read a payload
  by position with mt_at, where they held opaque handles.

## Initial corpus

- Add 362 asserted C programs in the eight Python example directories: 311
  language twins and 51 handwritten host examples.
- Add explicit lifetime, allocation, callback reentry, SQLite, BLAS, POSIX,
  shared extension and installed consumer examples.
- Add discovery-based builds, an always-enabled assertion helper, per-program
  results, fixture and source drift checks, and generated coverage tables.
- Document CMeTTa repairs, host-specific coverage limits and the independently
  reproduced SWI-Prolog cleanup failure.
