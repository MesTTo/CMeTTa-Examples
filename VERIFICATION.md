<!-- Purpose: how the corpus was last verified: the trees it ran against, the
commands, what each reported, and what the result does not cover.
Open Obligations: the open issues in ERRORS.md. -->

# Verification

The corpus at `5339297` was verified from a clean copy of its tree: a worktree
of this repository checked out at the commit `git stash create` wrote of that
tree, `59d897c`, with every untracked and ignored file removed, so every
program was built from nothing. `5339297` adds only the evidence stamps the
run supplied to four headers; later commits change no program's behaviour:
`9f10acb` rewords one comment in `common.c`, and the rest touch ERRORS.md,
CHANGELOG.md and this record with its receipts. It ran against the MeTTa
checkout's committed tree, superproject `99bd67a73`, which carries the chapter
22 fixes of `7f373da2b` and `3488b9753` in its library at lib `fa808bc` and
its originals at examples `b74a530`, pins `extensions/cmetta` at `8211c57`,
and runs on the patched SWI-Prolog 10.1.14 at `/home/user/Dev/swipl-patched`,
now `swipl-patched.5`, compiled Sep 24 2026 at 16:08:19; the battery carried
no uncommitted edit. GCC 15.2.0, CMake 4.2.3 and Python 3.14.4 built and ran
it.

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
| C seat gate | seven lanes ok: `c-binding` (166 public declarations, all defined; 1243 checks, 0 failures), `stranger-c`, `c-sanitize`, `c-bench`, `c-install`, `llms`, and `evidence` (0 unbacked evidence tags in 9724 claims) |
| `make surface` | the C seat's library built from the engine checkout |
| `make all` | 379 compile and link commands under `-std=c11 -Wall -Wextra -Wpedantic -Werror`, every one of the 374 programs among them |
| embedding programs | 51/51 passed |
| twin lane | 323/323 twins agree with their originals; 3891/3891 claims proved; stored content carried 50, declared 2, equal 271 |
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

Seven runs led here. The first, at `f7fad19`, failed two steps: `make
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
twins with 3887/3887 claims. This seventh, on `99bd67a73`, where the NARS and
PLN derivation-control originals claim `(LimitSize () 0)` and the size-0
derivation and the tile puzzle claims 181440, passed every step, 323/323 twins
with 3891/3891 claims, the four new claims among them.

`c-bench` declined its three boot comparisons in this configuration, since
the battery's checkout path is longer than the canonical shape its baseline
was measured at; every runtime row was compared.
