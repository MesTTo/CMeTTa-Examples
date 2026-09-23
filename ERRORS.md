<!-- Purpose: retain each defect, failed experiment and correction found while
building the corpus. Open Obligations: the two shared-runtime issues below. -->

# Errors and repairs

## Repaired in CMeTTa

These commits belong to `extensions/cmetta`, separately from this repository.
Each repair was followed by the C component's `check.sh` binding lane,
including its existing examples. The final component run also rebuilt through
`build.sh` and passed `make install-check`.

| Witness | Expected and observed | Repair |
| --- | --- | --- |
| Existing `examples/ops.c` | Four words; printed `word-count -> 0` and exited successfully because it called a different spelling from the registered `word_count` | `5c72fd4`: exact name and assertions for callback results and failures |
| Archive consumer | Build a static CMeTTa library; `make: *** No rule to make target 'libcmetta.a'. Stop.` | `83a61f2`: archive build/install and private pkg-config dependencies |
| First archive launch | Load SWI; `error while loading shared libraries: libswipl.so.10: cannot open shared object file: No such file or directory` | `83a61f2`: carry SWI's runtime library path in private link flags |
| Isolated component gate | Run component scripts; `cannot open ../../tools/bounded.sh: No such file` | `83a61f2`: build/test scripts invoke their own Makefile; the enclosing gate owns deadlines |
| Regex twin | Supply a grounded matcher; the header had no constructor for the engine's `matchable_value` / `custom_match` hooks | `43026df`: `mt_matcher`, retained iterators, binding/error propagation and abandonment tests |
| SQLite provider teardown | Release after closing provider and retained cursor; release count stayed zero because completed participant blobs kept their capture | `27777dc`: release completed transaction captures immediately; regression requires final-owner release |
| Effect-rank twin | Obtain the joined source effect and operation list without execution; general explain metadata lacked a complete C plan door | `3a173ba`: `mt_effect_plan` delegates to the shared source planner |
| New effect regression | Registered pure callback classified `pure`; received `EffectPlan oracleIO` | `3a173ba`: publish callback classifications in the shared catalog, compose overloads and withdraw owned rows |

## Shared-runtime issues left open

**Rational source spelling.** A constructed rational `-1/2` displays as `-1r2`,
but the engine reader parses that spelling as `Symbol`. `mt_write_dup` correctly
refuses a lossy round trip:

```text
swrite/2: cannot write -1r2 as MeTTa text because its printed form would read back as a different value (printed form would read back as a different value)
```

The same writer limitation rejects `1.0Inf`. Language extraction therefore keeps
the original source text for non-round-trippable forms. `data/exact_numbers.c`
uses `mt_rational` and checks numerator/denominator fields and exact arithmetic.
The reader and writer belong to the shared engine, outside the permitted edits.

**Intermittent Linda shutdown.** On SWI-Prolog 10.1.14, full embedded cleanup can
abort after the assertions pass:

```text
ERROR: ./src/pl-thread.c:5141: destroy_message_queue: Assertion failed: !queue->waiting && !queue->wait_for_drain
```

The initial corpus sweep recorded exit `-6` for `thread_linda`. A separate C
probe linked only to `libswipl`, loaded the same engine and Linda program through
`PL_call`, then called `PL_cleanup(0)`: one of thirty runs reproduced the same
assertion, and twenty command-line `swipl` runs passed. This establishes that
libcmetta is not required for the failure; it does not determine whether SWI
cleanup or the shared Linda implementation must change. Neither is editable
here. The completed corpus sweep passed all 361 programs, including Linda.
The final expanded sweep also passed all 362 programs. The corpus keeps normal
cleanup and reports a future recurrence as a failure.

## Corrected corpus mistakes

| Witness | Error or mistaken expectation | Correction |
| --- | --- | --- |
| Source-form probe | Expected a separate `!` form; `mt_forms` returned the two form bodies | Assert the documented unevaluated body list in `basics/source_forms.c` |
| Family carrier selection | `mt_eval_under: algebra \`provenance' does not exist (declare an (algebra ...) row in &metta before selecting it)` | Select actual `prov`, `ranked` and `prob` carrier names |
| First full sweep | 326/343 passed; fifteen fixture-dependent programs could not resolve their original source paths | Commit the required fixture bytes and rewrite paths in `corpus.json`; drift-check every fixture |
| Relative import | Remaining Python-only empty import fixture could not load from a C host | Use an empty MeTTa fixture and document the host-language boundary |
| Restricted-space policy | Expected the test file to exist; the old source path did not | Point the policy assertion at the committed `corpus.json` |
| Large matespace | `Stack limit (1.0Gb) exceeded` while collapsing all answers | Stream and count all 1,572,862 answers through the C cursor |
| SQLite compilation | `incompatible types` from treating `mt_write_dup`'s `mt_string` result as a pointer | Bind its counted bytes with `sqlite3_bind_text64`, then release its data |
| OpenBLAS headers | ISO C pedantic diagnostics for `_Float16` in dependency headers | Mark only the external pkg-config include path as a system include |
| Matcher regression | C11 `true`/`false` macros selected integer coercions through `_Generic` | Construct explicit `mt_bool` values |
| Durable observer | Subscription refused because the provider had no declared event contract | Declare `per-write-exactly ordered`; all writes pass through engine mutation hooks |
| Effect example | Generic metadata projection returned effect `none` without an operation item | Use the new source-effect plan API and test all five ranks |
| Provenance example | Correct commutative sum arrived in the other order | Assert either ordering of the two expected evidence products |
| Runtime settings | `mt_add` retained both 7 and 3; it is bag insertion, not replacement | Remove the owned prior row before adding its replacement |
| Coverage audit | A substring test for `c_space` accidentally excluded `parametric_spaces` | Restore the example; commit an explicit roster with twelve reasoned boundaries |
| Git fixture audit | The inherited default created scratch repositories under `./repos` | Pass `./ai-tmp/repos` explicitly to both the fixture and the import |
| README audit | Illustrative first-step count said 4; executable reported 12 | Copy the observed result, `OK first_steps (12 checks)` |
| Nested roster audit | Two algebra demonstrations had been grouped with fixtures | Map both explicitly and add the dual-number derivative example |

The missing-fixture cases were compression, conformance, Fibonacci import,
foreign rules, Git import, duplicate/cyclic imports, import order, relative
imports, imported space identity, include, reference loading/maps, predicate
registration, module doors and the Prolog rung. No failing program was removed
from the runner to obtain a passing result.

## Failed probes and setup corrections

- Loading libcmetta through Python `ctypes` first produced
  `undefined symbol: PL_new_atom` in SWI's `uuid.so`. Retrying with global symbol
  visibility produced `Fatal Python error: PyImport_AppendInittab:
  PyImport_AppendInittab() may not be called after Py_Initialize()`.
  These probes were replaced by ordinary C executables; embedding into an
  already initialized Python interpreter is not demonstrated by this corpus.
- Python 3.11 inventory parsing reported `SyntaxError: invalid syntax` for
  PEP 695 syntax. Inventory extraction used the installed Python 3.13.
- pkg-config reported `Package 'sqlite3', required by 'virtual:world', not found`
  and the equivalent missing `libcurl` package during dependency inspection.
  The corpus needs SQLite, not libcurl. The installed SQLite runtime was paired
  with an official development-package header extracted inside this repository.
- The initial commit reported `Author identity unknown`; local repository
  identity was configured before committing. An existing battery path was
  reused after checking its occupancy instead of creating another tree.
- Record creation encountered the existing `corpus` identifier; the existing
  goal was retained. A later write reported `unrecognized arguments: --id`;
  the documented positional identifier succeeded.
- An exact source lookup reported
  `rg: lib/lib_gitimport.metta: IO error for operation on lib/lib_gitimport.metta: No such file or directory (os error 2)`.
  The actual source is `lib/lib_gitimport/lib_gitimport.pl`.
- No C language-server provider was exposed by the available tool metadata.
  Source reads and repository searches supplied navigation.
- A provenance edit read Git's quoted Unicode fixture name as a literal path,
  producing `FileNotFoundError: [Errno 2] No such file or directory`. The
  incomplete patch was rejected with `The last line of the patch must be
  '*** End Patch'`; a numeric hunk header then produced `Failed to find context
  '-30,3 +30,3 @@'`. NUL-delimited changed paths and plain hunk markers fixed
  the edit. No partial source change was accepted.
- A lookup using `engine/*algebra*` reported `No such file or directory`.
  The actual contract was read from the existing algebra tests and catalog.
  Header review also corrected the derivative draft to use `mt_float` and
  `mt_fail(call, message)` before compilation.
- The staged whitespace check reported `new blank line at EOF` in
  `borrowed_storage.c`, `daemon.c`, `native_iterator.c`, `reentrant_events.c`,
  `standing_queries.c`, `space_lifetime.c` and `pln_uncertain_reasoning.c`.
  Removed only their surplus trailing blank lines before committing.
- A post-commit drift check initially selected the verification tree's own
  `HEAD` instead of the new repository revision. The output exposed the older
  snapshot; checkout and drift checks were repeated using the explicit commit.
- A byte-exact provenance audit reported `AssertionError: basics/atom_values.c`:
  the patch application also normalized surplus EOF whitespace in earlier files.
  Comparing after that whitespace normalization confirmed that every change
  was an evidence reference or trailing whitespace; no program body changed.

The duplication audit found one six-line clone, 0.2% of handwritten code:
the include, identity callback and opening of `main` in the annotation and
effect examples. Keeping those local makes the callback ownership visible in
both examples. Shared SQLite lifecycle code lives in `support/sqlite_store.c`.
