<!-- Purpose: record commands, fixed snapshots and observable verification results.
Open Obligations: SWI shutdown and source-writer limits in ERRORS.md. -->

# Verification

All 362 C programs were compiled with C11, `-Wall -Wextra -Wpedantic -Werror`
and executed in an isolated Git worktree. The final receipt is
[verification/results.json](verification/results.json): every discovered program
has exit status zero and an `OK` assertion report. No executable was skipped.
The two deliberate assertion-helper failures were also checked under `NDEBUG`.

The environment used GCC 15.2.0, SWI-Prolog 10.1.14, OpenBLAS 0.3.32,
CMake 4.2.3 and Python 3.11.15 for the runner. Python 3.13 was used only to
read the Python source roster, which includes PEP 695 syntax.

## Corpus and consumers

The verification tree lives at `ai-battery-1/`. Its C dependency was also built
in an isolated component worktree. With `surface` set to the repaired component
and `engine` to its containing MeTTa checkout, the executed build was:

```sh
make -C ai-battery-1 -j8 check check-consumers \
  CMETTA_DIR="$surface/ai-battery-1" \
  CMETTA_ENGINE="$engine" \
  SQLITE_CFLAGS="-I$PWD/ai-tmp/deps/sqlite/usr/include" \
  SQLITE_LIBS=-l:libsqlite3.so.0 JOBS=4
```

The SQLite overrides pair the installed runtime with the header from Ubuntu's
`libsqlite3-dev_3.46.1-9ubuntu0.3_amd64.deb`, downloaded and extracted under
`ai-tmp/deps/`. They avoid modifying the system. With SQLite development files
installed normally, the Makefile obtains both flags through pkg-config.

Results:

- All example binaries compiled and ran, including the 1,572,862-answer stream.
- Source/fixture regeneration and the index/coverage drift checks passed.
- Shared Make consumer: `OK installed_consumer (4 checks)`.
- Archive Make consumer: `OK installed_consumer (4 checks)`; `readelf` confirmed
  no `libcmetta.so` dependency.
- CMake consumer: `1/1` CTest passed with `METTA_PATH` unset.
- The loaded extension was compiled as a shared object and invoked by the
  `shared_extension` example. SQLite and BLAS support paths ran in their examples.

The `Makefile` enumerates C files in the eight corpus directories. Support C
files and the assertion-helper executable are built and exercised by those
targets; installed linking variants reuse the same asserted consumer source.

## Component regression checks

Component functional evidence is pinned to
`91eef0753a3d55913cee42a2d385bbbf008f0be5`; the following comment-only commit
`4d479e762fdc38ec5dc2ed0dc6bff59c115f9f28` records that evidence in headers.
The actual `extensions/cmetta/check.sh` file was sourced through a local harness
that selected its `c-binding` registration and invoked its `test.sh` directly.
`ENGINE_PATH` pointed at the permitted shared engine checkout, and temporary
files remained inside this repository. The enclosing superproject gate was
not run because it also owns unrelated components and writes outside this scope.

Each C repair passed that binding lane. The final component snapshot also ran:

```sh
ENGINE_PATH="$engine" sh "$surface/ai-battery-1/build.sh"
make -C "$surface/ai-battery-1" install-check ENGINE_PATH="$engine"
```

The binding lane reported 156 public declarations, all defined; 543 core checks,
zero failures; passing ownership, native parity, matcher, provider, transaction,
subscription, thread, iterator, hash, extension and cursor suites; and successful
`hello`, `ops`, `stream`, `lower` and `language` examples. Its deliberate
`assertEqual 1 2` diagnostics test the exception barrier and are expected.
Installed shared and archive consumers both booted the installed engine.
The full component output is in [verification/component.txt](verification/component.txt).

## Limits of the result

The independent SWI-only Linda probe aborted once in thirty full-cleanup runs;
its error and attribution boundary are in [ERRORS.md](ERRORS.md). Successful
corpus runs establish that every program was executed successfully, not that
this external intermittent failure has been repaired.

The correspondence inventory is explicit about missing host services and
package-specific behavior. No claim of Python package/API identity is made.
The duplicate-code audit reported one six-line callback fragment, 0.2% of
handwritten code at the first audit, 0.1% after the final additions; the reason
for retaining it is in the error ledger.
