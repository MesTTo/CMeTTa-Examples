<!-- Purpose: build, run and navigate the C examples of MeTTa. -->

# CMeTTa by example

Two kinds of program live here. The **language twins** under
[`language-feature-examples/`](language-feature-examples) are MeTTa examples
from [the MeTTa corpus](https://github.com/MesTTo/MeTTa-Examples) written again
in C on [CMeTTa](https://github.com/MesTTo/CMeTTa)'s surface, each at its
original's path and each proving every claim its original makes with no MeTTa
source text. The **embedding examples** in `basics/`, `data/`, `gallery/`,
`integration/`, `live/`, `operations/` and `reasoning/` are C programs about
embedding the engine: ownership, callbacks, threads, SQLite, BLAS.

The twins are being written by hand, chapter by chapter; the table below counts
what exists, and [COVERAGE.md](COVERAGE.md) lists the rest.

<!-- coverage:begin -->
| chapter | originals | Python twins | C twins | untwinned | declined claims |
|---|---:|---:|---:|---:|---:|
| `ch01-getting-started` | 2 | 0 | 0 | 0 | 0 |
| `ch02-programming-a-family-tree` | 4 | 0 | 0 | 0 | 0 |
| `ch03-atoms-and-expressions` | 6 | 6 | 6 | 0 | 0 |
| `ch04-spaces-and-matching` | 21 | 19 | 0 | 0 | 0 |
| `ch05-equations-and-evaluation` | 26 | 26 | 1 | 0 | 0 |
| `ch06-many-answers` | 10 | 10 | 0 | 0 | 0 |
| `ch07-control-flow` | 44 | 42 | 0 | 0 | 0 |
| `ch08-data` | 70 | 70 | 0 | 0 | 0 |
| `ch09-types` | 24 | 21 | 0 | 0 | 0 |
| `ch10-errors-and-refusals` | 2 | 2 | 0 | 0 | 0 |
| `ch11-python-as-a-notation` | 10 | 7 | 0 | 0 | 0 |
| `ch12-testing` | 4 | 4 | 0 | 0 | 0 |
| `ch14-seeing-your-program` | 3 | 2 | 0 | 0 | 0 |
| `ch15-writing-transactions-and-worlds` | 7 | 6 | 0 | 0 | 0 |
| `ch16-events-and-standing-queries` | 1 | 1 | 0 | 0 | 0 |
| `ch17-concurrency-and-the-loop` | 13 | 12 | 0 | 0 | 0 |
| `ch18-performance` | 24 | 20 | 0 | 0 | 0 |
| `ch19-spaces-backed-by-anything` | 10 | 10 | 0 | 0 | 0 |
| `ch20-extending-the-engine` | 52 | 38 | 0 | 0 | 0 |
| `ch22-a-reasoner-you-can-serve` | 33 | 27 | 0 | 0 | 0 |
| **all** | 366 | 323 | 7 | 0 | 0 |
<!-- coverage:end -->

## Running them

You need a C11 compiler, Make, Python 3, pkg-config, SQLite's development
files, OpenBLAS, and the patched SWI-Prolog 10 the engine runs on, first on
your `PATH`; the engine refuses a stock SWI-Prolog when it boots, and
[docs/patched-host.md](https://github.com/MesTTo/MeTTa/blob/main/docs/patched-host.md)
shows how to build and declare the patched one. Clone MeTTa beside this
repository: its `examples/` are the originals the twins are held against, and
its `extensions/cmetta` is the C surface.

```sh
git clone --recurse-submodules https://github.com/MesTTo/MeTTa.git ../MeTTa
make surface
make -j8 all
make check JOBS=4
make check-consumers
```

For a MeTTa checkout elsewhere, name it and its C component:

```sh
make check CMETTA_ENGINE=$HOME/src/MeTTa CMETTA_DIR=$HOME/src/MeTTa/extensions/cmetta
```

`make check` runs the embedding examples, then the twin lane,
[`tools/twin_lane.py`](tools/twin_lane.py), which runs every original through
the C seat and every twin in its own process and compares what they answer:
the claims each proves, the definitions each makes visible, and the atoms each
leaves in `&self`. `make twins TWIN=language-feature-examples/ch03-atoms-and-expressions`
runs the lane over part of the corpus, and `tools/twin_lane_selftest.py`
proves the lane refuses a twin that drifts.

`common.h` holds the checks every program states its claims through:
`check`, `check_int`, `check_text`, `check_atom` and `check_answers` are
claims, and `require` is a status the program needs before it can go on.
A false claim exits nonzero whatever `NDEBUG` says.
