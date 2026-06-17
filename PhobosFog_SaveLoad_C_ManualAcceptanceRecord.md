# PhobosFog SaveLoad-C Manual Acceptance Record

This document records the current manual acceptance result for PhobosFog save/load behavior at the RC1 handoff stage.

It is a development acceptance record, not final upstream user documentation.

## 1. Scope

SaveLoad-C validates the accepted save/load baseline:

- PhobosFog stores explored memory through the optional HouseExt `PFOG` payload.
- PhobosFog does not persist exact hard `Visible` state.
- PhobosFog does not persist temporary visible windows, full-map holds, or overlay/cache/perf runtime state.
- Runtime behavior after load remains controlled by the save-bound rules state.

This record does not introduce new code, hooks, INI tags, or semantics.

## 2. Accepted Baseline

Current accepted save/load baseline:

- `Unknown` persists as not explored.
- `Explored` persists as explored.
- `Visible` persists as explored memory, not as hard visible.
- `PhobosFog_LastVisibleFrames` is reset on load.
- `PhobosFog_FullMapVisibleUntilFrame` is reset on load.
- Overlay final-region cache, row buckets, geometry templates, debug counters, and perf counters are runtime-only.
- Load touches PhobosFog raw and overlay-effective versions and requests one forced PhobosFog refresh.

## 3. Manual Acceptance Matrix

| Case | Expected result | Recorded result |
| --- | --- | --- |
| Save created with `PhobosFog.Enabled=false` | Loading the save keeps disabled behavior. No PhobosFog overlay, gating, radar override, reveal sync, or combat/command gating should become active from restored explored payload alone. | PASS |
| Save created with `PhobosFog.Enabled=true` | Loading the save keeps enabled behavior. Restored explored memory is available to PhobosFog systems after load. | PASS |
| INI edited before loading an existing save | Changing INI before load does not retroactively change the rule state stored in that save. The loaded game uses the save-bound rules state. | PASS |
| Crash check | Save/load flow does not produce an observed crash in the accepted test path. | PASS |
| Explored-only persistence | Previously explored memory persists. Exact hard `Visible` state does not persist as hard visible. | PASS |
| Temporary state persistence boundary | Temporary visible cells, full-map visible holds, and SpySat deactivation holds do not cross save/load as active hard-visible runtime windows. | PASS |
| Overlay/cache persistence boundary | Overlay cache, row buckets, geometry template cache, perf counters, and debug counters are not persisted. | PASS |

## 4. INI Notes

Disabled-save baseline:

```ini
[General]
PhobosFog.Enabled=false
```

Enabled-save baseline:

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlaySoftEdge=false
PhobosFog.ExploredOverlayFadeInFrames=0
```

The important acceptance point is save-bound behavior: changing the INI before load should not rewrite the rules state that was serialized in the save.

## 5. Explicit Non-Persistence

The following are intentionally not persisted:

- current hard `Visible` cells;
- `PhobosFog_LastVisibleFrames`;
- `PhobosFog_FullMapVisibleUntilFrame`;
- SpySat deactivation temporary hold;
- overlay final-region cache;
- row buckets;
- geometry template cache;
- perf counters;
- debug counters;
- raw or overlay-effective version counters.

## 6. Verdict

SaveLoad-C is accepted for RC1 as explored-only persistence.

The save/load contract is intentionally conservative: it preserves map memory while discarding transient visibility and draw/cache state.
