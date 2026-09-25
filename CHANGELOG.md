<!-- Purpose: record what changed in the corpus and why. -->

# Changelog

## Unreleased

## 2026-09-26

- The verification record is written from the run it describes, and names
  no path on the verifying machine. `tools/verification.py`, run as `make
  verification GATE=<gate output> LOG=<make output>`, writes VERIFICATION.md's
  identity paragraph from both checkouts' commits and the cmetta version the
  C seat gate printed, its host block from the run's own `swipl` (its
  version, the build its host declaration names and how many patches it
  declares) and the compilers' version lines, and its Results table, each
  cell the verdict line that step's own tool printed in the make or gate
  output; copies the lane's receipts; and writes the gate's output with the
  engine's and the corpus's roots repository-relative. It writes nothing
  unless the corpus checkout holds its commit's files alone, the engine
  checkout has no tracked change, the make output built every program and
  translation unit, each receipt holds the count its tool printed, and no
  quoted line names another home-directory path. VERIFICATION.md named the
  patched SWI by the directory it is installed in and component.txt an
  install prefix inside a battery, both paths only one machine has, which
  the release refuses to publish, and its identity paragraph and Results
  table were written by hand. `make check` now runs `tools/verification.py
  --check`, refusing any tracked line naming a path in a home directory or a
  record missing one of its three blocks, and the writer's self-test.

- VERIFICATION.md records the branch verified from a clean copy of
  `b094c80` on superproject `3a6b92b40` with the engine change `8cc5659d0`
  applied, which makes a partial function value callable where Prolog's
  `maplist/3` calls it: without it 05-lambda's claim about a lambda around a
  C function value fails on that superproject, here and on main alike. Every
  step of the corpus passes, 323/323 twins and 3891/3891 claims with stored
  content carried 50, declared 2, equal 271, no finding of the source rule,
  the self-test 26/26, and the source rule's 51 uses of MeTTa text all in
  files whose `text:` note says why. The C seat's `c-bench` fails its
  `error-ball` row the same with and without the engine change. Laid over
  `64a5bd1`, the same lane fails the 72 twins that lowered through
  `mt_lower` and `operations/lowering.c`, their claims and stored content
  still agreeing. The receipts are that run's.

- The twin lane's source rule reads each program's translation unit, which
  `make all` now writes beside the binary as `build/<path>.i` with the
  program's own flags, for the doors that hand the engine MeTTa source: every
  function `cmetta.h` declares with a C string named `source`, or answering a
  program's answers from a `path`, read off the header the unit includes,
  and the heads `parse`, `sread`, `parse-command`, `trace-source` and
  `observe-source`, each attributed to the corpus file the preprocessor
  names. A door a macro spells is found where it expands, so `mt_lower`'s
  `mt_do` is a finding and the scan's exemption for it is gone, a header's
  door is named in the header, and `common.c` and `lane.c`, linked into
  every program, are read as well; `common.c`'s `fail` joins the claim
  helpers whose strings are text. Only a file whose `text:` note says why
  may use one. The self-test plants a twin lowered through `mt_lower`, a
  door in an included header, a `parse` of a text atom and a declared text
  twin, 26 cases in all.

- Every twin builds its equations as atoms. The 142 `mt_lower` and
  `mt_lower_raw` calls in 65 twins, in `fib.h` and `fibsmart.h`, which seven
  more twins include, and in `operations/lowering.c` stringified MeTTa tokens
  into one `mt_do` source string the engine parsed; each is now `mt_add` of
  an `(= head body)` atom, a one-body macro expanded with `lowering.h`'s `T_`
  operators and every other body built with `E` and `V`, so one body still
  compiles to the C function and builds the equation. `lowering.h` drops its
  `M_` operators and the twins their local ones, `06-if_branch_binding`'s
  `let*` names its variable through `T_LET`, and `11-reference_rows` reads
  its offsets from the enum C checks against, which the token form could
  not. On superproject
  `2ffb3fb39` the lane reads what it read before the change: 323/323 twins
  agree, 3891/3891 claims, stored content carried 50, declared 2, equal 271.

- Follow 31-system_lib's family claim, which the MeTTa corpus now states as
  one of the four families a platform flag names, `"windows"`, `"apple"`,
  `"unix"` and `"emscripten"`, where it pinned `"unix"`. The twin evaluates
  the original's `is-member` form over a list built from its own `families`
  array, and derives the answer from `family()`, which now reads the macros
  SWI sets its flags from, `_MSC_VER` or `__MINGW32__`, `__EMSCRIPTEN__`,
  `__APPLE__` and `__unix__`, and answers `"unknown"` for none. It read
  `__unix__` alone, so on macOS it expected `"other"`, a word lib_system
  never answers, where the library answers `"apple"`. One `listed()` now
  decides both of the file's membership tests.

- VERIFICATION.md records the corpus verified on superproject `99bd67a73`,
  the chapter 22 fixes' tree on `swipl-patched.5`: every step passes,
  323/323 twins and 3891/3891 claims, with that run's receipts.

- ERRORS.md closes SWI's halt race. The host went live as
  `swipl-patched.5` in superproject `622e425d4`, carrying
  `swi-halt-passes-created-thread`, and cmetta's
  `make runtime-halt-created-thread`, which died 16 runs of 20 on
  `swipl-patched.2`, halts cleanly 20 runs of 20 on it. The report moves
  from Open to the table of reports with its outcome, and `done()`'s
  comment on why it flushes before closing names the race as past.

- Follow the chapter 22 originals the library and example fixes of
  superprojects `7f373da2b` and `3488b9753` changed. lib_nars' and
  lib_pln's `LimitSize` now treat the empty queue as its own limit, so
  `(LimitSize () 0)` answers `()` where it recursed forever; the NARS and
  PLN derivation-control twins claim it, and the derivation whose queues
  are bounded at 0, `(() ())`, as their originals do, and `derive.h`'s
  comment says what the library does now. C's model already stopped at an
  empty queue. `derive()` and `engine_derive()` take the two queue sizes as
  one `bounds`, which the five-argument derivation needs, and the PLN
  twin's `run_loop` wrapper goes. The tile puzzle's original records its
  start in `&dup` with `add-unique-or-fail` and claims 181440, 9!/2; the
  twin's `bfs_all` does the same, and C's model records the start before
  its search.

- VERIFICATION.md records the corpus at `f8d79ba` verified on superproject
  `3f3d8107a`, whose admission fix gave chapter 15's original a two-row
  section: every step passes, 323/323 twins and 3887/3887 claims, with
  that run's receipts. ERRORS.md closes the admission report: the fixed
  chain and `space-admission-verdict` agree across six capacity-row
  orderings, where the old chain asked beside them still admits at rows
  `(5 2)` and `(5 3 2)`.

- Hold chapter 15's admission twin to its original's two-row section, which
  superproject `47855fa71` added when it made the original's MeTTa chain
  walk every capacity row as `space-admission-verdict` does. The twin's own
  two-row check used rows 3 and 2; it now uses the original's 5 and 2 over
  the two tickets held, proves both new claims, that the builtin answers
  C's verdict and that the chain refuses at `(pool-at-capacity 2)`, and
  checks the four row writes as it checks the original's others. Its header
  no longer says the original's chain reads only the first row.

- VERIFICATION.md records the corpus verified against the verdict rename:
  `d4affe3` from a clean copy on superproject `f9c56dbba` as pinned, the
  C seat at `8211c57`, where every step passes, 323/323 twins and 3885/3885
  claims among them, with the receipts replaced by that run's. The repository
  checks now search what they are about: the program-text grep reads the C
  sources, and the generator check asks git whether `tools/generate.py` or
  `corpus.json` is tracked and greps the sources, the Makefile and `tools/`,
  so neither matches the prose describing it. ERRORS.md records the halt
  race taking chapter 20's `13-reference_loading` a second time, in the
  fourth run, and the README links I wrote relative in the C seat, which its
  site page could not follow, now URLs in cmetta `8211c57`.

- Answer the judges' verdicts as the engine's capitalized constructors,
  `(Accept)`, `(Accept <atom>)`, `(Refuse <words>)` and `(Drop)`, which the
  engine reads since superproject `5bae989df`: a lowercase `(drop)` is a
  call wherever a library defines `drop`, as lib_functional does.
  `verdicts.h`'s three builders are the one place the corpus spells a
  verdict, so the five twins that answer one, chapter 9's typing rules,
  chapter 15's pre-add, admission and post-add hooks and chapter 20's
  translator refusal, follow it with no edit of their own; the comments
  that spelled the lowercase heads in `judges.h` and the admission twin
  follow too, and both headers cite the engine at `5bae989df`.

- VERIFICATION.md records the hand-written corpus's verification from a
  clean copy of `4fe7740`: the C seat's seven gate lanes, `make surface`,
  `all`, `check` and `check-consumers`, the lane's source rule over every
  tracked C file and the greps for generated twins, with the three runs it
  took and what the result does not cover. Its receipts are replaced by that
  run's: `verification/results.json` for the embedding programs,
  `verification/twins.json`, new, for the lane's verdict on each twin, and
  `verification/component.txt` for the seat's gate output.

- ERRORS.md's open entry for SWI's halt race now names where the fix is:
  a `PL_THREAD_CREATED` case in `exitPrologThreads()`, in the native host
  build `swipl-patched.5`, which becomes the live host after gate-perf
  lands. The gate runs `swipl-patched.2` until then, and the entry stays
  open.

- Record in ERRORS.md the C seat's `f76b44e`, which keeps version-control
  metadata out of `make install`. `make check-consumers` installs the seat
  into `build/prefix`, and the install had carried lib's `.git`, so `git
  clean -fdx` left `build/prefix/share/metta/lib` behind as a nested
  repository.

- Build the installed consumers again. `lane.c` copied the published
  operations' names with `strdup`, which is POSIX, and `make
  check-consumers` compiles the helpers as plain C11, so both consumers
  failed with an implicit declaration; the names are copied through
  `lane.c`'s own text builder now, as `head_key` copies its keys. `done()`
  and the originals' runner flush stdout before closing the engine, so a
  close that crashes, which SWI's halt can do when the engine created a
  thread just before (ERRORS.md, Open), still hands the lane the claims
  and the space report instead of discarding them with the pipe's buffer.

- Refuse a program that leaves an atom unreleased. `open_engine()` gives
  cmetta a counting allocator on its thread, and `done()` fails a program
  if any block cmetta allocated for it outlives `mt_close`. A program that
  releases everything leaves zero, so a leak is counted, never guessed.
  Twelve of the 374 programs leaked. The four twins: the alpha-member twin
  printed an owned answer it never dropped; the class twin handed a fresh
  space reference to a helper that only borrows; the tabling twin kept a
  child of a report it never dropped; and the soft twin's `SCORE` macro
  named each atom twice, building it again inside `mt_keep`. The eight
  embedding examples compared with `mt_alpha_eq` against an expectation
  built in place, which it borrows, so `common.h` gains `alpha_equal`,
  which borrows the atom a program holds and takes the expectation. The
  lane's self-test plants a twin that keeps an atom and is refused, and its
  own short-claims twin no longer leaks.

- Fix what a C reviewer reading ten twins across the chapters found. The
  backward-arithmetic twin computed `#div` and `#mod` with C's truncating
  `/` and `%`, which agree with CLP(FD)'s floored ones only for positive
  operands; its table now holds C functions that floor. The segments twin
  collected an answer per separator into a four-slot array nothing bounded;
  it is sized by the row now. The hyperpose twin built its list of numbers
  through a helper that only adapted `N` to a mapping, which `mt_array`
  replaces. The other seven read as written C.

- Keep ERRORS.md true to the hand-written corpus. It opens with the C seat
  repairs this corpus needed, each with the twin or probe that showed it and
  the commit. Then come the defects reported to the shared engine with what
  became of each, the two issues still open (SWI's halt passing over a
  thread still being created, and no refusal kinds in C), and the mistakes
  corrected while writing the twins. What was found while the corpus was
  generated follows under its own heading.

- Write README.md in the Python seat's shape. Six real pairs, shortest twin
  first, each MeTTa original above its C twin: a collapse, fib under a
  budget, a parametric type, an equation as one body, a C builtin in place
  of the Python seat's notation section, and a pattern read by a C function.
  Then running them, the reading order, what each chapter teaches and where
  a twin cannot follow. `tools/index.py` copies every pair from the files
  between its markers, the original without its comments and the twin
  without its obligation header, and refuses pairs out of shortest-first
  order. It derives the where-a-twin-cannot-follow list from `residue.json`
  and every twin's lane notes, read by the lane's own parser, so none of it
  is kept by hand.

- Read a twin's lane notes as comment fields. The lane took `text:`
  anywhere in a comment as a note, so prose such as "reads it as text: a
  String" exempted six twins from the source rule, and each reason ran on
  into the next field. A note is now written as an obligation header writes
  a field: the comment's opening or a line's asterisk, one space, the label
  and a colon, with its reason running to the next field. One twin relied on
  the accident: the parsing twin hands lib_parsing arithmetic text such as
  `"(1+2)"`, and it now declares that subject in a `text:` field of its own.
  The self-test plants a twin whose comment says "text:" in a sentence and
  runs source through `mt_do`, which the old reading let through.

- Write chapter 22's search twins in C. The tile puzzle's twenty-four move
  equations are one geometry C writes. Its breadth-first search is C's over
  the same geometry, and it dequeues the start twice, because
  `add-unique-item-or-empty` is defined nowhere and so never records it.
  `matespace.h` models the matespace programs' space and the order the
  engine evaluates them in, measured against the engine round by round. In
  the second program, `(superpose (collapse (match ...)))` is not a
  snapshot: superpose walks its unevaluated argument, answering the symbol
  `collapse` before it reaches the match. The model reproduces that at every
  round count tried and gives the originals' 1,063,919 and 1,297,533. The
  energy function is one body for C and its equation, and the added
  Fibonacci is `fib.h`'s. These twins need extensions/cmetta cf925da.

- Write chapter 22's weighted answers in C: fourteen twins held to C's
  models of the reasoning libraries. `nars_truth.h` is lib_nars's truth
  functions in the library's own operation order, so every truth a rule must
  answer matches C's in the last bit. `pln.h` is lib_pln's formulas. The
  deduction and consistency conditions 03 and 04 define are one body each,
  for C and for their equations, and lib_pln's `/safe` divisions are C
  functions that answer whether they have a value. `derive.h` is the loop
  NARS and PLN share: sentences, stamps, the bounded priority queue and the
  step budget, run in C over C's rules and held to the engine's queues.
  `measure.h` and `soft.h` model lib_measure's weighted superpositions and
  lib_soft's weak unification. The query twins hold each answer to the proof
  its stamp names: 06's is the revision of two PLN deductions, and 09's is
  an abduction, a deduction, a revision and a deduction through the smoking
  rule. The posterior twin is the exhaustive definition lib_statistics'
  tests compare against, in exact ratios. The independence twin is
  lib_pln2's walk over the support groups. Both libraries' `LimitSize` never
  returns at size zero once the queue is empty; C's model stops there.

- Write chapter 22's logic programs in C: eight twins. The relations are C
  tables turned into equations, and each rule is the term it is. C decides
  every answer from its tables: the letters below d in its alphabet, the
  members a constrained list must hold, which negations hold and which leave
  constraints, the Scallop README's five programs and each `not-provable`
  over nested case arms. The backward chainer's five recursive clauses are
  one `chain_clause(n)` over one to five premises, searched over Metamath's
  demo0 in three knowledge bases C fills. The tagged fixpoint closes the same
  rules in C under each carrier with `semirings.h`, including the exact
  probability over the possible worlds of the edges.

- Turn C arrays into expressions with cmetta's new `mt_array`. Twelve chapter
  8 twins built a tuple from a C array of bytes, codes or integers with a
  loop of their own, most of them into a fixed-size buffer, and the database
  twin wrote the loop inline. Each is now one `mt_array` call. The crypto
  twin's zero-length case is `mt_unit()`, since `mt_array` reads the element
  type from the array. The vector twin's two literal rows are `E(1, 1, 1)`
  and `E(0, 0)`, which count themselves. These twins need extensions/cmetta
  cf925da.

- Spell MeTTa's `/`, `<=`, `min`, `max` and `and` in `lowering.h`, for
  chapter 22's PLN formulas, whose deduction and consistency conditions are
  one body each for C and for the equations a program adds.
  `operations/lowering.c` holds the new operators to the engine's over every
  pair of a five-value grid, a zero divisor's signed infinity included, as it
  holds `C_MOD` to MeTTa's `%`. `C_MIN` and `C_MAX` are SWI's `min` and `max`
  except at NaN and at zeros of opposite sign.

- Write chapter 20's thirty-eight twins in C. The translator-rule twins keep
  the engine's cost fold and orientation rule in `costs.h`, so C decides
  which way each bidirectional rule turns a call. The MeTTa-in-MeTTa twins
  model the instruction set in C. `unify-mod` is C's matcher over cmetta's
  `mt_unify`, with `(:= x)` equality and `...` segments. `mm-switch` is C's
  walk over its cases, and the Turing machine runs on a C tape. The four
  control forms are one table in `control_forms.h`, lib_he's math is
  `math.h` row by row, and lib_strategy's typed schemes read C's
  declarations. The Prolog twins use cmetta's new doors. The foreign-rules
  space is chapter 19's C store promising rules through `mt_provider.rules`.
  The git fixture registers through `mt_register_prolog`. `mt_library`
  answers for `register_metta_library_path`, and C keeps its own model of
  the Prolog clause database. The module twins import fixtures named from
  the engine tree, and two reference twins declare that path spelling as
  their stored rows' one difference. The reader-token twin classifies
  tokens with POSIX regular expressions and reads text through `mt_parse`,
  since reading text is its subject. These twins need extensions/cmetta
  468f449.

- Give chapter 7's exponential fib and chapter 19's C store one header each.
  The same `FIB` body was written out in chapter 7's and chapter 18's twins,
  and chapter 20 needs it twice more, once as the atom a definition adds for
  itself. `fib.h`, beside `fibsmart.h`, holds its C function, its lowering
  and its atom. Chapter 19's C space kept its store inline. `c_store.h`
  holds it now, and chapter 20's foreign-rules twin opens the same store
  promising rules.

- Spell the refusal once for every judge. `verdicts.h` moves from chapter
  15 to the corpus root, because the engine reads `(refuse <words>)` in
  three places: a write hook's verdict, a typing rule's outcome and a
  translator rule's decline. Chapter 9's typing-rule twin now builds its
  refusal through `refusing()`, as chapter 15's judges do and chapter 20's
  translator rule will.

- Write chapter 18's twenty twins and chapter 19's ten in C. The workload
  loaders are C loops storing their atoms through one `mt_add_all` batch,
  sharing each subterm they build, while the questions stay the originals'
  equations, lowered from C tokens. Where the engine's own `collapse` or
  `length` does the counting, C asks for the count rather than walking a
  million answers across: `01-scale`'s questions cost 4.4 seconds as C
  cursors against the engine's 0.5, and the mate-space count 1.64 trillion
  user instructions against 1.38. Every count C expects is its own, computed
  from the loader's definition. The gap query twin reaches the language
  matcher through `mt_query` under `mt_limit`'s inference bound. The
  memoisation and tabling twins hold each report to one `tabling.h` builds
  from the counts C expects, spelled from `vocabularies.h`. `17-memo_controls`
  asks through `mt_run_goal`, because memoize-exact's tables are private to
  the engine that filled them and every `mt_eval` cursor is an engine of its
  own. Chapter 19's spaces are the engine's models built as terms through
  `spaces.h`. The parametric twin opens `(cache &primary-kb 100)` with
  `mt_space_of`. The C space is a provider behind `mt_provider_open`: a
  pthread-mutexed array answering each match with a snapshot, held to the
  seam harness's report and to four attached writer threads. The builtin
  is a function published with `mt_def`, and the native vector a C struct
  wrapped by `mt_object`. The MORK twin builds lib_mm2's operators as terms.
  The Redis and MORK twins run their guarded claims exactly when the
  originals do. Twins that import libraries mirror the originals' imports,
  since the heads rule counts what an import defines. These twins need
  extensions/cmetta 4dacbae.

- Compare a space too large to list by its equations and the hash of
  everything else. Over the lane's 50,000-atom cap, the twin lane compared
  the two `&self` spaces by one multiset hash, so a twin whose C operation
  carries an equation the original stores could pass only through a
  Divergence pin naming two hashes. `lane.c` now also prints each equation,
  `(= head body)`, as a `LANE-EQUATION` line, their count, and
  `LANE-DATA-HASH` over the atoms that are not equations. When the data
  hashes agree, the lane compares the equations under the same carried and
  derived exceptions it applies below the cap; a pin is needed only when the
  data differs. The content rule is now `twin_lane.content()`, which the
  self-test calls directly with over-cap reports, since no small original is
  over the cap. The self-test now judges 20 cases, up from 14. The new ones
  check a real report against the atoms it lists, and five over-cap reports.

- Spell each MeTTa operator once, in `lowering.h`: `C_X` is the C
  expression, `M_X` the MeTTa tokens `mt_lower()` stringifies, and `T_X` the
  atom `mt_expr()` builds, for `if`, `==`, `<`, `>`, `+`, `-`, `*`, `%` and
  `xor`. Six twins in chapters 5 and 7 each defined their own copies and now
  include it. `C_MOD` is Prolog's floored `mod`, which MeTTa's `%` is, where
  C's own `%` truncates: `(% -7 3)` is 2 and `-7 % 3` is -1.
  `operations/lowering.c` now proves `C_MOD` equal to the lowered `%` for
  every sign of dividend and divisor. The Makefile takes each program's
  header prerequisites from the compiler (`-MMD -MP`) instead of a list kept
  beside its rules, so a program rebuilds when a header it includes changes,
  and a twin including no shared header no longer rebuilds when one does.

- Name every published operation's effect class by cmetta's generated
  `enum mt_effect_class`, which replaced the hand-named `MT_PURE` through
  `MT_IO` and `mt_effect_str` in extensions/cmetta 3f4d713: fifty-nine twins,
  examples and shared headers, and the lane self-test's planted operation.
  `operations/effect_ranks.c` now publishes one copy of its function per
  member of the engine's effect-class vocabulary, counted with
  `MT_VOCABULARY_COUNT(mt_effect_class_names)` and named from each class's
  word, so a sixth class would be checked without an edit here. It also
  compares the plan's class by name, where it used to leak the symbol it
  built for the comparison.

- Write chapter 17's twelve twins in C and chapter 16's one. The lib_thread
  twins share `thread_oracle.h`: inc and big? as C functions, C's own map,
  filter, all and any as the oracle every parallel collection answers as, a
  pool's report read into C's record of a pool, and a writer on a C pthread
  attached to the engine, which await-atom and take-atom block for. A C
  thread attached to the engine also raises thread-count, and cpu-count is
  exactly `sysconf(_SC_NPROCESSORS_ONLN)`, the count SWI's flag holds.
  prime? decides by deterministic Miller-Rabin in 128-bit arithmetic, exact
  below 2^64, so every hyperpose branch is cheap in C. The class twins
  write their classes the C way: a struct marshalled to its constructor
  term, methods as C functions under the class's prefix, dispatch through a
  C function table, prototypes as spaces C names, decorators as accessors,
  composed comparators, a C generator and a closure. The grains twin runs
  each scope's body as a C function the scope evaluates. Chapter 16's
  catalog twin reads every expected word from `vocabularies.h`, the header
  cmetta now generates from the engine's rows, and the build rebuilds a twin
  when that header changes.

- Write chapter 15's six twins in C. The mutexed counter is five pthreads,
  each attached to the engine, calling a published increment that holds a
  pthread mutex, and the failing transaction is `mt_transaction` inside a
  published operation whose body answers `MT_FAIL`. The hook judges are C
  rule tables published as functions and claimed through the engine's own
  `declare-pre-add!` and `declare-post-add!`; `judges.h` keeps a model of
  what the claimed space should hold, derived from the verdicts, and builds
  each refusal's Error from C data. The admission judge is C, one function
  per head of the original's chain, with loops where the chain recurses. It
  reads every capacity row as the builtin does, and a two-row differential
  shows the builtin agreeing with it where the original's chain reads only
  the first row. Owned records are written through `mt_transaction`, and a
  refused commit is the door's `MT_ERROR`.

- Write chapter 14's two twins in C. The bounded forms take their expression
  as a term C builds, and each answer is what C computes for the expression
  itself; relational arithmetic is checked against C solving each equation
  by the inverse operation, and the clock against C's own `time()` and
  `strftime`. The pragma twin found that `mt_eval` ignored
  `max-stack-depth`: a factorial answered 120 and then overflowed the host
  stack where the original answers `(Error -3 StackOverflow)`. cmetta
  34f6aa6 runs an evaluated goal in the engine's fuel scope, and the twin
  needs that commit.

- Write chapter 12's assertion twins in C: each verdict is C's own, and each
  failure report is built from the bags C knows the forms compare, the
  missing and excess bags being the two directed `bag_minus` differences,
  which now live in `common.h`.

- Write chapter 11's Python twins in C: the same py-call terms, built as
  atoms, with each answer Python gives held against C's own value (M_PI,
  toupper, llabs, C loops for numpy's aranges and torch's elementwise
  operations, relu and sigmoid). bind! names a value for the reader, which C
  does not use, so a C variable holds each Python callable instead.

- Write chapter 10's error twins in C, where an error is a value as a C
  result is: C computes each sum and division it can and an error for the
  rest, and models if-error, return-on-error and throw, which wraps a reason
  unless it is an error already, over the atoms it builds.

- Write chapter 9's type twins in C. Declarations come from C tables and
  each claim that is a lookup is derived from them: a symbol's types, an
  arrow's result where the arguments fit, and subtyping's widening run as
  the rounds the original describes, a diamond's join appended once per
  path. The built-in operations' arrows are read from C's own prototypes
  through `_Generic`: double is Number, bool is Bool, and an atom of any type
  a type variable of its own. What is the engine's evaluation rule, such as
  a held Atom argument or a checked result, the twin states beside the
  expectation it writes.

- `metatype()` in `common.h` names an atom's kind by the metatype
  `get-metatype` answers, measured on the engine: a space is a Symbol, since
  the engine names it by a symbol, and True, False and texts are Grounded.
  The functional and CLI twins held their own copies, and the functional
  one called a space Grounded.

- Write chapter 8's CLI twin in C: an option parser written from lib_cli's
  own grammar, since getopt_long clusters `-vn` and reads `-vtrue` as five
  flags where the library refuses one and attaches the other. Declarations
  are read and refused as the library refuses them, tokens are scanned in
  its order, every value is converted before a repeat policy selects, and
  defaults precede supplied occurrences; tokens are counted bytes, so an
  argument holding NUL stays whole. Each held converter has a C counterpart,
  and `cli-plus` is one C operation the engine and C's parser both call.
  Help is laid out in optparse's columns, padded by characters, and the
  engine's help text equals C's.

- Write chapter 8's testing twin in C: range is a half-open interval
  counted in GMP, so a bound past int64 walks the same loop;
  cartesian-power is an odometer whose last place turns fastest; forall is an
  all-of loop whose check holds when True is among its answers, foldall a
  count and once the first witness. A bag assertion is the two counted
  differences subtraction-atom makes, and a caught failure is held against
  the missing and excess bags C computes. Each forall form of the original
  is a C loop over the engine's generator, proving its check on every value.

- Write chapter 8's database twin in C against SQLite: a store is a
  directory holding one SQLite file under SQLite's exclusive locking mode, so
  a second owner is refused for the store's lifetime; values keep insertion
  order as their MeTTa source, removal takes the first alpha-identical row, a
  file that is no store is refused whole with its bytes kept, and a sync
  policy with no name is refused before anything is created. The original's
  segment lets and unify run on a C matcher that follows the engine's rules:
  two leaves match when identical or, both numbers, by `=:=`, exactly, or as
  the nearest doubles when a float is involved, as SWI rounds them; a
  pattern's `(:= X)` becomes a `==/2` guard settled after the match; a
  search backtracks by cutting its bindings back to a mark. Stored equations
  are summed exactly in GMP, and `exact_oracle.h` now also gives a number's
  nearest double.

- The vector twin counts a BigRational as a number: its own number test
  predated the BigRational kind and refused one the library accepts. One
  test now lives in `exact_oracle.h`, and the statistics twin uses it too.

- Write chapter 8's compression twin in C against zlib and libarchive: C
  inflates what the engine compressed and runs the same file program in a
  directory of its own, staging and publishing by `rename(2)`; archives are
  read seekably with a gzip layer decoded first, names follow PKWARE's
  APPNOTE through C's own central-directory reader and iconv's CP437, and
  extraction refuses the library's unportable names before publishing.
  libarchive's headers are staged like cJSON's.

- Write chapter 8's socket twin in C against POSIX sockets: C runs the same
  program on its own listener, client, accepted connection and IPv6 datagram
  socket, and each engine answer is held against what C's sockets say through
  `getsockopt`, `getsockname`, `getpeername`, `poll`, `read`, `write`,
  `sendto` and `recvfrom`; refusals are the kernel's answer to the same
  operation or the precondition the library states.

- Write chapter 8's URI twin in C: uriparser, C's RFC 3986 library, for the
  strict grammar and section 5.2 resolution; RFC 3986 appendix B's regular
  expression for components, absent told from empty; section 6.2.2
  normalization in C, dots removed only from anchored paths and a leading
  double slash without an authority spelled `/.//`, where uriparser's
  normalizer departs from the library; SWI's per-context character classes
  for encoding, and strict UTF-8 through utf8proc for decoding. uriparser's
  headers are staged like cJSON's.

- Write chapter 8's HTTP twin in C against libcurl: every request the
  engine's client makes to the engine's server, C makes too, and the status,
  fields and bytes are held against what libcurl read, fields matched
  case-insensitively and repeated ones in order. One C description of a
  request builds the engine's options, drives libcurl and decides each
  refusal by RFC 9110's rules; the lifecycle refusals run inside
  `mt_transaction`. libcurl's headers are staged like cJSON's.

- Write chapter 8's statistics twin in C over GMP rationals: every moment
  exact until one rounding, and only where a float was observed; quantiles at
  CPython's positions, the geometric mean summing binary exponents apart from
  mantissa logs, ranks averaging ties, modes grouped by identity, and the
  variance's own equation read back out of the space and applied. The wide
  ratio 1/2^2000 is held in C as a BigRational, which cmetta 23bce3e added.

- Write chapter 8's random twin in C, treating randomness as an input: for
  a seed C takes the engine's own uniform stream and runs the library's
  gamma, beta and Weibull recipes over it in C, exact where the library is
  exact; properties of any draw C checks on the engine's draw, and a seed's
  determinism by comparing two runs.

- Write chapter 8's math twin in C: GMP for counts, gcd and lcm, exact
  ratios, SWI's rationalizing rule, integer roots and modular powers; libm
  by name for the real functions; `fpclassify` for classes. Rounding an
  exact value to a double once, and its square root, now live in
  `exact_oracle.h`, shared with the vector twin.

- Write chapter 8's logging twin in C: RFC 5424's severities as `syslog(3)`
  names them, a C topic table with exact names, the line format, and the
  delivery rule that checks a level always and applies a handler only while
  its topic is on.

- Write chapter 8's UUID twin in C against libuuid: random and time-based
  generation, the name-based generators over its namespace templates,
  parsing and lower-case text, and the version, variant and time fields.

- Write chapter 8's process twin in C: C runs the same programs itself with
  `posix_spawnp`, pipes read by `poll(2)` and `waitpid(2)`, watches its own
  started processes with `WNOHANG` and `kill(2)`, and never reaps or signals
  the engine's children, which share its process.

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

- LICENSE is the Apache License 2.0, and NOTICE keeps the MIT copyright
  and permission notice of PeTTa, whose programs the twins translate and
  one fixture copies.

## Initial corpus

- Add 362 asserted C programs in the eight Python example directories: 311
  language twins and 51 handwritten host examples.
- Add explicit lifetime, allocation, callback reentry, SQLite, BLAS, POSIX,
  shared extension and installed consumer examples.
- Add discovery-based builds, an always-enabled assertion helper, per-program
  results, fixture and source drift checks, and generated coverage tables.
- Document CMeTTa repairs, host-specific coverage limits and the independently
  reproduced SWI-Prolog cleanup failure.
