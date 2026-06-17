# PhobosFog Final RC1 Staging Dry Run

This document is a dry-run staging plan for the current PhobosFog RC1 working tree.

No `git add`, staging, or commit was performed while creating this document.

## 1. Current Git Snapshot

Commands reviewed:

```bat
git status --short
git diff --stat
git diff --name-only
```

Tracked diff snapshot:

```text
24 tracked files changed, 9329 insertions(+), 30 deletions(-)
```

Tracked modified files:

```text
.agents/skills/check-hooks/check_hook_conflicts.py
src/Ext/Aircraft/Hooks.cpp
src/Ext/Anim/Body.cpp
src/Ext/Anim/Hooks.cpp
src/Ext/House/Body.cpp
src/Ext/House/Body.h
src/Ext/ParticleType/Hooks.cpp
src/Ext/Rules/Body.cpp
src/Ext/Rules/Body.h
src/Ext/Scenario/Body.cpp
src/Ext/Techno/Hooks.Firing.cpp
src/Ext/Techno/Hooks.Misc.cpp
src/Ext/Techno/Hooks.Pips.cpp
src/Ext/Techno/Hooks.cpp
src/Ext/TechnoType/Hooks.cpp
src/Ext/TerrainType/Hooks.cpp
src/Ext/WarheadType/Body.cpp
src/Ext/WarheadType/Body.h
src/Ext/WarheadType/Detonate.cpp
src/Misc/Hooks.Crates.cpp
src/Misc/Hooks.UI.cpp
src/Misc/Hooks.VeinholeMonster.cpp
src/Utilities/Stream.cpp
src/Utilities/Stream.h
```

Important untracked files include:

- final docs and living PhobosFog docs;
- many root-level PhobosFog phase reports;
- hook replacement tooling files;
- local patch/status artifacts;
- local `AGENTS.md`.

## 2. Global Dry-Run Rules

Do not stage:

- `AGENTS.md`;
- `debug.log`;
- `*.log`;
- temporary test files;
- temporary patch/status artifacts;
- copied DLL/PDB output;
- full disassembly dumps;
- large batches of `PhobosFog_P*_Task*.md` phase reports by default;
- Codex temporary instruction/request documents.

Preferred staging mechanics:

- Use full-file staging only when a file belongs cleanly to one commit.
- Use `git add -p` or equivalent patch staging for shared files that span multiple commit scopes.
- Re-run build after each staged commit if the split is actually performed.
- Keep `git diff --cached --name-only` empty until the user explicitly approves staging.

## 3. Commit 0: Stream Optional Payload Helpers

Draft commit message:

```text
Add stream helpers for optional extension payloads
```

Suggested files:

```text
src/Utilities/Stream.h
src/Utilities/Stream.cpp
```

Review focus:

- Remaining-byte query is safe.
- Peek helpers are non-consuming.
- Existing save/load stream behavior remains unchanged.
- No unknown optional-tail magic is consumed accidentally.

Dry-run staging note:

- These files are cleanly scoped and can likely be staged as whole files.

## 4. Commit 1: Core PhobosFog State, Rules, Refresh, Reveal, Save-Load

Draft commit message:

```text
Add PhobosFog state refresh and explored persistence
```

Suggested files:

```text
src/Ext/Rules/Body.h
src/Ext/Rules/Body.cpp
src/Ext/House/Body.h
src/Ext/House/Body.cpp
src/Ext/Scenario/Body.cpp
src/Ext/Aircraft/Hooks.cpp
src/Ext/WarheadType/Body.h
src/Ext/WarheadType/Body.cpp
src/Ext/WarheadType/Detonate.cpp
src/Misc/Hooks.Crates.cpp
```

Review focus:

- `PhobosFog.Enabled=false` remains default-off.
- All new `[General]` and `[WarheadType]` tags are loaded, clamped, and serialized.
- House-level `Unknown / Explored / Visible` state and effective visibility semantics are consistent.
- Sight refresh and reveal-source sync do not run when disabled.
- `PFOG` save/load stores explored memory only.
- Load resets runtime-only state and requests one forced refresh.

Dry-run staging note:

- `src/Ext/House/Body.*` and `src/Ext/Scenario/Body.cpp` also support overlay/radar/cache behavior, so review before whole-file staging.
- If the commit split must be extremely strict, use patch staging for the save/load/state portions.

## 5. Commit 2: Presentation, Overlay, Radar, And UI Hiding

Draft commit message:

```text
Gate PhobosFog presentation and radar visibility
```

Suggested files:

```text
src/Ext/Techno/Hooks.Pips.cpp
src/Ext/Techno/Hooks.cpp
src/Ext/TechnoType/Hooks.cpp
src/Ext/Anim/Body.cpp
src/Ext/Anim/Hooks.cpp
src/Ext/ParticleType/Hooks.cpp
src/Ext/TerrainType/Hooks.cpp
src/Misc/Hooks.UI.cpp
src/Misc/Hooks.VeinholeMonster.cpp
```

Review focus:

- Render-only hooks do not affect simulation logic.
- Enemy FootClass hiding uses current player plus valid allied hard visibility.
- Live building presentation is hard-visible gated while explored snapshots remain allowed.
- World anim, particle, terrain, and radar presentation gates remain opt-in.
- Hover cursor, tooltip/name, and health bar gates do not leak hidden objects.
- Overlay visual baseline remains the accepted RC1 baseline.

Dry-run staging note:

- `src/Misc/Hooks.VeinholeMonster.cpp` contains both overlay presentation and performance cache optimization. If commits 2 and 4 are split, this file requires patch staging.
- `src/Ext/Techno/Hooks.cpp` and `src/Ext/TechnoType/Hooks.cpp` may also contain cross-scope UI/command work. Review hunks before staging.

## 6. Commit 3: Command And Combat Gating

Draft commit message:

```text
Gate hidden PhobosFog commands and combat targets
```

Suggested files:

```text
src/Ext/Techno/Hooks.Firing.cpp
src/Ext/Techno/Hooks.Misc.cpp
src/Ext/TechnoType/Hooks.cpp
```

Review focus:

- `GateHiddenObjectCommands` is command presentation/control, not AI behavior.
- `GateAutoTargets` rejects non-hard-visible enemy live object candidates.
- `GateAutoFire` clears non-hard-visible enemy live object targets at fire time.
- `GateForceFireCells` rejects hidden direct cell and terrain-backed cell targets.
- Combat gates use attacker owner plus valid allies, not UI `CurrentPlayer`.
- Already-fired projectiles are not modified.

Dry-run staging note:

- `src/Ext/TechnoType/Hooks.cpp` overlaps with hover UI gating in commit 2. Use patch staging if keeping command/UI commits separate.

## 7. Commit 4: Performance Cache And Overlay Optimization

Draft commit message:

```text
Optimize PhobosFog explored overlay region drawing
```

Suggested files:

```text
src/Misc/Hooks.VeinholeMonster.cpp
src/Ext/House/Body.h
src/Ext/House/Body.cpp
src/Ext/Scenario/Body.cpp
```

Review focus:

- Active overlay path is direct template-to-row-bucket.
- Legacy cliff-cover collector stays bypassed.
- Final-region cache key uses viewer identity, overlay-effective visibility hash, viewport hash, and config hash.
- Raw `PhobosFog_StateVersion` is not used as overlay cache dirty key.
- `TemporalCacheFirstInvalidFrame` avoids full `LastVisibleFrames` scans.
- `SoftEdge` and `FadeIn` still disable final-region cache reuse.
- `PhobosFog.Perf.Enabled` remains independent from `PhobosFog.Debug`.

Dry-run staging note:

- This commit overlaps heavily with commits 1 and 2. If the history must be clean, use patch staging. If patch staging becomes too risky, prefer a single feature commit after review rather than forcing an artificial split.

## 8. Commit 5: Developer Tooling

Draft commit message:

```text
Support intentional hook replacement checks
```

Suggested files:

```text
.agents/skills/check-hooks/check_hook_conflicts.py
.agents/skills/check-hooks/hook_replacements.json
.agents/skills/check-hooks/test_hook_replacements.py
```

Review focus:

- Intentional hook replacements are declared explicitly.
- Existing non-overlap hook checks still work.
- Tooling change is not mixed silently with runtime behavior.

Dry-run staging note:

- This can be Commit 0 instead if hook tooling must exist before reviewing runtime hooks.
- Keep separate from game runtime commits if possible.

## 9. Commit 6: Docs, Semantic Baseline, And RC Handoff

Draft commit message:

```text
Document PhobosFog RC1 baseline and validation
```

Suggested files:

```text
PhobosFog_INI_Tags.md
devDocs/PhobosFog_Semantic_Debt.md
devDocs/PhobosFog_Algorithm_Inventory_For_Performance.md
PhobosFog_P9_Z_PerformanceBaselineFreeze.md
PhobosFog_SaveLoad_B2B_ExploredOnlyPayload.md
PhobosFog_SaveLoad_C_ManualAcceptanceRecord.md
PhobosFog_Final_RC1_Handoff.md
PhobosFog_Final_RC1_VerificationSummary.md
PhobosFog_Final_RC1_CommitCandidateChecklist.md
PhobosFog_Final_RC1_StagingPlan.md
PhobosFog_Final_RC1_StagingDryRun.md
```

Optional docs if the user wants broader local history in git:

```text
PhobosFog_P9_Final_Handoff_RC1.md
PhobosFog_P9_Z_C_FinalSanityValidationChecklist.md
PhobosFog_CurrentBaseline_And_Feature_Summary.md
```

Review focus:

- INI registry matches code defaults and semantics.
- Semantic debt and algorithm inventory describe the current accepted baseline, not failed experiments as active paths.
- SaveLoad-C record states explored-only persistence.
- Known limitations are explicit.

Dry-run staging note:

- Do not stage `PhobosFog_CurrentBaseline_And_Feature_Summary.docx` unless binary docs are intentionally accepted.

## 10. Explicitly Not Stage

Do not stage these current files or patterns:

```text
AGENTS.md
PhobosFogP2_CurrentDiff_AfterTask5D.patch
PhobosFogP2_CurrentDiff_AfterTask5G.patch
PhobosFogP2_git_status_AfterTask5D.txt
PhobosFogP2_git_status_AfterTask5G.txt
PhobosFog_CurrentBaseline_And_Feature_Summary.docx
PhobosFog_P*_Task*.md
*.log
debug.log
except.txt
*.asm.txt
*.dumpbin.txt
*.objdump.txt
Debug/Phobos.dll
Debug/Phobos.pdb
DevBuild/Phobos.dll
DevBuild/Phobos.pdb
```

The `PhobosFog_P*_Task*.md` exclusion is a default recommendation. A specific phase report can be staged only if the user explicitly wants it preserved as review evidence.

## 11. Needs Human Confirmation

Files requiring explicit user decision:

```text
AGENTS.md
PhobosFog_CurrentBaseline_And_Feature_Summary.docx
PhobosFog_CurrentBaseline_And_Feature_Summary.md
PhobosFog_P9_Final_Handoff_RC1.md
PhobosFog_P9_Z_C_FinalSanityValidationChecklist.md
```

Decision points:

- Should `AGENTS.md` remain local-only?
- Should the generated `.docx` be kept outside git?
- Should broad phase reports stay local, or should a curated subset be included for auditability?
- Should hook tooling be committed before runtime code?
- Should the branch use one feature commit instead of many patch-staged commits?

## 12. Validation Commands For Any Real Staging

After actual staging, run:

```bat
git diff --check
scripts\build_debug.bat
git diff --cached --name-only
```

If splitting commits, repeat build and at least smoke validation after each committed split.

## 13. Dry-Run Verdict

The split above is feasible but not frictionless.

The main risk is that several files carry cross-cutting PhobosFog work:

- `src/Ext/House/Body.*`
- `src/Ext/Scenario/Body.cpp`
- `src/Misc/Hooks.VeinholeMonster.cpp`
- `src/Ext/TechnoType/Hooks.cpp`

If the user wants clean semantic commits, use patch staging carefully. If minimizing staging risk is more important than clean history, prefer one reviewed feature commit plus a separate hook-tooling commit and a separate docs commit.
