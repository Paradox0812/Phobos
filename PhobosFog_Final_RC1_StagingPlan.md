# PhobosFog Final RC1 Staging Plan

This document triages the current PhobosFog RC1 working tree into commit-scope groups.

It is a staging recommendation only. No files are staged by this document.

## 1. Current Git Snapshot

Commands reviewed during Final Stage-A:

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

## 2. A. Must Submit Runtime Production Code

These files contain the PhobosFog runtime implementation and should be staged if the RC1 feature is accepted:

```text
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

Runtime scope represented by these files:

- global PhobosFog INI tags and serialization;
- house-level fog state and explored-only save/load payload;
- sight refresh, reveal sync, SpyPlane, SpySat, full-map reveal, and crate integration;
- explored overlay pipeline and final-region cache;
- render-only presentation gates;
- radar fog and radar object gates;
- hover UI and hidden-object command gates;
- auto-target, fire-time, and force-fire cell gates;
- stream helpers required by optional `PFOG` tail payload.

## 3. Required Developer Tooling

These are not game runtime production files, but they are required if the hook replacement tooling changes are part of the RC1 commit:

```text
.agents/skills/check-hooks/check_hook_conflicts.py
.agents/skills/check-hooks/hook_replacements.json
.agents/skills/check-hooks/test_hook_replacements.py
```

Recommended decision:

- Stage them with the RC if intentional hook replacement support is required to validate the current hook set.
- Otherwise split them into a preceding tooling commit and re-run hook checks/build after staging.

## 4. B. Must Submit User Or Development Documentation

Core living docs and final handoff docs recommended for staging:

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
```

Optional but useful RC handoff docs:

```text
PhobosFog_P9_Final_Handoff_RC1.md
PhobosFog_P9_Z_C_FinalSanityValidationChecklist.md
PhobosFog_CurrentBaseline_And_Feature_Summary.md
```

The generated `.docx` summary should be staged only if the project wants binary handoff artifacts in git.

## 5. C. Suggested Local-Only Phase Reports

Most root-level phase reports are valuable history but are noisy for a final feature commit.

Recommended default:

- Keep locally for traceability.
- Do not stage unless the user wants a full development audit trail in the repository.
- Prefer the compact final docs in section B as the commit-facing documentation.

Examples of local-only phase report groups:

```text
PhobosFog_P2_*.md
PhobosFog_P3_*.md
PhobosFog_P4_*.md
PhobosFog_P5_*.md
PhobosFog_P6_*.md
PhobosFog_P7_*.md
PhobosFog_P8_*.md
PhobosFog_P9_Task*.md
PhobosFog_SaveLoad_B1_OptionalPayloadCompatibilityProbe.md
PhobosFog_SaveLoad_B2A_MinimalStreamOptionalTailHelper.md
```

Exceptions can be made for reports that directly justify a high-risk hook or save/load decision.

## 6. D. Do Not Submit Logs Or Temporary Files

Do not stage:

```text
PhobosFogP2_CurrentDiff_AfterTask5D.patch
PhobosFogP2_CurrentDiff_AfterTask5G.patch
PhobosFogP2_git_status_AfterTask5D.txt
PhobosFogP2_git_status_AfterTask5G.txt
```

Also do not stage if present:

```text
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

Do not stage local game runtime files or copied Yuri's Revenge files.

## 7. E. Needs Manual Confirmation

These files require explicit user decision before staging:

```text
AGENTS.md
PhobosFog_CurrentBaseline_And_Feature_Summary.docx
```

`AGENTS.md` is currently local and untracked. It must not be staged unless the user explicitly decides to add it as a tracked project coordination file.

The `.docx` file is a generated binary summary. It should be staged only if binary handoff artifacts are intentionally accepted.

## 8. Suggested Commit Shapes

If using one feature commit:

1. Stage all section A runtime code.
2. Stage required developer tooling from section 3 if hook replacement support is needed.
3. Stage section B living/final docs.
4. Do not stage local-only phase reports or temporary artifacts.
5. Run build and hook checks before commit.

If splitting commits:

1. Hook tooling support.
2. Core PhobosFog INI/state/refresh/reveal/save-load.
3. Presentation gating.
4. Command and combat gating.
5. Explored overlay and performance cache.
6. Living docs and final RC handoff docs.

After each split, re-run at minimum:

```bat
git diff --check
scripts\build_debug.bat
git diff --cached --name-only
```

## 9. Notes For Final Cleanup

- `PhobosFog_INI_Tags.md` currently has historical numbering drift in later sections. This is a documentation cleanup candidate, not a Stage-A code change.
- Keep `PhobosFog.HideBuildings` documented as live-building presentation gating, not total building erasure.
- Keep `SpySatellite.DeactivateHoldFrames` explicit-only unless a dedicated task changes semantics.
- Keep SaveLoad-C scoped to explored-only persistence unless a future save/load task expands the payload.
