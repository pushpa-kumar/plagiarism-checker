# Plagiarism Checker

A token-stream-based C++ plagiarism detection system, built as the course project for **CS293: Data
Structures and Algorithms Lab** (IIT Bombay, Autumn 2024). The project is split into three phases: an
accurate pairwise checker, an efficient multithreaded bulk checker, and an adversarial "hack the checker"
exercise.

> This repository mirrors the structure of the course's own project repository,
> [SuperSat001/CS293-Project-2024](https://github.com/SuperSat001/CS293-Project-2024), and is published
> here for archival/portfolio purposes now that the course has concluded. See
> [`problem_statement.pdf`](./problem_statement.pdf) for the full, original assignment brief and
> [`REPORT.md`](./REPORT.md) for a deeper technical write-up.

## Background

Plagiarism detection for code is harder than for prose: renaming variables, reordering independent
statements, or splicing together snippets from multiple sources can all defeat a naive text diff. This
project instead compares **streams of integer tokens** produced by parsing each C++ file with `libclang`
(via the provided `tokenizer_t` class) — so cosmetic changes like variable renaming don't fool the checker,
while the *order and structure* of the code is still what gets compared.

The assignment defines five kinds of plagiarism to detect: **global**, **direct/verbatim**, **paraphrasing**,
**self-plagiarism**, and **patchwork/mosaic** plagiarism (see `problem_statement.pdf`, §1.1).

## Repository structure

```
.
├── Dockerfile              # Pinned build/dev environment (Ubuntu 24.04 + clang/llvm-18 + libclang)
├── tokenizer.hpp            # Shared, unmodifiable tokenizer interface (libclang-based)
├── problem_statement.pdf    # Original CS293 assignment brief
│
├── phase1/                  # Phase 1: pairwise checker (accuracy-focused)
│   ├── match_submissions.hpp  # <- interface students implement (currently a stub, see Status below)
│   ├── main.cpp                # Test harness: tokenizes testcases, runs match_submissions, diffs output
│   ├── tokenizer.cpp            # Tokenizer implementation
│   ├── Makefile
│   └── testcases/{one,two,three}/   # Sample file pairs + expected 5-value output
│
├── phase2/                  # Phase 2: multithreaded bulk checker (efficiency-focused)
│   ├── plagiarism_checker.hpp  # <- class interface students extend (do not change the public API)
│   ├── plagiarism_checker.cpp  # <- implementation students write (currently a stub, see Status below)
│   ├── structures.hpp           # student_t / professor_t / submission_t definitions
│   ├── main.cpp                  # Test harness: replays timestamped submissions, checks flag output
│   ├── tokenizer.cpp
│   ├── Makefile
│   └── hogwarts/, ainur/          # Two themed sample datasets with expected flagging output
│
└── phase3/                  # Phase 3: "hack the checker" exercise
    ├── README.md             # Provenance of the six checker implementations below
    ├── checker_zero.hpp       # TA reference solution
    └── checker_one.hpp … checker_five.hpp   # Five independent peer-team solutions
```

## How each phase works

**Phase 1 — pairwise checker.** Given two tokenized submissions, `match_submissions` must find
non-overlapping **exact** matches in the 10–20 token range (summed length reported), plus the single longest
**approximate** match of 30+ tokens (accepting ≥80% subsequence similarity, to catch paraphrasing). It
returns a flag plus 4 supporting values — see `problem_statement.pdf` §2 for the exact contract.

**Phase 2 — bulk checker.** `plagiarism_checker_t` is seeded with pre-existing submissions and exposes a
non-blocking `add_submission()` that must hand off the actual matching work to background thread(s)
(`std::thread` + `std::mutex`, timestamps taken via `chrono::time_point` *before* tokenizing). A submission
is flagged if it has an exact match ≥75 tokens, ≥10 total matches (each ≥15 tokens) against another single
submission, or ≥20 total matches spread (patchwork-style) across all prior submissions within a one-second
window. Flagging calls the provided `student_t`/`professor_t` APIs; original seed submissions are never
flagged. See `problem_statement.pdf` §3.

**Phase 3 — hack the checker.** Given six independent, complete implementations of the Phase 1 interface
(one TA reference + five peer solutions — see `phase3/README.md`), the goal is to construct C++ file pairs
that trigger **false positives** or **false negatives** against them. This phase is self-contained; it reuses
Phase 1's harness/tokenizer/testcases. See `problem_statement.pdf` §4.

## Implementation status

| Phase | File(s) | Status |
|---|---|---|
| 1 | `phase1/match_submissions.hpp` | **Stub** — returns all zeros, algorithm not yet implemented |
| 2 | `phase2/plagiarism_checker.cpp` | **Stub** — empty implementation, threading/flagging not yet implemented |
| 3 | `phase3/checker_*.hpp` | **Complete** — six full reference/peer implementations provided as hacking targets |

Everything else (tokenizer, harnesses, Makefiles, Dockerfile, testcases) is course-provided infrastructure
and is not meant to be modified.

## Build & run

The tokenizer relies on a specific `libclang`/LLVM-18 setup, so the project is meant to be built inside the
provided Docker image:

```sh
docker build . -t plagiarism_checker
docker run --rm -it -v .:/plagiarism_checker plagiarism_checker
```

Inside the container, each phase builds and runs its own test harness via `make`:

```sh
cd phase1 && make        # builds main, runs testcases one/two/three
cd phase2 && make        # builds main, runs the hogwarts and ainur datasets, diffs vs expected.txt
```

`make clean` in either directory removes build artifacts.

## Testcases

- `phase1/testcases/{one,two,three}/` — each holds `one.cpp` / `two.cpp` (a file pair) and `expected.txt`
  (the 5 expected `match_submissions` output values).
- `phase2/hogwarts/` and `phase2/ainur/` — two themed datasets, each with `students.txt`, `professors.txt`,
  `originals.txt` (seed submissions), a timestamped `submissions.txt` feed, and `expected.txt` (expected
  flagging transcript).

## Attribution & academic integrity

This is coursework: the problem statement, tokenizer, harnesses, testcases, and the Phase 3 reference
checkers were provided by the CS293 course staff (with the Phase 3 checkers additionally including code
authored by peer student teams, per `phase3/README.md`). It is published here after the course's
submission deadlines have passed, for archival and portfolio purposes.
