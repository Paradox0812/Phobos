# PhobosFog Final RC1 Verification Summary

This document records Final RC1 command validation and recommended manual verification coverage.

## 1. Stage Scope

Final RC1 verification summary is documentation-only.

This stage does not modify production code, hooks, INI tags, visual behavior, cache semantics, save/load semantics, command gating, or combat gating.

## 2. Source Documents Reviewed

Reviewed or attempted during this pass:

- `PhobosFog_INI_Tags.md`
- `devDocs/PhobosFog_Semantic_Debt.md`
- `devDocs/PhobosFog_Algorithm_Inventory_For_Performance.md`
- `PhobosFog_P9_Z_PerformanceBaselineFreeze.md`
- `PhobosFog_P9_Final_Handoff_RC1.md`
- `PhobosFog_P9_Z_C_FinalSanityValidationChecklist.md`
- `PhobosFog_SaveLoad_B2B_ExploredOnlyPayload.md`
- `PhobosFog_SaveLoad_C_ManualAcceptanceChecklist.md` or equivalent SaveLoad-C record was requested but not found in the current working tree.

SaveLoad-B2B explored-only `PFOG` behavior is now recorded in both the standalone B2B report and the living semantic docs.

## 3. Git State Snapshot

Command:

```bat
git status --short
```

Result:

```text
Exit code: 0
Working tree is dirty from the active PhobosFog workstream.
Tracked modified files include PhobosFog source/tooling files such as:
- .agents/skills/check-hooks/check_hook_conflicts.py
- src/Ext/Aircraft/Hooks.cpp
- src/Ext/Anim/Body.cpp
- src/Ext/Anim/Hooks.cpp
- src/Ext/House/Body.cpp
- src/Ext/House/Body.h
- src/Ext/ParticleType/Hooks.cpp
- src/Ext/Rules/Body.cpp
- src/Ext/Rules/Body.h
- src/Ext/Scenario/Body.cpp
- src/Ext/Techno/Hooks.Firing.cpp
- src/Ext/Techno/Hooks.Misc.cpp
- src/Ext/Techno/Hooks.Pips.cpp
- src/Ext/Techno/Hooks.cpp
- src/Ext/TechnoType/Hooks.cpp
- src/Ext/TerrainType/Hooks.cpp
- src/Ext/WarheadType/Body.cpp
- src/Ext/WarheadType/Body.h
- src/Ext/WarheadType/Detonate.cpp
- src/Misc/Hooks.Crates.cpp
- src/Misc/Hooks.UI.cpp
- src/Misc/Hooks.VeinholeMonster.cpp
- src/Utilities/Stream.cpp
- src/Utilities/Stream.h
```

Untracked files include PhobosFog phase reports, `devDocs/`, local patch/status artifacts, and local `AGENTS.md`.

Final RC1 adds these documentation files:

- `PhobosFog_Final_RC1_Handoff.md`
- `PhobosFog_Final_RC1_VerificationSummary.md`
- `PhobosFog_Final_RC1_CommitCandidateChecklist.md`

`AGENTS.md` remains untracked and was not modified by this stage.

## 4. Diff Size Snapshot

Command:

```bat
git diff --stat
```

Result at start of RC1 pass:

```text
24 tracked files changed, 9329 insertions(+), 30 deletions(-)
```

Largest tracked source deltas:

- `src/Misc/Hooks.VeinholeMonster.cpp`: explored overlay, cache, perf path.
- `src/Ext/ParticleType/Hooks.cpp`: particle presentation gating and probes.
- `src/Ext/House/Body.cpp`: PhobosFog state, versions, save/load payload, refresh helpers.
- `src/Ext/Techno/Hooks.Pips.cpp`: object/building/fogged presentation gates.

This does not include many untracked development reports.

## 5. Diff Name Snapshot

Command:

```bat
git diff --name-only
```

Result at start of RC1 pass:

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

## 6. Required Command Validation

Final command results are recorded after this file is created.

```text
git diff --check:
Passed. Git reported existing LF-to-CRLF normalization warnings for tracked modified files, but no whitespace errors.

scripts\build_debug.bat:
Passed. `Debug\Phobos.dll` was produced by `Phobos.vcxproj`.

git diff --cached --name-only:
Passed. Output was empty.
```

## 7. Manual Verification Coverage

Manual RC coverage should include:

| Area | Required check | Result | Notes |
| --- | --- | --- | --- |
| Disabled baseline | `PhobosFog.Enabled=false` starts main menu and skirmish with no behavior changes. | NT |  |
| Overlay visual | Explored overlay covers flat and height-discontinuity terrain without missing viewport blocks. | NT |  |
| Scroll/move/scout | Unit select, move, scroll, and scout do not stall or cause catch-up movement. | NT |  |
| SpySat active | SpySat active state provides intended hard visibility. | NT |  |
| SpySat deactivate hold | Hold uses explicit `DeactivateHoldFrames` and logs temporal cache behavior when perf is on. | NT |  |
| Building snapshot | Explored building snapshot semantics remain intact while live-only presentation is hard-visible gated. | NT |  |
| Command gating | Hidden object command and force-fire cell gates behave as documented. | NT |  |
| Combat gating | Auto target and fire-time gates reject non-hard-visible enemy targets. | NT |  |
| Radar | Radar object hiding and radar fog colors match visible/explored/unknown state. | NT |  |
| Perf counters | `[PhobosFog][Perf]` works independently from debug logs. | NT |  |
| Save/load | Explored memory persists, exact `Visible` and temporary holds do not. | NT | SaveLoad-C record not found in current tree. |

## 8. Recommended RC INI Presets

Disabled baseline:

```ini
[General]
PhobosFog.Enabled=false
```

Cacheable overlay:

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.Debug=false
PhobosFog.Perf.Enabled=true
PhobosFog.Perf.IntervalFrames=300
PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlaySoftEdge=false
PhobosFog.ExploredOverlayFadeInFrames=0
PhobosFog.ExploredOverlayAlphaVariance=0
PhobosFog.ExploredOverlayCliffCover=false
```

Full feature sanity:

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.Debug=false
PhobosFog.Perf.Enabled=true
PhobosFog.Perf.IntervalFrames=300
PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlaySoftEdge=false
PhobosFog.ExploredOverlayFadeInFrames=0
PhobosFog.HideEnemyFoot=true
PhobosFog.HideBuildings=true
PhobosFog.HideTiberiumSpawners=true
PhobosFog.HideWorldAnim=true
PhobosFog.HideWorldParticles=true
PhobosFog.HideHoverCursor=true
PhobosFog.HideHoverTooltip=true
PhobosFog.HideHoverHealthBar=true
PhobosFog.GateHiddenObjectCommands=true
PhobosFog.GateAutoTargets=true
PhobosFog.GateAutoFire=true
PhobosFog.GateForceFireCells=true
PhobosFog.HideRadarObjects=true
PhobosFog.OverrideRadarFog=true
PhobosFog.SyncSpySatellite=true
PhobosFog.SpySatellite.MarkExplored=true
PhobosFog.SpySatellite.PersistentVisible=true
PhobosFog.SpySatellite.DeactivateHoldFrames=300
PhobosFog.SyncFullMapReveal=true
PhobosFog.FullMapReveal.MarkExplored=true
PhobosFog.FullMapReveal.VisibleHoldFrames=300
PhobosFog.SpyPlaneReveal.VisibleHoldFrames=300
PhobosFog.WarheadReveal.VisibleHoldFrames=30
```

Save/load sanity:

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlaySoftEdge=false
PhobosFog.ExploredOverlayFadeInFrames=0
```

Expected save/load result:

- explored memory persists;
- exact `Visible` does not persist;
- temporary visible holds do not persist;
- overlay cache does not persist;
- next refresh after load is forced once.

## 9. Current Verification Verdict

Static command validation passed for this RC1 documentation pass.

Manual gameplay verification remains required before treating this as a merge-ready RC.
