# Technical Report: Plagiarism Checker (CS293 DSA Lab Project)

**Course:** CS293 — Data Structures and Algorithms Lab, IIT Bombay, Autumn 2024
**Source document:** `problem_statement.pdf`

## 1. Problem statement

The task is to detect plagiarism between C++ code submissions without relying on citations or code
semantics — purely by comparing the **order and structure of tokens** each file parses into. The brief
identifies five forms of plagiarism the system should be robust to:

1. **Global plagiarism** — copying nearly all of another submission.
2. **Direct/verbatim plagiarism** — copying whole functions or blocks.
3. **Paraphrasing plagiarism** — renaming variables / minor reordering that leaves token structure mostly
   intact.
4. **Self-plagiarism** — resubmitting one's own prior work as new.
5. **Patchwork/mosaic plagiarism** — interleaving copied fragments from multiple sources with original
   content.

To keep the parsing problem out of scope, the course provides a fixed `tokenizer_t` class
(`tokenizer.hpp` / `tokenizer.cpp`) that uses `libclang` to walk a file's AST and emit a
`std::vector<int>` of Clang cursor-kind values, restricted to cursors from the file itself
(`is_from_main_file`). Every phase operates purely on these integer streams — never on raw text — which is
precisely what makes variable-renaming attacks ineffective while leaving semantics-preserving reorderings
harder to catch.

## 2. Phase 1 — pairwise checker (design requirements)

**Interface:** `std::array<int, 5> match_submissions(std::vector<int> &submission1, std::vector<int> &submission2)`

The function must find two classes of matching subsequences between the two token streams:

- **Short exact matches** (~10–20 tokens): identical token subsequences, in order. These must not overlap
  within either file and must not be double-counted — a requirement that directly targets patchwork
  plagiarism (many small copied fragments stitched into original code).
- **One long approximate match** (30+ tokens): the longest pair of subsequences that are at least 80% as
  long as the longer of the two, allowing a few inserted/deleted/changed tokens. This targets paraphrasing
  plagiarism (a few added statements or renamed identifiers) that an exact-match-only approach would miss.

**Required output** (`std::array<int, 5>`):

| Index | Meaning |
|---|---|
| 0 | Plagiarism flag (1/0) — threshold is left to the implementer, "reasonable" per the brief |
| 1 | Total length of all non-overlapping short exact matches |
| 2 | Length of the longest approximate match (0 if none found ≥30 tokens) |
| 3 | Start index of that match in `submission1` |
| 4 | Start index of that match in `submission2` |

Accuracy is prioritized over raw speed in this phase, though the brief still expects efficient code since
some submissions run to a few thousand tokens.

## 3. Phase 2 — bulk checker (design requirements)

**Interface:** `class plagiarism_checker_t`, constructed with a set of pre-existing ("original") tokenized
submissions, exposing a single mutating entry point: `void add_submission(std::shared_ptr<submission_t>)`.

Key design constraints from the brief:

- **Non-blocking API.** `add_submission` must record a `chrono::time_point` timestamp (taken *before*
  tokenizing, to minimize timing skew) and return immediately; the actual tokenization, matching, and
  flagging must happen on background thread(s), synchronized with a `std::mutex` (or condition
  variables/semaphores) around shared state.
- **Flagging rules**, evaluated per new submission against the growing database:
  - Flag if there is an exact match of **≥75 tokens** against any single other submission.
  - Flag if there are **≥10 total matches** (each ≥15 tokens, exact — approximate matching is explicitly
    dropped in this phase for speed) against a single other submission.
  - Flag for **patchwork plagiarism** if ≥20 total matches exist in aggregate against the pool of all prior
    submissions (by timestamp, considered up to one second after the current submission's timestamp).
  - Timestamp-based attribution: if two flagged submissions differ by ≥1 second, only the later one is
    flagged; if <1 second apart, both are flagged. Null student/professor pointers are skipped. Seed
    ("original") submissions from the constructor are never flagged themselves (their timestamp is treated
    as 0).
- **Efficiency is paramount** here — the brief explicitly calls out that a naive O(n²) pairwise scan against
  every prior submission is not acceptable at scale; some indexing/precomputation strategy is expected
  (e.g., a shared trie/suffix structure or hash index across all stored token streams, as opposed to
  re-running a matcher against each stored submission independently).

## 4. Phase 3 — hack the checker

Phase 3 supplies six **complete** implementations of the Phase 1 `match_submissions` interface — one TA
reference solution and five solutions from peer teams (`phase3/checker_zero.hpp` through
`checker_five.hpp`; provenance listed in `phase3/README.md`) — as targets for constructing adversarial
input pairs that produce false positives or false negatives. Reading through the six implementations shows
a range of distinct algorithmic strategies, each with different failure modes:

- **`checker_zero.hpp` (TA reference).** Classic **KMP** (failure-function + search) to locate exact
  matches, plus a DP pass (`net_match_len`) to combine non-overlapping matches, and an LCS-style DP
  (`is_approx_match`, `longest_subseq`) extended along diagonals to find the longest approximate block.
  Flags when total short-match length ≥100 or the longest approximate match ≥75. As a KMP/LCS-DP hybrid, it
  is relatively precise on ordered exact/approximate matches but — like any LCS-based approach — is
  sensitive to *how* the DP window/threshold is tuned, which is a natural target for edge-case token
  streams that sit just below its thresholds.
- **`checker_one.hpp`.** A **polynomial rolling hash** (`class RollingHash`, base 257, mod 1e9+7) for O(1)
  substring-hash comparison, used to find exact matches efficiently. Rolling-hash matchers are fast but rely
  on hash comparisons standing in for true equality; without a fallback verification step they are
  susceptible to hash collisions, and being exact-match-only, they are also a natural target for
  paraphrasing-style token perturbations that break every fixed-length window.
- **`checker_two.hpp`.** A bespoke, unnamed exact/approximate matching routine (no reusable named
  algorithm/class) — its ad hoc structure makes its exact matching thresholds and edge-case behavior the
  most implementation-specific of the six, and therefore the hardest to predict without directly reading it
  end-to-end for a given hack attempt.
- **`checker_three.hpp`.** `class SequenceMatcher` — an **LCS DP with backtracking** (`findValidSequences`)
  to reconstruct actual matched index pairs from the DP table, rather than just a match length. This gives
  it precise match locations but the classic O(n·m) LCS DP cost, and backtracking logic is a common source
  of off-by-one edge cases around match boundaries.
- **`checker_four.hpp`.** Splits matching into two dedicated classes, `Exact_Match` and `Approx_Match`, the
  latter also reconstructing matches from an LCS DP table. The separation of exact vs. approximate logic
  makes it a good target for probing whether the two paths agree at the boundary lengths where a match
  could plausibly be classified as either.
- **`checker_five.hpp`.** The most elaborate of the six: a restricted `NotAcceptingTrie` (fixed-length-10
  code insertion) plus a `SuffixTree` explicitly implementing **Ukkonen's algorithm** for efficient
  substring matching at scale. This is the closest in spirit to what Phase 2 calls for; its complexity also
  makes it the most likely to have subtle correctness edge cases around suffix-tree construction/traversal
  that a targeted adversarial input could expose.

## 5. Current implementation status

| Component | File(s) | Status |
|---|---|---|
| Phase 1 pairwise matcher | `phase1/match_submissions.hpp` | **Not implemented** — stub returns `{0,0,0,0,0}` |
| Phase 2 bulk checker | `phase2/plagiarism_checker.cpp` | **Not implemented** — empty method bodies |
| Phase 3 hacking targets | `phase3/checker_*.hpp` | Complete, provided as-is |
| Tokenizer / harnesses / testcases | `tokenizer.hpp`, `phase{1,2}/main.cpp`, `phase{1,2}/tokenizer.cpp`, `Makefile`s, `Dockerfile` | Complete, course-provided, not to be modified |

The algorithmic core of Phases 1 and 2 remains to be written; the six Phase 3 reference checkers above
serve as concrete worked examples of the range of approaches that satisfy the Phase 1 interface.

## 6. Toolchain notes

Because `libclang`'s tokenization behavior can vary across machines/versions, the course pins the build to
a Docker image (`ubuntu:24.04` + `clang`/`llvm-18`/`libclang-dev`) rather than relying on a native install;
both phase Makefiles hardcode `/usr/lib/llvm-18/{include,lib}` accordingly. The brief also imposes a code
style rubric for manual grading: functions averaging ~30 lines, ≤4 levels of nesting, ≤100-character lines,
descriptive naming, and sparse, purposeful comments (see `problem_statement.pdf` §5.3) — a good baseline to
follow when implementing Phases 1 and 2.
