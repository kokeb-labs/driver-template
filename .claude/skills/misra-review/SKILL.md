---
name: misra-review
description: Review a change against the handbook's MISRA C++:2023 compliance matrix, guideline by guideline, citing numbers only and recording deviations the handbook's way. Use on every pull request that changes C++ in inc/, src/, tests/ or examples/, and before a release at the high or safety level.
---

# MISRA C++:2023 review

The handbook's compliance matrix is the reference, not the standard itself. The matrix gives each guideline's
number, MISRA's category, the category we enforce, a one-line summary in our own words and how it is verified.
The standard's text is copyrighted: never quote it, paraphrase it beyond the matrix summary, paste it into code,
commits, pull requests or comments, or upload or attach the licensed copy. Cite guidelines by number
(`Rule 7.0.5`, `Dir 0.3.1`).

1. **Use the pinned handbook.** The commit the repository follows is the `ref:` of the handbook checkout in
   `.github/workflows/ci-repo-checks.yml`. Fetch the matrix and the project-wide deviations at that commit:
   `curl -fsSL https://raw.githubusercontent.com/kokeb-labs/handbook/<ref>/compliance/misra-cpp-2023.yml` and the
   same for `compliance/misra-cpp-2023-deviations.yml`. Read the repository's level in `.kokeb/handbook.yml`.
2. **Find the scope.** `git diff --name-only origin/main...HEAD -- inc src tests examples` and, for each changed
   file, the changed lines (`git diff -U0`). Review only what changed, plus any code the change makes reachable.
3. **Map each changed construct to guidelines.** For each one (a cast, a shift, a loop, a switch, a lambda, a
   template, a buffer access, an initialisation, a header), find the guidelines whose matrix `summary` covers it.
   Then follow each entry's `verification`:
   - a tool entry without `partial: true` decides the guideline: confirm the CI job that runs it passed (or run
     the tool locally) instead of judging it yourself;
   - a `partial` tool entry finds some violations only: run it, then also do the entry's `review` step;
   - a `review` entry: check the changed code against its `how` text and say what you checked;
   - `not-applicable`: confirm the construct the reason rules out really is absent (for example, no `throw`).
   Apply the entry's `our_category`, never a weaker one.
4. **C++20 constructs** are reviewed through deviation MD-CPP-001 and the handbook's C++20 profile: a feature the
   profile allows is checked against its KB-CPP20 guidelines and the profile page's mapping table (a concept as a
   template, span indexing as array access, and so on). A forbidden feature, a deprecated one or anything from
   C++23 or later is a violation of Rule 4.1.1 with no deviation. Run the `cpp20-profile` skill for these.
5. **Licensed text and private notes, only if you hold them.** If you hold a licensed copy of MISRA C++:2023
   and the maintainers' private interpretation notes, use them to decide how a guideline applies; otherwise work
   from the handbook's compliance matrix: guideline numbers and its summaries only. Never quote the standard or
   the notes in this repository, a pull request or a review comment.
6. **Deviations follow the handbook's model.** A project-wide deviation (`MD-CPP-NNN`) covers a finding only
   inside its `scope`. A finding in one place that is correct and deliberate gets a suppression that names the
   check and the reason on the line (`// NOLINTNEXTLINE(<check>): <reason>`), and at the high and safety levels an
   entry in `docs/deviations.yml` (`id`, `rule`, `check`, `where`, `reason`, `risk`, `approved_by`, `date`). A
   mandatory guideline is never deviated. Do not add a deviation or a waiver yourself: propose it in the report
   for the maintainer to approve, and fix the code when a fix exists.
7. **Report** in the pull request (or to whoever asked), ranked by our category, mandatory first:

   | Guideline | Our category | File:line | Finding | Fix, or deviation to propose |
   | --- | --- | --- | --- | --- |

   Then two lists: the guidelines decided by a tool in CI for this change (with the job), and those you reviewed
   by hand (with the review step). Say which tools you could not run locally. At the safety level, add that the
   commercial checker's run in CI is the final word for every guideline no open tool decides.
