"""Purpose: hold every C twin against the MeTTa original it mirrors, so a twin
cannot drift from its program without this lane going red.

A twin is an ordinary C program at its original's path, with `.c` for
`.metta`. The lane runs the original through the C seat (build/tools/original)
and the twin in its own process, and compares what the two answer:

  claims   the original states one claim per assert-family `!` form; the twin
           proves one per check_* call it survives, so it must prove at least
           as many, less the forms residue.json declines
  heads    every head the original defines by an equation is either an
           equation head in the twin's &self or a C operation it published
  content  the atoms the two &self spaces hold are one multiset, up to
           variable renaming, except where the twin's C operation carries the
           original's equations for that head, or the twin declares the exact
           difference as a Divergence
  engine   a twin reaches the engine (20 inferences or more) unless it says
           why not with an `engine-free:` note
  source   no program runs MeTTa source text: no mt_run, mt_do, mt_load,
           mt_parse, mt_parsen or mt_forms, and no string literal shaped like
           a MeTTa form, unless its subject is reading or running text and it
           says so with a `text:` note; this binds the embedding examples too
  scope    every original the Python seat twins has a C twin or an untwinned
           residue entry, and no twin mirrors an original that is not there

This is extensions/python/tools/twin_coverage.py's contract ported to C:
count against count, visible definitions, and stored content compared across
two processes. Its inference BUDGET is not ported. A C function spends no
engine inferences, so a band around the original's count would measure which
door a twin chose rather than whether it drifted; the lane reports both counts
and gates only the floor [source: extensions/python/tools/twin_coverage.py,
compare/_visible/_stored/_price; commit=7d995f762ba535440834d6edd071679f672fdca9].

Assumes: `make all` has built every twin and build/tools/original; the engine
tree holds examples/ and, for the scope rule, extensions/python/examples.
Guarantees: exits nonzero on any finding and prints each with its twin's path;
writes build/twins.json with every verdict [tested: make check;
commit=WORKTREE].
Fails when: an original's answers depend on wall-clock order across threads;
its space digest then differs run to run and the twin must declare it.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TWINS = ROOT / "language-feature-examples"
RESIDUE = TWINS / "residue.json"
BUILD = ROOT / "build"
RUNNER = BUILD / "tools" / "original"
HANDWRITTEN = ("basics", "data", "gallery", "integration", "live", "operations",
               "reasoning", "support", "checks")

#: The example heads that state a claim, the Python lane's own set
#: [source: extensions/python/tools/twin_coverage.py, ASSERT_HEADS;
#: commit=7d995f762ba535440834d6edd071679f672fdca9].
ASSERT_HEADS = frozenset({
    "test", "test-no-answer", "assert", "assertEqual", "assertAlphaEqual",
    "assertEqualToResult", "assertAlphaEqualToResult", "assertIncludes",
    "assertEqualMsg", "assertAlphaEqualMsg", "assertEqualToResultMsg",
    "assertAlphaEqualToResultMsg",
})

#: Below this a twin did nothing an engine was needed for. The Python lane's
#: floor is 100 because its cheapest querying twin cost 449; a C twin's count
#: starts after boot and excludes the lane's own report, so a program that
#: never evaluates reads 5 (the counter's own read) and one that evaluates a
#: single text atom reads 84 [measured 2026-09-24: ai-tmp/probe/idle.c three
#: runs at 5; ch03-atoms-and-expressions/02-string.c at 84]. 20 sits between.
ENGINE_FLOOR = 20

#: The doors that take MeTTa source text.
SOURCE_DOORS = ("mt_run", "mt_do", "mt_load", "mt_parse", "mt_parsen", "mt_forms",
                "mt_self_run", "mt_space_run", "mt_self_do", "mt_space_do",
                "mt_self_load", "mt_space_load")

#: A string literal shaped like MeTTa source: a parenthesised form, a bang
#: form, or a $variable.
FORM_LITERAL = re.compile(r'^\s*!?\(.*\)\s*$|\$[A-Za-z_]', re.S)

#: Calls whose string arguments are text or a label, never structure.
TEXT_CALLS = frozenset({
    "mt_text", "mt_textn", "T", "printf", "fprintf", "puts", "fputs", "snprintf",
    "check", "check_int", "check_real", "check_text", "check_atom",
    "check_answers", "check_none", "require", "strcmp", "strncmp", "strstr",
    "mt_fail", "mt_error_set", "perror",
})


# ---------------------------------------------------------------- the original
def example_forms(source: str) -> list[str]:
    """The head of every `!` form in source order, read by balancing parentheses."""
    heads, index, size = [], 0, len(source)
    while index < size:
        char = source[index]
        if char == ";":
            end = source.find("\n", index)
            index = size if end < 0 else end
            continue
        if char == '"':
            index = _past_string(source, index)
            continue
        if char == "!" and index + 1 < size and source[index + 1] == "(":
            start = index
            index = _past_form(source, index + 1)
            head = re.match(r"!\(\s*([^\s()]+)", source[start:index])
            heads.append(head.group(1) if head else "")
            continue
        index += 1
    return heads


def _past_string(source: str, index: int) -> int:
    index += 1
    while index < len(source) and source[index] != '"':
        index += 2 if source[index] == "\\" else 1
    return index + 1


def _past_form(source: str, index: int) -> int:
    depth, size = 0, len(source)
    while index < size:
        char = source[index]
        if char == ";":
            end = source.find("\n", index)
            if end < 0:
                return size
            index = end
            continue
        if char == '"':
            index = _past_string(source, index)
            continue
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    return size


# ---------------------------------------------------------------- one run
@dataclass
class Run:
    """What one side reported: its exit, claims, heads, ops and &self."""

    returncode: int
    output: str
    claims: int | None = None
    inferences: int | None = None
    heads: tuple[str, ...] = ()
    ops: tuple[str, ...] = ()
    held: int | None = None
    atoms: list[str] | None = None
    hash: str | None = None
    ok: bool = False


def read(returncode: int, output: str) -> Run:
    run = Run(returncode, output)
    atoms: list[str] = []
    for line in output.splitlines():
        marker, _, rest = line.partition(" ")
        if marker == "LANE-CLAIMS":
            run.claims = int(rest)
        elif marker == "LANE-INFERENCES":
            run.inferences = int(rest)
        elif marker == "LANE-HEADS":
            run.heads = tuple(rest.split())
        elif marker == "LANE-OPS":
            run.ops = tuple(rest.split())
        elif marker == "LANE-HELD":
            run.held = int(rest)
        elif marker == "LANE-ATOM":
            atoms.append(rest)
        elif marker == "LANE-HASH":
            run.hash = rest.strip()
        elif marker == "OK":
            run.ok = True
    if run.held is not None and run.held == len(atoms):
        run.atoms = atoms
    return run


def execute(argv: list[str], cwd: Path) -> Run:
    """Run without a deadline and keep the whole output for the verdict."""
    env = dict(os.environ, TMPDIR=str(ROOT / "ai-tmp"))
    done = subprocess.run(argv, cwd=cwd, env=env, capture_output=True,
                          text=True, errors="replace", check=False)
    return read(done.returncode, done.stdout + done.stderr)


# ---------------------------------------------------------------- the twin's source
def c_tokens(source: str):
    """Yield (kind, text) for identifiers, strings, comments and punctuation."""
    index, size = 0, len(source)
    while index < size:
        char = source[index]
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            end = size if end < 0 else end + 2
            yield "comment", source[index:end]
            index = end
        elif source.startswith("//", index):
            end = source.find("\n", index)
            end = size if end < 0 else end
            yield "comment", source[index:end]
            index = end
        elif char == '"':
            end = index + 1
            while end < size and source[end] != '"':
                end += 2 if source[end] == "\\" else 1
            yield "string", source[index + 1:end]
            index = end + 1
        elif char == "'":
            end = index + 1
            while end < size and source[end] != "'":
                end += 2 if source[end] == "\\" else 1
            index = end + 1
        elif char.isalpha() or char == "_":
            end = index
            while end < size and (source[end].isalnum() or source[end] in "_$"):
                end += 1
            yield "ident", source[index:end]
            index = end
        elif char.isspace():
            index += 1
        else:
            yield "punct", char
            index += 1


def notes(source: str) -> dict[str, str]:
    """The lane notes a twin writes in its comments: `text:`, `engine-free:`
    and `Divergence <digest>:`, each followed by its reason."""
    found: dict[str, str] = {}
    for kind, text in c_tokens(source):
        if kind != "comment":
            continue
        body = " ".join(line.strip(" */") for line in text.splitlines())
        for key in ("text", "engine-free"):
            hit = re.search(rf"\b{key}: (.+)", body)
            if hit:
                found[key] = hit.group(1).strip()
        hit = re.search(r"\bDivergence ([0-9a-f]{16}):", body)
        if hit:
            found["divergence"] = hit.group(1)
    return found


def scan(source: str, allowed_text: bool) -> list[str]:
    """Source discipline: which MeTTa-source doors and form-shaped literals
    the twin uses, with the call each literal sits in."""
    findings = []
    calls: list[str] = []          # the callee of each open parenthesis
    previous = ""
    for kind, text in c_tokens(source):
        if kind == "ident":
            previous = text
            if text in SOURCE_DOORS and not allowed_text:
                findings.append(f"runs MeTTa source through {text}; a twin "
                                "builds terms, unless its original is about "
                                "text and it says so with a `text:` note")
            continue
        if kind == "punct":
            if text == "(":
                calls.append(previous)
            elif text == ")" and calls:
                calls.pop()
            previous = ""
            continue
        if kind == "string":
            callee = calls[-1] if calls else ""
            if (FORM_LITERAL.search(text) and callee not in TEXT_CALLS
                    and callee != "mt_lower" and not allowed_text):
                findings.append(f"the string \"{text[:60]}\" in {callee or 'a '
                                'declaration'} is shaped like MeTTa source")
    return findings


# ---------------------------------------------------------------- stored content
def surplus(these: list[str], those: list[str]) -> list[str]:
    extra = Counter(these) - Counter(those)
    return sorted(atom for atom, n in extra.items() for _ in range(n))


def carried_by_op(atom: str, ops: set[str]) -> bool:
    """Whether a stored atom is an equation or declaration for a head the twin
    publishes as a C operation, which carries that definition instead."""
    hit = re.match(r"^\((?:=|:) \(?([^\s()]+)", atom)
    return bool(hit and hit.group(1) in ops)


def divergence_digest(example_only: list[str], twin_only: list[str]) -> str:
    text = json.dumps([example_only, twin_only], separators=(",", ":"))
    return hashlib.sha256(text.encode()).hexdigest()[:16]


# ---------------------------------------------------------------- the verdict
@dataclass
class Verdict:
    twin: str
    example: str
    claims_owed: int = 0
    claims_proved: int = 0
    example_inferences: int | None = None
    twin_inferences: int | None = None
    storage: str = "unknown"
    findings: list[str] = field(default_factory=list)


def residue_entries() -> list[dict]:
    if not RESIDUE.exists():
        return []
    return json.loads(RESIDUE.read_text(encoding="utf-8"))["entries"]


def check_one(twin: Path, engine: Path, entries: list[dict]) -> Verdict:
    rel = twin.relative_to(TWINS).with_suffix(".metta")
    example = engine / "examples" / rel
    verdict = Verdict(str(twin.relative_to(ROOT)), f"examples/{rel}")
    find = verdict.findings.append
    if not example.exists():
        find("mirrors no original: the corpus has no " + verdict.example)
        return verdict
    source = twin.read_text(encoding="utf-8")
    told = notes(source)
    for finding in scan(source, "text" in told):
        find(finding)

    heads = example_forms(example.read_text(encoding="utf-8"))
    declined = {e["form"] for e in entries
                if e["example"] == verdict.example and e["kind"] == "declined"}
    verdict.claims_owed = max(0, sum(h in ASSERT_HEADS for h in heads) - len(declined))

    binary = BUILD / twin.relative_to(TWINS.parent).with_suffix("")
    if not binary.exists():
        find(f"was not built: {binary} is missing; run make all")
        return verdict
    left = execute([str(RUNNER), verdict.example], engine)
    right = execute([str(binary)], ROOT)
    if left.returncode != 0:
        tail = [line for line in left.output.splitlines() if line.strip()][-2:]
        find(f"the original failed to run (exit {left.returncode}): "
             + " | ".join(tail))
        return verdict
    if right.returncode != 0 or not right.ok:
        tail = [line for line in right.output.splitlines() if line.strip()][-3:]
        find(f"the twin failed (exit {right.returncode}): " + " | ".join(tail))
        return verdict
    verdict.claims_proved = right.claims or 0
    verdict.example_inferences = left.inferences
    verdict.twin_inferences = right.inferences

    if verdict.claims_proved < verdict.claims_owed:
        find(f"the original states {verdict.claims_owed} claims and the twin "
             f"proved {verdict.claims_proved}; a claim C cannot make is a "
             "residue entry, never a silent gap")

    ops = set(right.ops)
    hidden = sorted(h for h in set(left.heads) - set(right.heads)
                    if h.split("/")[0] not in ops and "_Spec_" not in h)
    if hidden:
        find("defines " + " ".join(hidden) + " as neither an equation nor a "
             "published C operation, so a definition the original makes "
             "visible is hidden")

    if (right.inferences or 0) < ENGINE_FLOOR and "engine-free" not in told:
        find(f"spent {right.inferences} engine inferences, under the floor of "
             f"{ENGINE_FLOOR}; a twin that never reaches the engine only agrees "
             "with its original, so say why with an `engine-free:` note")

    if left.hash == right.hash:
        verdict.storage = "equal"
        if "divergence" in told:
            find("declares a Divergence while the two spaces hold the same atoms")
        return verdict
    if left.atoms is None or right.atoms is None:
        pinned = divergence_digest([left.hash or ""], [right.hash or ""])
        verdict.storage = "pinned" if told.get("divergence") == pinned else "different"
        if verdict.storage != "pinned":
            find(f"stored content differs (hash {left.hash} against {right.hash}, "
                 f"over the enumeration cap); pin it with `Divergence {pinned}:`")
        return verdict
    example_only = [a for a in surplus(left.atoms, right.atoms)
                    if not carried_by_op(a, ops)]
    twin_only = surplus(right.atoms, left.atoms)
    if not example_only and not twin_only:
        verdict.storage = "carried"
        if "divergence" in told:
            find("declares a Divergence while the only difference is the "
                 "definitions its C operations carry")
        return verdict
    digest = divergence_digest(example_only, twin_only)
    if told.get("divergence") == digest:
        verdict.storage = "declared"
        return verdict
    verdict.storage = "different"
    find(f"stored content differs: original-only={json.dumps(example_only)} "
         f"twin-only={json.dumps(twin_only)}; if the difference is meant, "
         f"declare `Divergence {digest}:` with its reason")
    return verdict


# ---------------------------------------------------------------- scope
def python_twinned(engine: Path) -> set[str]:
    tree = engine / "extensions" / "python" / "examples" / "language-feature-examples"
    return {f"examples/{p.relative_to(tree).with_suffix('.metta')}"
            for p in tree.rglob("*.py") if "_fixtures" not in p.parts
            and (engine / "examples" / p.relative_to(tree).with_suffix(".metta")).exists()}


def twins_on_disk() -> list[Path]:
    return sorted(p for p in TWINS.rglob("*.c") if "_fixtures" not in p.parts)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("only", nargs="*", help="twin paths or a prefix to narrow the run")
    args = parser.parse_args()
    engine = args.engine.resolve()
    entries = residue_entries()
    twins = twins_on_disk()
    if args.only:
        twins = [t for t in twins if any(str(t.relative_to(ROOT)).startswith(o.rstrip("/"))
                                         or str(t).startswith(o) for o in args.only)]
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        verdicts = list(pool.map(lambda t: check_one(t, engine, entries), twins))

    scope_findings = []
    if not args.only:
        # The source rule binds every program, not only the twins.
        for program in sorted(p for d in HANDWRITTEN for p in (ROOT / d).glob("*.c")):
            source = program.read_text(encoding="utf-8")
            for finding in scan(source, "text" in notes(source)):
                scope_findings.append(f"{program.relative_to(ROOT)}: {finding}")
        twinned = {v.example for v in verdicts}
        untwinned = {e["example"] for e in entries if e["kind"] == "untwinned"}
        for example in sorted(python_twinned(engine) - twinned - untwinned):
            scope_findings.append(f"{example}: the Python seat twins it and C has "
                                  "neither a twin nor an untwinned residue entry")
        for example in sorted(twinned & untwinned):
            scope_findings.append(f"{example}: twinned and also listed as untwinned")

    BUILD.mkdir(exist_ok=True)
    (BUILD / "twins.json").write_text(json.dumps(
        [v.__dict__ for v in verdicts], indent=1) + "\n", encoding="utf-8")
    failed = [v for v in verdicts if v.findings]
    for v in failed:
        for finding in v.findings:
            print(f"FINDING {v.twin}: {finding}")
    for finding in scope_findings:
        print(f"FINDING {finding}")
    storage = Counter(v.storage for v in verdicts)
    owed = sum(v.claims_owed for v in verdicts)
    proved = sum(min(v.claims_proved, v.claims_owed) for v in verdicts)
    print(f"{len(verdicts) - len(failed)}/{len(verdicts)} twins agree with their "
          f"originals; {proved}/{owed} claims proved; stored content "
          + ", ".join(f"{k} {n}" for k, n in sorted(storage.items())))
    return 1 if failed or scope_findings else 0


if __name__ == "__main__":
    sys.exit(main())
