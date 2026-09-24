<!-- Purpose: how the corpus was last verified: the trees it ran against, the
commands, what each reported, and what the result does not cover.
Open Obligations: the open issues in ERRORS.md. -->

# Verification

The corpus at `f8d79ba` was verified from a clean copy: a worktree of this
repository checked out at that commit with every untracked and ignored file
removed, so every program was built from nothing. Later commits change no
program: `818dc34` pins the admission twin's evidence tag to this commit, and
the rest touch ERRORS.md, CHANGELOG.md and this record with its receipts. It
ran against the MeTTa checkout's committed tree, superproject `3f3d8107a`,
which carries the verdict rename of `5bae989df` and the admission fix of
`47855fa71` in its originals at examples `3372c22`, and pins
`extensions/cmetta` at `8211c57`, in a battery of that checkout carrying no
uncommitted edit. The host was the patched SWI-Prolog 10.1.14 at
`/home/user/Dev/swipl-patched`, compiled Sep 24 2026 at 09:57:51, with GCC
15.2.0, CMake 4.2.3 and Python 3.14.4.

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

`make check` builds and runs `check-helpers`, runs the 51 embedding programs
through `tools/run.py`, then the twin lane, the lane's self-test and
`tools/index.py --check`.

## Results

| Step | Result |
| --- | --- |
| C seat gate | seven lanes ok: `c-binding` (166 public declarations, all defined; 1243 checks, 0 failures), `stranger-c`, `c-sanitize`, `c-bench`, `c-install`, `llms`, and `evidence` (0 unbacked evidence tags in 9703 claims) |
| `make surface` | the C seat's library built from the engine checkout |
| `make all` | 379 compile and link commands under `-std=c11 -Wall -Wextra -Wpedantic -Werror`, every one of the 374 programs among them |
| embedding programs | 51/51 passed |
| twin lane | 323/323 twins agree with their originals; 3887/3887 claims proved; stored content carried 50, declared 2, equal 271 |
| lane self-test | 22/22 planted cases judged as expected |
| index | INDEX.md, COVERAGE.md and README.md agree with the files |
| `make check-consumers` | both Make consumers print `OK` (2 claims each), the archive links no `libcmetta.so`, CTest passes 1/1 with `METTA_PATH` unset, and the installed prefix holds no `.git*` entry |

Every program ran under `done()`'s ownership check, so none left a block
cmetta allocated for it once its engine had closed.

Beside the gate, three checks over the repository at that commit:

- The lane's source rule, `scan()` in `tools/twin_lane.py`, over all
  409 tracked C files: 39 findings. Each is in a file whose
  `text:` note says why it handles MeTTa text, except `tools/original.c`,
  which loads each original and so reads text by definition, and
  `common.c`'s `"(null)"`, printf's spelling of a null string.
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

Six runs led here. The first, at `f7fad19`, failed two steps: `make
check-consumers`, because `lane.c` called POSIX `strdup` where the consumers
compile plain C11, fixed in `2c09a57`; and chapter 20's `13-reference_loading`
twin, which died with SIGSEGV while closing its engine when SWI's halt raced
the loader thread its `background` policy had started. That race is the host
defect ERRORS.md keeps open: any program whose engine creates a thread just
before it closes can fail that way under the parallel lane until the live host
carries the fix, which is in the native build `swipl-patched.5`, due after
gate-perf lands. The second run, at `2c09a57`, passed every step, and then
`git clean -fdx` could not empty its build directory, because cmetta's `make
install` had copied the lib submodule's `.git` into the consumers' prefix; the
seat's `f76b44e` keeps version-control metadata out of the install. The third,
at `4fe7740` against superproject `8bda9d552`, passed every step before the
verdict rename landed. The fourth, the first against the rename, on
superproject `bc058ca6e` with the seat's link fix carried uncommitted, held
the five twins that answer a verdict, chapter 9's `16-typing_rules`, chapter
15's `03-pre_add_hooks`, `04-admission_pools` and `05-post_add_hooks`, and
chapter 20's `07-translatorrule_refusal`, to their renamed originals through
`verdicts.h`'s capitalized constructors, and lost `13-reference_loading` to
the same race as the first run, after the twin had printed its space report
(core 1509400, `PL_cleanup` in `cleanupFunctors` this time); that twin's lane
then agreed ten times of ten run alone on the same engine, 5/5 claims each.
The fifth, on superproject `f9c56dbba` as pinned, passed every step. This
sixth, on `3f3d8107a`, where `47855fa71` made the admission original's MeTTa
chain walk every capacity row and gave it a two-row section, which chapter
15's `04-admission_pools` twin follows since `f8d79ba`, passed every step,
323/323 twins with 3887/3887 claims, the original's two new claims among them.

`c-bench` declined its three boot comparisons in this configuration, since
the battery's checkout path is longer than the canonical shape its baseline
was measured at; every runtime row was compared.
