# PhobosFog Final RC1 Handoff

This document summarizes the current PhobosFog commit-candidate state for final review. It is a handoff document, not upstream user documentation.

## 1. Scope

Final RC1 is documentation-only.

This stage does not:

- modify production `.cpp` or `.h` files;
- add hooks;
- add INI tags;
- change visual behavior;
- change overlay cache behavior;
- change save/load behavior;
- change command or combat gating.

## 2. Current Feature Completion Scope

The current PhobosFog workstream contains these implemented areas:

- House-level fog state with `Unknown`, `Explored`, and `Visible`.
- Sight refresh from eligible owned technos and buildings.
- Degrade from `Visible` to `Explored` when sight is lost.
- Explored overlay presentation.
- Enemy foot, live building, terrain-like, animation, particle, radar, hover UI, command, and combat gates.
- Reveal-source synchronization for selected vanilla reveal paths.
- SpyPlane, SpySat, full-map reveal, and warhead reveal hold integration.
- Overlay final-region cache and performance logging.
- Explored-only save/load persistence through optional HouseExt `PFOG` payload.

All user-facing PhobosFog behavior remains gated by `PhobosFog.Enabled`.

## 3. Default-Off Semantics

Default configuration:

```ini
[General]
PhobosFog.Enabled=false
```

Expected default-off behavior:

- no PhobosFog visual overlay;
- no object draw hiding;
- no radar fog replacement;
- no hover, command, or combat gating;
- no reveal-source synchronization effects;
- no PhobosFog debug or perf output unless explicitly enabled and reached by a feature path.

The disabled behavior is expected to remain equivalent to the current upstream Phobos baseline for gameplay and presentation.

## 4. Visibility Semantics

### 4.1 Raw Cell State

Raw cell state is stored per house in `HouseExt::ExtData::PhobosFog_CellStates`.

Meanings:

- `Unknown`: no memory of the cell.
- `Explored`: the cell has been discovered before, but is not currently hard visible.
- `Visible`: the cell is currently hard visible or temporarily hard visible through a supported source.

Raw state is house-local. It is not automatically viewer-plus-allies merged.

### 4.2 Effective State

One-house effective state is raw cell state plus house-level full-map hard-visible overrides:

- SpySat persistent visibility;
- full-map temporary visible hold.

This does not rewrite the entire raw cell vector to `Visible`.

### 4.3 Viewer Or Allies Merged State

Viewer-facing presentation merges current viewer plus valid allied houses:

- any `Visible` wins;
- otherwise any `Explored` means the merged state is `Explored`;
- otherwise the merged state is `Unknown`.

### 4.4 Known And HardVisible

In RC1 documents:

- `Known` means not `Unknown`, usually `Explored` or `Visible` after viewer/allies merge.
- `HardVisible` means effective `Visible`.

Presentation gates that must hide live information use `HardVisible`.

Explored building snapshot semantics may use `Known` where intentionally accepted.

## 5. Explored Overlay Final Pipeline

The accepted P9 overlay pipeline is:

```text
source cells
-> geometry template rows
-> viewport-preclipped clipped spans
-> row buckets
-> per-row union
-> compact main union
-> height-discontinuity face spans
-> fallback down dilation
-> final close-gap union
-> cached final region union spans when allowed
-> strict vertical draw-rect batching
```

Important notes:

- The active main path no longer materializes a legacy global `mainSpans` vector.
- `RawMainSpans` perf output now means accepted clipped template rows / row-bucket input rows.
- Legacy cliff-cover collection and old `cliffSpans` are bypassed in the P9 baseline.
- Height-discontinuity coverage is the accepted terrain height-face path.

## 6. Overlay Cache

The final-region cache is a single-entry cache.

Cache key:

```text
viewer identity
+ OverlayEffectiveVisibilityVersionHash
+ OverlayViewportHash
+ OverlayConfigHash
```

The cache key intentionally excludes:

- raw `PhobosFog_StateVersion`;
- `RawEffectiveVisibilityVersionHash`;
- current frame;
- remaining temporary hold frame counts;
- overlay cache contents;
- row buckets or geometry template cache.

### 6.1 TemporalCacheFirstInvalidFrame

Full-map temporal visible holds use `TemporalCacheFirstInvalidFrame`.

This allows cache hits during a full-map hold and forces rebuild at the first invalid frame. The cache decision does not scan the full cell-level `PhobosFog_LastVisibleFrames` array.

### 6.2 Cache Disable Cases

Known final-region cache disable cases:

- `PhobosFog.ExploredOverlaySoftEdge=true` with active visible soft edge;
- `PhobosFog.ExploredOverlayFadeInFrames > 0`;
- invalid viewer or stage state;
- draw failure;
- max draw-rect budget hit.

Cache disabled does not mean overlay disabled. The overlay still rebuilds and draws.

## 7. Radar, UI, Command, And Combat Gating

### 7.1 Radar

Implemented radar scope:

- radar object dot hiding for non-hard-visible enemies;
- radar background fog replacement for `Visible`, `Explored`, and `Unknown`;
- budgeted radar cell refresh for full-map visibility edge cases.

Radar behavior remains gated by PhobosFog tags and does not change when the feature is disabled.

### 7.2 UI Presentation

Implemented UI presentation gating:

- hover cursor;
- object tooltip/name;
- hover or permanent health bar/pip presentation where hooked.

These use current player plus valid allied hard visibility.

### 7.3 Command Gating

Implemented command gating:

- hidden object command target downgrade;
- force-fire cell and terrain-backed cell fire-time gate.

Command gating is opt-in and separate from render-only presentation.

### 7.4 Combat Gating

Implemented combat gating:

- auto-target acquisition rejects non-hard-visible enemy live objects;
- fire-time object target gate clears non-hard-visible enemy live object targets and returns cannot-fire.

Combat gates use attacker owner plus valid allied visibility, not `HouseClass::CurrentPlayer`.

Already-fired projectiles are not retroactively changed.

## 8. Reveal And Full-Map Synchronization

Implemented synchronization scope:

- Warhead `Reveal=` sync;
- per-warhead `PhobosFog.Warhead.RevealVisibleHoldFrames`;
- SpyPlane reveal sync;
- SpySat active sync;
- SpySat deactivate hold;
- full-map reveal sync;
- reveal-map crate sync.

Vanilla reveal behavior is not replaced. PhobosFog mirrors selected reveal sources into explored state or temporary hard-visible windows.

`PhobosFog.SpySatellite.DeactivateHoldFrames` is explicit-only. It does not inherit from `PhobosFog.RevealSources.VisibleHoldFrames`.

## 9. Save/Load Semantics

RC1 save/load scope is explored-only persistence.

The optional HouseExt `PFOG` payload stores:

```text
magic = PFOG
version = 1
cellCount
byteCount
explored bitset bytes
```

Save semantics:

- `Unknown` -> `0`;
- `Explored` -> `1`;
- `Visible` -> `1`.

Load semantics:

- bit `0` -> `Unknown`;
- bit `1` -> `Explored`.

### 9.1 Save-Bound PhobosFog.Enabled Behavior

The optional `PFOG` payload may be present in save data independently from the runtime value of `PhobosFog.Enabled`.

Runtime effects remain controlled by `PhobosFog.Enabled`:

- if disabled, restored explored memory does not activate PhobosFog rendering or gating;
- if enabled, restored explored memory can be used by the PhobosFog systems.

### 9.2 Not Persisted

RC1 does not persist:

- exact `Visible` state;
- `PhobosFog_LastVisibleFrames`;
- `PhobosFog_FullMapVisibleUntilFrame`;
- SpySat temporary deactivation hold;
- overlay final-region cache;
- row buckets;
- geometry template cache;
- perf/debug counters;
- `PhobosFog_StateVersion`;
- `PhobosFog_OverlayEffectiveVersion`.

Loaded saves touch raw and overlay-effective versions and request one forced fog refresh so `PhobosFog.UpdateInterval` does not delay the next refresh pass.

## 10. Known Limitations

Required RC1 known limitations:

- SoftEdge and FadeIn disable overlay final-region cache reuse.
- Save/load stores explored memory only, not exact `Visible` or temporary-visible state.
- `PhobosFog.SpySatellite.DeactivateHoldFrames` is explicit-only.
- P9 baseline has no additional edge pass and no extra soft-edge pass beyond the documented soft-edge behavior.
- Overlay cache, row buckets, geometry template cache, and perf/debug counters are runtime-only.

Additional known limitations:

- Particle ownership remains a conservative mixed model and should only be expanded under a focused leak task.
- Force-fire cell gating is fire-time only; no separate command-stage force-fire cell hook is part of RC1.
- Building semantics intentionally distinguish live-building presentation from explored/fogged building snapshots.
- Legacy cliff-cover probe/collector history remains in development reports but is not the active baseline.
- `PhobosFog_SaveLoad_C_ManualAcceptanceChecklist.md` was not found during this RC1 documentation pass, so manual save/load acceptance still needs a dedicated record.
- The standalone `PhobosFog_SaveLoad_B2B_ExploredOnlyPayload.md` report is present again and was reviewed during this RC1 pass.

## 11. Optional Future Work

Recommended follow-up work:

- Manual SaveLoad-C validation and a persistent acceptance record.
- User-facing documentation cleanup after RC validation.
- Optional soft-edge/fade cache design if performance demands it.
- Optional particle ownership redesign only if a concrete visual leak reappears.
- Optional terminology cleanup around `CliffCover`, `HeightFace`, and overlay perf fields.
- Optional consolidation of duplicated hard-visible helper patterns after stability is proven.

## 12. Commit Candidate Notes

The current working tree is a broad PhobosFog feature candidate with many source files and development reports.

Before commit:

- review staged files carefully;
- avoid staging local runtime logs, game files, or temporary patch/status artifacts;
- decide whether root-level phase reports should remain in the commit or be moved/trimmed in a documentation cleanup phase;
- keep `AGENTS.md` local-only unless the user explicitly decides otherwise.
