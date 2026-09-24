<!-- Purpose: retain each defect, failed experiment and correction found while
building the corpus. Open Obligations: the open issues below. -->

# Errors and repairs

## Repaired in CMeTTa while writing the twins by hand

Each row is a fix the C seat needed before a twin could say what its original
says, once the same thing worked in the Python seat or cmetta's own contract
promised it. Each landed in `extensions/cmetta` with a regression test, its
CHANGELOG entry and docs, and passed the seat's gate in a battery; each has a
provenance commit after it pinning its evidence tags.

| Witness | Expected and observed | Repair |
| --- | --- | --- |
| Copying `(= (sq $x) (* $x $x))` into another space through C | The variables to stay one variable; `(fact $u $u $w)` read back as `(fact $_ $_ $_)`, and `(sq 7)` there raised "ran backwards with more than one unknown" | `cce10b3`: name unnamed engine variables from a session counter |
| An answer compared with the atom that describes it | `(pair $x $x)` to match an answer up to renaming; C had only `mt_eq`, by variable name | `52d89c6`: `mt_alpha_eq`, proven against the engine's `=alpha` |
| Sorting answers in C, as msort does | An order over atoms; C atoms had none, so a program handed a list back to the engine to sort it | `27d508c`, `d1e3a98`: `mt_compare` and `mt_order` |
| `!(wrapper2 (+ 1))` from C | The partial application `(partial + (1))`; the cursor failed on a Prolog term with no MeTTa reading | `0733adc`, `eb4a073`, `0f7fd79`, then `65b02ca`: the shared wire grammar, with only native blobs held as handles |
| Parsing `(f $v0 ... $v15999)` | An atom; SWI aborted with "API error: invalid term_t 0" once the name walk's references filled the stacks | `6e91a33`: a frame per name lookup, and each walk's references bounded by its depth |
| Chapter 5's arithmetic run backwards | Answers keyed by variable, the Python seat's `solve()`; C had no counterpart | `c61a645`: `mt_solve` |
| A partial application stored in a C provider's space | The term the engine gave it back unchanged; `(match &np (stored $x) ($x 2))` answered `((partial + (1)) 2)` where the native space answers 3 | `256a3a6` |
| `(math-rational 1 (pow-math 2 200))` | The exact ratio; C refused the wide rational and answered nothing | `23bce3e`: `MT_BIGRATIONAL` and exact comparison at any width |
| A handle dropped on a thread with no Prolog engine | A release; the process died in SWI's `signalGCThread` | `25def05` queued such drops; `bcf14b8` erased on the dropping thread again once the required host carried the patch |
| `(pragma! max-stack-depth 20)` then `mt_eval` (chapter 14) | 120 and a StackOverflow error; 120 and a 1 GiB host stack overflow | `34f6aa6`: evaluate in the fuel scope a runnable form runs in |
| Effect classes named in C | One spelling; a hand enum and the generated one, held equal by a static assertion | `4d9802e`, `3f4d713`: `vocabularies.h`, generated from the engine's rows |
| Chapter 18's `05-matespacefast` | To finish, as under the CLI and the Python seat; a stack resource error after 31 s under SWI's 1 GiB default | `4798190`: boot under `settings.h`'s 8,000,000,000 bytes and restore that ceiling |
| memoize-exact called through cursors (chapter 18) | Hits on the second call; every call missed, `(entries 0)`, since each cursor is an engine of its own | `cfa188f`: `mt_run_goal`, eager in the runtime's own engine |
| A parametric space (chapter 19) | A handle on `(cache &primary-kb 100)`; `mt_space_open` refused every name without an ampersand | `237fcd3`: `mt_space_of` |
| Registering Prolog predicates (chapter 20) | The engine's one registration sequence, as the Python and Node seats cross it; C reached it only through the MeTTa-level `import_prolog_functions_from_file` | `f741cbe`: `mt_register_prolog` |
| A C space holding a program (chapter 20) | Equations that answer; the engine refused them, since no callback can promise rules | `4cae9c8`: `mt_provider.rules` |
| A tuple from a C array | One call; thirteen hand-written loops in twelve twins, most into fixed-size buffers | `4bb12d3`: `mt_array` |

Beside those, the gate and its measurements: `4bede71` runs the gate's
linking lanes after the lane that cleans and rebuilds the library, because
they raced and stranger-c failed `cannot find -lcmetta`; `4b7a44b` resolves
each bridge predicate once per runtime instead of interning it per call,
which had moved c-bench's cursor-step row past its band; `9d72d61` decodes
true and false by length and a fixed-size compare; `12432c3` re-pins the boot
row's governed QLF inventory from 28 artifacts to 26; and `697eff4` adds
`make runtime-halt-created-thread`, the reproduction of the host defect
below.

## Reported to the shared engine

The engine and its libraries are not this corpus's to change, so each of these
went to the MeTTa checkout's owner with its reproduction.

| Found by | Defect | Reported |
| --- | --- | --- |
| Chapter 8's unicode twin | NFKC-casefold keeps U+00AD, `(97 173 98)` for `a\u00ADb`, where Unicode 16's DerivedNormalizationProps removes it and utf8proc's own casefold answers `61 62` | 2026-09-24 07:11 |
| Chapter 8's HTTP twin | lib_http's server answers 500 to a request whose Accept field is `*/*`, libcurl's and browsers' default: SWI's `http_header` reads `*` as an unbound variable and `native_data/2` runs `=..` on it | 2026-09-24 |
| A partial stored through the Python seat's provider | `((partial + (1)) 2)` where the native space answers 3 | 2026-09-24 |
| Chapter 15's `04-admission_pools` | The MeTTa chain the example calls the builtin's executable specification reads only the first capacity row, `(accept)` where the builtin answers `(refuse (pool-at-capacity 2))` | 2026-09-24 11:39 |
| `mt_register_prolog`'s refusals | No `prolog:error_message` for `metta_control_signal(value \| type \| interrupted, _)`, and the value refusal row carries the JSON crossing's ground and remedy | 2026-09-24 15:38 |
| Chapter 22's derivation twins | lib_nars' and lib_pln's `LimitSize` never return at size 0 once the queue is empty: the test is false for `()`, and excluding the empty tuple's best candidate from `()` leaves `()` | 2026-09-24 18:41; upstream PeTTa 43705f5 has the same body, and a guard for the case that hung is planned |
| Chapter 22's tile puzzle | The original calls `add-unique-item-or-empty`, which nothing defines, so the start board is never recorded and the claim 181441 is 9!/2 + 1 | 2026-09-24 18:41; upstream makes the same call, and the example is to record the start and claim 181440, the C twin moving with it |
| Chapter 22's `04-matespace2` | `(superpose (collapse (match ...)))` walks its unevaluated argument, answering the symbol `collapse` before the match, where the Python twin's docstring describes a snapshot | 2026-09-24 18:41; the file is upstream's, so this is what it means, and the docstring is being corrected |

## Open

**SWI's halt passes over a thread still being created.** `thread_create`
marks a thread created before `pthread_create`, and `exitPrologThreads()` has
no case for that state, so the thread starts its goal while `PL_cleanup()`
frees the module tables its goal is looked up in. `make
runtime-halt-created-thread` in `extensions/cmetta` reproduces it without the
engine, and it dies 16 runs of 20 with eight threads. In the corpus it shows as
an original that uses lib_thread or a background loader exiting -11 after
every claim passed, under a parallel lane only: twelve serial runs of each
such program exited 0. It is the host's defect, and with the engine to be
replaced, no host patch is proposed.

**No refusal kinds in C.** cmetta reports every engine refusal as `MT_ERROR`
with its words, remedy and ground, but does not read the engine's
`metta_host_error_kind/3` table, which the Python and Node seats map to
classes, so a C program tells a missing source from a bad value by reading
prose. No twin needs the distinction yet.

## Corrected while writing the twins

| Witness | Error or mistaken expectation | Correction |
| --- | --- | --- |
| Chapter 22's NARS derivation twin | An expectation argument consumed the list its query argument read; C leaves the order of a call's arguments unspecified, and the engine was sent a list already emptied | The model's `limited()` copies the queue |
| Chapter 17's sorting twins | `mt_order` wrapped in a comparator that dereferenced the elements; one standalone run passed by luck, and the lane caught the next | Pass `mt_order` to `qsort` directly, as `cmetta.h`'s example does |
| A lane run after a failed build | 37/37 claims from a binary the failed build had left in place | Build with its status kept before reading a lane |
| `pragma!` | A truth value; it answers the unit | `modules.h`'s `pragma()` steps every answer and requires `mt_ok()` |
| `&rows` in a stored atom | A space reference; an `&name` atom decodes as a space only while it names one | Spell it as the symbol it is, `S("&rows")` |
| A compound literal passed to `mt_array` | One argument; the preprocessor split it at its commas | `mt_array` takes the array as its variadic part |
| `memoize energy` before its equation | True; memoize refuses a name that has no equation yet | Add the equation first, as the original's file loads its equations before its commands |
| Chapter 20's claim audit | `20-07` states nineteen claims, not twenty-one, and `20-04/04-import_error_surface` had no twin | Counted from the original's top-level asserts; the twin written |
| Chapter 5's backward-arithmetic twin, on a reviewer's reread | `#div` and `#mod` as C's `/` and `%`; those truncate toward zero and CLP(FD)'s floor, so the two disagree whenever the operands' signs differ | The table's column holds `floor_div` and `lowering.h`'s `floor_mod` |
| Chapter 8's segments twin, on the same reread | Four slots enough for the answers; nothing bounded the array, so a row with more separators would write past it | The array is sized by the row's length |
| `mt_alpha_eq(held, E(...))` in eight embedding examples | A comparison and nothing more; `mt_alpha_eq` borrows both arguments, so each expectation built in place was never released | `common.h`'s `alpha_equal` borrows the held atom and takes the expectation |
| Chapter 22's soft twin | One atom per argument of its `SCORE` macro; the macro named each argument twice, so `mt_keep` kept a second atom built only to leak, 154 blocks | A function, which evaluates each argument once |
| An answer printed (chapter 8), a space reference handed to a helper (chapter 17), a report's child kept (chapter 18) | Released by the call that used them; each call borrowed, and nothing dropped the owner | Each owner is named and dropped; `done()` now fails any program that leaves a block cmetta allocated |

## Before the rewrite: the generated corpus

Everything below records the corpus as `tools/generate.py` produced it from
`corpus.json`, with each twin pasting its original's MeTTa text into a C
string array. Both are gone, and every twin is now written by hand, but what
was found then stays true of that time.

### Repaired in CMeTTa

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

### Shared-runtime issues found then

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

### Corrected corpus mistakes

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

### Failed probes and setup corrections

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
