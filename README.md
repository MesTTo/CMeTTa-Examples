<!-- Purpose: build, run and navigate the standalone C corpus.
Assumes: the repaired CMeTTa surface and its engine are available locally.
Open Obligations: the shared-runtime limitations are listed in ERRORS.md. -->

# CMeTTa examples

362 C programs open MeTTa in their own process and assert their results.
Start with [first_steps.c](basics/first_steps.c), then use the
[complete index](INDEX.md) or the [Python correspondence](COVERAGE.md).
`common.h` and `common.c` provide always-enabled `check` and `done` helpers.
A wrong result exits unsuccessfully, including builds with `NDEBUG`.

## Build and run

You need a C11 compiler, Make, Git, Python 3 for build tooling, pkg-config,
SQLite development files and OpenBLAS, and the patched SWI-Prolog 10 the MeTTa
engine runs on, first on your `PATH`. The engine refuses a stock SWI-Prolog
when it boots; [docs/patched-host.md](https://github.com/MesTTo/MeTTa/blob/main/docs/patched-host.md)
shows how to build and declare the patched one, and its install carries the
development headers. CMake is needed for the installed CMake consumer. No
Python runtime is embedded.

Clone MeTTa beside this repository. Its C component, `extensions/cmetta`,
includes every repair listed in [ERRORS.md](ERRORS.md), such as `mt_matcher`
and `mt_effect_plan`.

```sh
git clone --recurse-submodules https://github.com/MesTTo/MeTTa.git ../MeTTa
make surface
make -j8 all
make check JOBS=4
make check-consumers
```

Run these commands from this repository root. For a MeTTa checkout somewhere
else, name it and its C component:

```sh
export CMETTA_ENGINE="$HOME/src/MeTTa"
export CMETTA_DIR="$CMETTA_ENGINE/extensions/cmetta"
```

To run one example:

```sh
./build/basics/first_steps
# OK first_steps (12 checks)
```

`make list` lists every example source. `make check` discovers all eight
directories, compiles every example, checks the generated sources, runs the
negative assertion-helper tests, and executes every binary. It keeps individual
logs under `build/logs/` and all exit codes in `build/results.json`.
`JOBS` controls concurrent example processes; the default is one.
Examples use local fixtures and local sockets. Git import uses a local repository.

`make check-consumers` installs CMeTTa into `build/prefix/`, builds and runs
shared and archive consumers through [consumer/Makefile](consumer/Makefile),
then builds and runs [consumer/CMakeLists.txt](consumer/CMakeLists.txt) through
CTest. The archive consumer still dynamically links SWI-Prolog; its executable
is checked for an unintended `libcmetta.so` dependency.

## C embedding paths

| Start here | Checked result |
| --- | --- |
| [Lifetime](integration/embedding_lifetime.c), [arena](integration/arena_values.c), [borrowed storage](integration/borrowed_storage.c) | Explicit runtime ownership, allocator release and borrowed-buffer lifetime |
| [Callback reentry](integration/callback_reentry.c), [errors](integration/callback_errors.c), [iterator](integration/native_iterator.c) | C calls MeTTa during a C callback; errors remain statuses; abandoned streams close |
| [Structs](data/struct_marshalling.c), [arrays](data/array_marshalling.c), [objects](integration/object_lifetime.c) | Round trips and deterministic release across the foreign boundary |
| [SQLite space](integration/sqlite_space.c), [WAL store](gallery/journaled_observed_store.c), [migration](integration/persistent_migration.c) | Duplicate rows, rollback, policy refusal, committed events and reopened persistence |
| [Threads](integration/threads.c), [daemon](integration/daemon.c) | Attached workers, isolated error state, multiple requests, malformed input and orderly shutdown |
| [Shared extension](integration/shared_extension.c), [installed consumer](integration/installed_consumer.c) | A loaded C callback returns 42; shared, archive and CMake consumers boot independently |
| [Regex matcher](reasoning/regex_matching.c), [tensor callback](gallery/symbolic_tensors.c) | POSIX matching and one CBLAS GEMM after symbolic simplification |

SQLite gives a C process durable storage with explicit statements and
transactions. POSIX supplies threads, sockets and regular expressions. CBLAS
supplies a numerical kernel behind symbolic terms. These integrations sit
beside the language twins; [COVERAGE.md](COVERAGE.md) names host-specific behavior
that they do not reproduce.

Edit `corpus.json` to change a language twin or fixture, then run
`python3 tools/generate.py` and `python3 tools/index.py`. Handwritten examples
remain ordinary C files with short checked sections and ownership comments.

[VERIFICATION.md](VERIFICATION.md) records the executed commands and outcomes.
[ERRORS.md](ERRORS.md) records the repaired C defects, corrected example mistakes,
and unresolved reader and SWI shutdown failures.
