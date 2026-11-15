| # | Step | Skill | Output | Exit rule |
| --- | --- | --- | --- | --- |
| 0 | Plan: new library or extension, and the API shape | [`plan-driver`](../.claude/skills/plan-driver/SKILL.md) | `placement.md` with a verdict | Verdict `NEW`; otherwise stop or extend |
| 1 | Scaffold | [`new-driver-from-template`](../.claude/skills/new-driver-from-template/SKILL.md) | A building library with the example renamed | `verify` passes on the untouched scaffold |
| 2 | Build | [`cpp20-profile`](../.claude/skills/cpp20-profile/SKILL.md) | Headers, tests, docs | Host tests green |
| 3 | Self-review | [`misra-review`](../.claude/skills/misra-review/SKILL.md) | Findings fixed | Every catalogue class in `recurring-defects.md` answered |
| 4 | Verify | [`verify`](../.claude/skills/verify/SKILL.md) | Local gate result | `tools/preflight.sh` ends `0 failure(s)` |
| 5 | Independent review | [`review-round`](../.claude/skills/review-round/SKILL.md) | A report per round | `NO BLOCKER` and no MAJOR |
| 6 | First-push gate | [`first-push`](../.claude/skills/first-push/SKILL.md) | Container rehearsal and the evidence list | `0 failure(s)` and `0 gate failure(s)` |
| 7 | Publish | [`publish-driver`](../.claude/skills/publish-driver/SKILL.md) | Public repository, one commit, green `main`, Pages | `CI gate / gate` green on `main`; the owner did the owner-only steps |
| 8 | Release candidate | [`release`](../.claude/skills/release/SKILL.md) | Tag `v0.1.0-rc.N` | Release workflow green; checksums verified |
| 9 | After release | [`bump-pin`](../.claude/skills/bump-pin/SKILL.md) | A pin-bump pull request per consumer | Consumer CI green |
