# PhobosFog Final RC1 Commit Candidate Checklist

This checklist is for the final pre-commit and PR review pass.

## 1. Commit Candidate Status

Current status:

- Broad PhobosFog feature work exists in a dirty working tree.
- Final RC1 documentation files are newly added.
- No files are staged.
- No commit has been made by this RC1 pass.
- Local `AGENTS.md` remains untracked and must not be staged unless explicitly approved.

## 2. Must-Review Source Areas

Review these tracked modified areas before commit:

- `src/Ext/Rules/Body.h`
- `src/Ext/Rules/Body.cpp`
- `src/Ext/House/Body.h`
- `src/Ext/House/Body.cpp`
- `src/Ext/Scenario/Body.cpp`
- `src/Misc/Hooks.VeinholeMonster.cpp`
- `src/Ext/Techno/Hooks.Pips.cpp`
- `src/Ext/Techno/Hooks.Firing.cpp`
- `src/Ext/Techno/Hooks.Misc.cpp`
- `src/Ext/Techno/Hooks.cpp`
- `src/Ext/TechnoType/Hooks.cpp`
- `src/Ext/Anim/Body.cpp`
- `src/Ext/Anim/Hooks.cpp`
- `src/Ext/ParticleType/Hooks.cpp`
- `src/Ext/TerrainType/Hooks.cpp`
- `src/Ext/Aircraft/Hooks.cpp`
- `src/Ext/WarheadType/Body.h`
- `src/Ext/WarheadType/Body.cpp`
- `src/Ext/WarheadType/Detonate.cpp`
- `src/Misc/Hooks.Crates.cpp`
- `src/Misc/Hooks.UI.cpp`
- `src/Utilities/Stream.h`
- `src/Utilities/Stream.cpp`
- `.agents/skills/check-hooks/check_hook_conflicts.py`

## 3. Must-Review Documentation Areas

Review these docs before commit:

- `PhobosFog_INI_Tags.md`
- `devDocs/PhobosFog_Semantic_Debt.md`
- `devDocs/PhobosFog_Algorithm_Inventory_For_Performance.md`
- `PhobosFog_P9_Z_PerformanceBaselineFreeze.md`
- `PhobosFog_P9_Final_Handoff_RC1.md`
- `PhobosFog_P9_Z_C_FinalSanityValidationChecklist.md`
- `PhobosFog_Final_RC1_Handoff.md`
- `PhobosFog_Final_RC1_VerificationSummary.md`
- `PhobosFog_Final_RC1_CommitCandidateChecklist.md`

Important missing or incomplete documentation records:

- `PhobosFog_SaveLoad_B2B_ExploredOnlyPayload.md` is present again and records the explored-only PFOG payload stage.
- `PhobosFog_SaveLoad_C_ManualAcceptanceChecklist.md` or equivalent SaveLoad-C manual acceptance record is not present in the current tree.

## 4. Do Not Stage

Do not stage:

- `AGENTS.md`, unless the user explicitly decides it should become a tracked project file;
- local runtime game files;
- copied `Phobos.dll` or `Phobos.pdb`;
- `debug.log`;
- `except.txt`;
- temporary disassembly dumps;
- temporary patch/status artifacts unless intentionally kept as part of the handoff.

Current local artifacts that need explicit decision before staging:

- `PhobosFogP2_CurrentDiff_AfterTask5D.patch`
- `PhobosFogP2_CurrentDiff_AfterTask5G.patch`
- `PhobosFogP2_git_status_AfterTask5D.txt`
- `PhobosFogP2_git_status_AfterTask5G.txt`
- many root-level phase reports
- generated summary `.docx`

## 5. Hook Review Checklist

Before committing hook changes:

- Confirm every new or changed hook has a recorded address, size, stolen bytes, and return path.
- Confirm render-only hooks do not affect logic, AI, damage, targeting, or sync unless explicitly intended.
- Confirm intentional hook replacements are documented through the check-hooks replacement tooling.
- Run check-hooks tooling if this branch is being prepared for formal review.

Required before PR, if not already run in the final review pass:

```bat
python .agents\skills\check-hooks\discover_hooks.py --json-only
python .agents\skills\check-hooks\check_hook_conflicts.py
```

## 6. INI Tag Checklist

Before commit:

- Confirm every `PhobosFog.*` tag in code appears in `PhobosFog_INI_Tags.md`.
- Confirm defaults in docs match `RulesExt::ExtData` and `WarheadTypeExt::ExtData`.
- Confirm serialization includes every INI-backed field.
- Confirm `PhobosFog.Enabled=false` remains default.
- Confirm no upstream Phobos tag is silently redefined.

Known accepted semantics to keep:

- `PhobosFog.SpySatellite.DeactivateHoldFrames` is explicit-only.
- `PhobosFog.HideBuildings` is live-building presentation gating, not total building erasure.
- `PhobosFog.Perf.Enabled` is independent from `PhobosFog.Debug`.

## 7. Save/Load Checklist

Before commit:

- Confirm optional stream helpers remain non-consuming for peek.
- Confirm no unrecognized optional tail magic is consumed.
- Confirm `PFOG` persists explored memory only.
- Confirm `Visible`, temporary visible holds, full-map holds, overlay cache, row buckets, templates, perf counters, and debug counters are not serialized.
- Confirm load touches raw and overlay-effective versions.
- Confirm load requests one forced fog refresh.
- Add or locate a SaveLoad-C manual acceptance record before declaring save/load fully accepted.

## 8. Performance Checklist

Before commit:

- Run a cacheable overlay perf preset.
- Confirm `PhobosFog.Perf.Enabled=true` does not enable debug RegionMask or CliffProbe output.
- Confirm cache hit path appears after warm-up when SoftEdge and FadeIn are disabled.
- Confirm `TemporalCellExpiryScanSkipped=true` remains present in perf output.
- Confirm no full `PhobosFog_LastVisibleFrames` scan is reintroduced into overlay cache decision.

## 9. Manual Gameplay Checklist

Recommended RC map checks:

- disabled baseline;
- explored overlay with scrolling;
- height-discontinuity terrain;
- enemy foot hiding;
- building explored snapshot;
- building animation hard-visible gating;
- hover cursor, tooltip, health bar;
- hidden object command gating;
- auto target and fire-time gates;
- force-fire cell and terrain-backed cell gates;
- radar object hiding and radar fog;
- SpyPlane reveal;
- SpySat active and deactivate hold;
- warhead reveal;
- save/load explored memory.

## 10. Required Command Checks

Run before commit:

```bat
git diff --check
scripts\build_debug.bat
git diff --cached --name-only
```

Current RC1 command result:

```text
git diff --check:
Passed. Git reported existing LF-to-CRLF normalization warnings, but no whitespace errors.

scripts\build_debug.bat:
Passed. `Debug\Phobos.dll` was produced by `Phobos.vcxproj`.

git diff --cached --name-only:
Passed. Output was empty.
```

## 11. Suggested Commit Shape

This branch is large. Prefer one carefully reviewed commit only if the user wants to preserve the entire PhobosFog workstream as one feature commit.

If splitting is desired, recommended split order:

1. Hook tooling replacement support.
2. Core INI/state/refresh/reveal/save-load.
3. Presentation gates.
4. Command/combat gates.
5. Explored overlay and performance cache.
6. Documentation and RC handoff.

Do not split without re-running build and at least smoke validation after each split.
