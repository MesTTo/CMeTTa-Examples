<!-- Purpose: how the corpus was last verified: the trees it ran against, the
commands, what each reported, and what the result does not cover.
Open Obligations: the open issues in ERRORS.md. -->

# Verification

The corpus at `4fe7740` was verified from a clean copy: a worktree of this
repository checked out at that commit with every untracked and ignored file
removed, so every program was built from nothing. Later commits change no
program: they touch ERRORS.md, CHANGELOG.md and this record with its receipts,
and `f9418e6` replaces each evidence tag's `WORKTREE` placeholder with the
commit that supplied it, in comments and in INDEX.md's copy of one. It ran
against the MeTTa checkout's committed tree, superproject `8bda9d552`, in a
battery of that checkout carrying no other uncommitted edit, with
`extensions/cmetta` at `e73dfea`, the seat's install fix, which the
superproject pins in place of `cf925da` once gate-perf has landed. The host
was the patched SWI-Prolog 10.1.14 at `/home/user/Dev/swipl-patched`, compiled
Sep 24 2026 at 09:57:51, with GCC 15.2.0, CMake 4.2.3 and Python 3.14.4.

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
| C seat gate | seven lanes ok: `c-binding` (166 public declarations, all defined; 1243 checks, 0 failures), `stranger-c`, `c-sanitize`, `c-bench`, `c-install`, `llms`, and `evidence` (0 unbacked evidence tags in 9674 claims) |
| `make surface` | the C seat's library built from the engine checkout |
| `make all` | 379 compile and link commands under `-std=c11 -Wall -Wextra -Wpedantic -Werror`, every one of the 374 programs among them |
| embedding programs | 51/51 passed |
| twin lane | 323/323 twins agree with their originals; 3885/3885 claims proved; stored content carried 50, declared 2, equal 271 |
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
- `git grep` for `check_program` and for `const char *const program[]`
  finds nothing. The two `program[]` arrays in chapter 22's matespace twins
  hold atoms built with `E`, not text.
- `git grep -i` for `generate.py`, `corpus.json` and `auto-generated` finds
  only history: one line of CHANGELOG.md and four of ERRORS.md.

The receipts are [verification/results.json](verification/results.json),
each embedding program's exit and log;
[verification/twins.json](verification/twins.json), the lane's verdict on
each twin; and [verification/component.txt](verification/component.txt), the
C seat gate's whole output.

## Limits of the result

This is the third run of the verification. The first, at `f7fad19`, failed two
steps. `make check-consumers` failed because `lane.c` called POSIX `strdup`
where the consumers compile plain C11, fixed in `2c09a57`. And one twin,
chapter 20's `13-reference_loading`, died with SIGSEGV while closing its
engine: SWI's halt raced the loader thread its `background` policy had
started, the host defect ERRORS.md keeps open. Any program whose engine
creates a thread just before it closes can fail that way under the parallel
lane until the live host carries the fix, which is in the native build
`swipl-patched.5`, due after gate-perf lands. The second run, at `2c09a57`,
passed every step, and then `git clean -fdx` could not empty its build
directory: cmetta's `make install` had copied the lib submodule's `.git` into
the consumers' prefix, making it a nested repository. The seat's `f76b44e`
keeps version-control metadata out of the install, and this run is against it,
`extensions/cmetta` at its provenance pin `e73dfea` ahead of the
superproject's pin.

The shared MeTTa checkout has an uncommitted rename of the judges' verdicts
to `(Accept)`, `(Refuse W)`, `(Drop)` and `Defer`. Against that live tree
five twins fail: chapter 9's `16-typing_rules`, chapter 15's
`03-pre_add_hooks`, `04-admission_pools` and `05-post_add_hooks`, and
chapter 20's `07-translatorrule_refusal`. They spell every verdict through
`verdicts.h` and move with it when the rename lands.

`c-bench` declined its three boot comparisons in this configuration, since
the battery's checkout path is longer than the canonical shape its baseline
was measured at; every runtime row was compared.
