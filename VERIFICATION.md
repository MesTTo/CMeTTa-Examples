<!-- Purpose: how the corpus was last verified: the trees it ran against, the
commands, what each reported, and what the result does not cover.
Open Obligations: the open issues in ERRORS.md. -->

# Verification

The corpus at `b094c80`, the head of branch `idiomatic-twins-no-text`, was
verified from a clean copy of its tree: a worktree of this repository checked
out at that commit with every untracked and ignored file removed, so every
program and every translation unit was built from nothing. The branch stacks
two commits on `65da89f`, which follows 31-system_lib's family claim: `a821e6f`
builds every twin's equations as atoms, where 72 twins handed `mt_do` MeTTa
text through `mt_lower`, and `b094c80` makes the twin lane read each
program's translation unit for doors that read MeTTa source; the commit
recording this run changes only this file, its receipts and CHANGELOG.md. It
ran against the MeTTa checkout's committed tree, superproject `3a6b92b40`,
whose library is lib `49315f8` and whose originals are examples `a35f577`,
pinning `extensions/cmetta` at `bd75ca8`, with one engine change applied on
top, `8cc5659d0` (`partial/3` to `partial/11` in `engine/metta/control.pl`,
the effect walk's reading of a partial closure, and the plunit suite
`closure_values.plt`, tracked in that tree's index as it is once the change
lands). Without that change 05-lambda's claim "maplist applies a lambda
around a C function value" fails on this superproject, on this branch and on
`main` alike, with `apply:maplist/3: Unknown procedure: partial/4`: since
`64169238b` a lambda holding a C function value is a partial value, which
Prolog's `maplist/3` called as `partial/4`. It ran on the host and toolchain
below, which `tools/verification.py` reads from the run's own `swipl`, its
declaration of the host patches it was built with, and the compilers' own
version lines:

<!-- host:begin -->
- SWI-Prolog 10.1.14, the build of Sep 25 2026, 12:27:40, declaring 37 host patches
- cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
- cmake version 4.2.3
- Python 3.14.4
<!-- host:end -->

## Commands

In the MeTTa checkout, the C seat's own gate lanes:

```sh
sh tools/check.sh c-binding stranger-c c-sanitize c-bench c-install llms evidence
```

Then in the clean copy, with `CMETTA_DIR` and `CMETTA_ENGINE` naming that
checkout's C seat and engine:

```sh
make surface
make -j8 all
make check JOBS=4
make check-consumers
```

`make all` also writes each program's translation unit, `build/<path>.i`,
which the lane's source rule reads. `make check` builds and runs
`check-helpers`, runs the 51 embedding programs through `tools/run.py`, then
the twin lane, the lane's self-test, `tools/index.py --check` and
`tools/verification.py --check`, which refuses a tracked line naming a path in
a home directory. Then the record's run-determined parts are written from the
run itself:

```sh
make verification GATE=<the C seat gate's output>
```

which copies the lane's receipts out of `build/`, writes the gate's output as
`verification/component.txt` with the engine's and this corpus's paths
repository-relative, and fills the host block above.

## Results

| Step | Result |
| --- | --- |
| C seat gate | six lanes ok: `c-binding` (166 public declarations, all defined; 1249 checks, 0 failures), `stranger-c`, `c-sanitize`, `c-install`, `llms`, and `evidence` (0 unbacked evidence tags in 9939 claims); `c-bench` fails one runtime row, `error-ball`, whose fewest instructions of three runs, 171,119,035, exceed its baseline's 170,412,819 plus 0.3%, and it fails the same on `3a6b92b40` without the engine change (171,156,432 at 2026-09-25T19:38:08+10:00), with its 67,413 inferences unmoved either way |
| `make surface` | the C seat's library built from the engine checkout |
| `make all` | 758 commands under `-std=c11 -Wall -Wextra -Wpedantic -Werror`: 379 compile and link commands, every one of the 374 programs among them, and 379 translation units |
| embedding programs | 51/51 passed |
| twin lane | 323/323 twins agree with their originals; 3891/3891 claims proved; stored content carried 50, declared 2, equal 271; no finding of the source rule |
| lane self-test | 26/26 planted cases judged as expected |
| index | INDEX.md, COVERAGE.md and README.md agree with the files |
| `make check-consumers` | both Make consumers print `OK` (2 claims each), the archive links no `libcmetta.so`, CTest passes 1/1 with `METTA_PATH` unset, and the installed prefix holds no `.git*` entry |

Every program ran under `done()`'s ownership check, so none left a block
cmetta allocated for it once its engine had closed.

Beside the gate, three checks over the repository at that commit:

- The lane's source rule, both halves, over all 409 tracked C files and
  the 379 translation units `make all` wrote: 51 uses of MeTTa text, every
  one in a file whose `text:` note says why it handles text: the eight
  twins README.md lists, `basics/source_forms.c`, `data/counted_text.c`
  and `support/sqlite_store.c`. No program reaches `mt_do`, and none
  applies a reader head outside those files.
- `git grep` over the C sources for `check_program` and for
  `const char *const program[]` finds nothing. The two `program[]` arrays in
  chapter 22's matespace twins hold atoms built with `E`, not text.
- Neither `tools/generate.py` nor `corpus.json` is tracked, and `git grep -i`
  over the C sources, the Makefile and `tools/` for `generate.py`,
  `corpus.json` and `auto-generated` finds nothing.

The receipts are [verification/results.json](verification/results.json),
each embedding program's exit and log;
[verification/twins.json](verification/twins.json), the lane's verdict on
each twin; and [verification/component.txt](verification/component.txt), the
C seat gate's whole output.

## Limits of the result

Nine runs led here. The first, at `f7fad19`, failed two steps: `make
check-consumers`, because `lane.c` called POSIX `strdup` where the consumers
compile plain C11, fixed in `2c09a57`; and chapter 20's `13-reference_loading`
twin, which died with SIGSEGV while closing its engine when SWI's halt raced
the loader thread its `background` policy had started. That race was the
host's: the host went live as `swipl-patched.5` in superproject `622e425d4`,
carrying the fix, and cmetta's `make runtime-halt-created-thread` halts
cleanly 20 runs of 20 on it in this run's battery. The second run, at
`2c09a57`, passed every step, and then `git clean -fdx` could not empty its
build directory, because cmetta's `make install` had copied the lib
submodule's `.git` into the consumers' prefix; the seat's `f76b44e` keeps
version-control metadata out of the install. The third, at `4fe7740` against
superproject `8bda9d552`, passed every step before the verdict rename landed.
The fourth, the first against the rename, on superproject `bc058ca6e` with the
seat's link fix carried uncommitted, held the five twins that answer a
verdict, chapter 9's `16-typing_rules`, chapter 15's `03-pre_add_hooks`,
`04-admission_pools` and `05-post_add_hooks`, and chapter 20's
`07-translatorrule_refusal`, to their renamed originals through `verdicts.h`'s
capitalized constructors, and lost `13-reference_loading` to the same race as
the first run, after the twin had printed its space report (core 1509400,
`PL_cleanup` in `cleanupFunctors` this time); that twin's lane then agreed ten
times of ten run alone on the same engine, 5/5 claims each. The fifth, on
superproject `f9c56dbba` as pinned, passed every step. The sixth, on
`3f3d8107a`, where `47855fa71` made the admission original's MeTTa chain walk
every capacity row and gave it a two-row section, which chapter 15's
`04-admission_pools` twin follows since `f8d79ba`, passed every step, 323/323
twins with 3887/3887 claims. The seventh, on `99bd67a73`, where the NARS and
PLN derivation-control originals claim `(LimitSize () 0)` and the size-0
derivation and the tile puzzle claims 181440, passed every step, 323/323 twins
with 3891/3891 claims, the four new claims among them. The eighth, on
`c601721e3` for the branch that builds every equation as an atom, passed
every step with the same counts, and the source rule found no MeTTa text
outside the files that declare it. The same rule laid over `64a5bd1`, on
superproject `2ffb3fb39`, fails the 72 twins that lowered equations through
`mt_lower` and `operations/lowering.c`, while their claims and stored
content still agree, so the text alone fails them. Once the branch was
stacked on `65da89f`, the run on `4a357180e` passed every step but one twin:
05-lambda failed the partial-value defect described above, which `main`'s
twin fails too on `64169238b` and later. This ninth, on `3a6b92b40` with the
engine change that makes a partial value callable, passes every step of the
corpus with the eighth's counts.

`c-bench` declined its three boot comparisons in this configuration, since
the battery's checkout path is longer than the canonical shape its baseline
was measured at, and its one failing runtime row, `error-ball`, is the
superproject's and not this corpus's: the table above gives the reading
without the engine change.
