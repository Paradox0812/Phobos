# PhobosFog SaveLoad-B2B Explored-Only PFOG Payload

This document records the SaveLoad-B2B implementation stage for the PhobosFog development branch. It is a development report, not final user documentation.

## 1. Goal

Add an optional HouseExt tail payload that persists only whether each PhobosFog cell has ever been explored.

The payload intentionally does not persist current `Visible` timing, temporary visibility holds, overlay cache data, perf counters, or debug state.

## 2. Modified Files

Production files:

- `src/Ext/House/Body.h`
- `src/Ext/House/Body.cpp`
- `src/Ext/Scenario/Body.cpp`

Documentation files:

- `devDocs/PhobosFog_Semantic_Debt.md`
- `PhobosFog_P9_Z_PerformanceBaselineFreeze.md`
- `PhobosFog_SaveLoad_B2B_ExploredOnlyPayload.md`

No hooks, INI tags, overlay rendering path, cache key, row-bucket path, radar path, command gate, or combat gate were added or changed.

## 3. Payload Format

The optional payload is appended after the existing `HouseExt::ExtData::Serialize(...)` stream data:

```text
magic = PFOG
version = 1
cellCount
byteCount
explored bitset bytes
```

The current magic value is the little-endian dword for `PFOG`.

Save semantics:

- `Unknown` -> bit `0`
- `Explored` -> bit `1`
- `Visible` -> bit `1`

Load semantics:

- bit `0` -> `PhobosFogCellState::Unknown`
- bit `1` -> `PhobosFogCellState::Explored`

`Visible` is deliberately degraded to persisted explored memory on save/load.

## 4. Compatibility Behavior

Old saves without a `PFOG` tail remain supported:

- if no tail bytes remain, PhobosFog initializes to all `Unknown`;
- if at least four bytes remain but the magic is not `PFOG`, the reader state is not consumed and normal block validation handles the unexpected tail.

Invalid `PFOG` payloads fail safe:

- if `cellCount != MapClass::MaxCells`, the payload bytes are skipped and the PhobosFog vector is restored to all `Unknown`;
- if the block does not contain enough bytes for the claimed payload, loading returns failure instead of reading out of bounds.

## 5. Runtime State Reset On Load

After load, only explored memory is restored. Runtime-only fog state is reset:

- `PhobosFog_LastVisibleFrames` is resized to map cell count and filled with `0`;
- `PhobosFog_FullMapVisibleUntilFrame` is cleared;
- overlay-effective batch state is aborted and scratch stamps are cleared;
- debug refresh stats are reset;
- perf/debug touch counters are reset before the post-load version touch.

Spy satellite edge state is initialized from the loaded house:

- `PhobosFog_LastSpySatActive = pHouse->SpySatActive`;
- `PhobosFog_LastFullMapHardVisible` is initialized from current effective full-map visibility.

Post-load dirty markers:

- `TouchPhobosFogStateVersion(Reset)`;
- `TouchPhobosFogOverlayEffectiveVersion()`;
- request one forced PhobosFog refresh so `PhobosFog.UpdateInterval` does not delay the next refresh pass.

## 6. Refresh Integration

`ScenarioExt` now consumes a global PhobosFog force-refresh flag in `RefreshPhobosFogState()`.

When the flag is set, the refresh pass bypasses only the `PhobosFog.UpdateInterval` throttle for that frame. Normal feature gating still applies:

- if `PhobosFog.Enabled=false`, no refresh runs;
- no new hook was added;
- existing `LogicClass_Update_BeforeAll` refresh placement remains unchanged.

## 7. Explicitly Not Saved

This stage does not save:

- `PhobosFog_LastVisibleFrames`;
- `PhobosFog_FullMapVisibleUntilFrame`;
- overlay final-region cache;
- row buckets;
- geometry template cache;
- perf/debug counters;
- `PhobosFog_StateVersion`;
- `PhobosFog_OverlayEffectiveVersion`.

## 8. Validation

Commands run:

- `git diff --check`: passed before documentation updates, with only existing LF-to-CRLF normalization warnings.
- `scripts\build_debug.bat`: passed. `Debug\Phobos.dll` and `Debug\Phobos.pdb` were produced.

Final validation commands are rerun at the end of the task summary.

## 9. Manual Test Plan

Recommended manual validation:

1. Start with `PhobosFog.Enabled=true` and normal visual settings.
2. Enter skirmish, scout several areas, then save.
3. Load that save and confirm previously explored cells return as explored, not hard visible.
4. Confirm active sight refreshes immediately after load rather than waiting for `PhobosFog.UpdateInterval`.
5. Confirm temporary reveal effects, SpySat deactivate holds, and visible holds are not restored as hard `Visible`.
6. Confirm `PhobosFog.Enabled=false` still behaves like the existing disabled baseline.

## 10. Stage Verdict

SaveLoad-B2B adds the first PhobosFog persistence contract: explored-only memory.

The implementation intentionally keeps all transient visibility, overlay cache, and diagnostics runtime-only.
