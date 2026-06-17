# PhobosFog Semantic Debt Registry

This document records semantic debt in the current PhobosFog development branch. It is a development-only work document, not final upstream user documentation.

Maintenance rule:

- Update this document whenever PhobosFog semantics are clarified, renamed, split, merged, optimized, or intentionally left ambiguous.
- Keep it aligned with `PhobosFog_INI_Tags.md`, `PhobosFog_CurrentBaseline_And_Feature_Summary.md`, and `devDocs/PhobosFog_Algorithm_Inventory_For_Performance.md`.
- Treat this as a triage and cleanup list. Do not fix multiple semantic debts in one phase unless the user explicitly approves that scope.
- Distinguish current accepted baseline from failed experiments and dormant prototype paths.

## 1. Current Accepted Semantic Baseline

PhobosFog currently combines several layers that were built incrementally:

- Runtime fog state: per-house `Unknown`, `Explored`, and `Visible`.
- Effective visibility: one house's raw state plus SpySat persistent visibility and full-map visible holds.
- Viewer visibility: current viewer plus valid allied houses.
- World presentation gating: render-only hiding for objects, building live presentation, building animations, particles, terrain-like visuals, radar dots, hover UI, cursor presentation, and command presentation.
- Combat gating: opt-in command and fire checks that use attacker-owner plus valid allied hard visibility, not `HouseClass::CurrentPlayer` UI visibility.
- Explored overlay rendering: a screen-space region-mask pipeline with direct template-to-row-bucket main fill, compacted union, fallback dilation, height-discontinuity face coverage, optional soft-edge pass, and a single-entry final-region cache.
- Reveal synchronization: selected vanilla reveal events mirrored into PhobosFog explored state or temporary hard-visible windows.
- Diagnostics: debug summaries, region-mask debug logs, radar logs, particle probes, hook probes, and independent perf logs.

The accepted user-facing direction is:

- `Visible` means live, actionable, fully presented information.
- `Explored` means remembered information or dimmed terrain presentation where appropriate.
- `Unknown` means hidden.
- Buildings may remain visible in `Explored` as last-known static information.
- Building live-only presentation, building animations, turrets or turret-like active visuals, hover UI, command UI, health bars, and combat gates should still require hard `Visible`.
- Existing vanilla reveal, targeting, commands, radar, and drawing should remain unchanged when the relevant PhobosFog switch is disabled.

## 2. Stable Semantic Layers

### 2.1 Raw Cell State

Source anchors:

- `HouseExt::ExtData::PhobosFog_CellStates`
- `HouseExt::ExtData::PhobosFog_LastVisibleFrames`
- `HouseExt::ExtData::MarkPhobosFog*`
- `HouseExt::ExtData::DegradePhobosFogVisibility(...)`

Meaning:

- Raw cell state is house-local stored fog state.
- `LastVisibleFrames` is a per-cell expiry/timing vector used for visibility degrade and overlay fade timing.
- Raw state is not viewer-plus-allies merged.
- Raw state is not equivalent to final presentation visibility.

Debt:

- Some names and older reports still use "visible state" without saying whether they mean raw state, one-house effective state, or viewer-plus-allies merged state.

Cleanup rule:

- New code and reports should say `raw cell state` when reading `PhobosFog_CellStates` directly.

### 2.2 One-House Effective State

Source anchors:

- `HouseExt::ExtData::GetEffectivePhobosFogCellState(...)`
- `HouseExt::ExtData::IsPhobosFogFullMapHardVisible()`
- `HouseExt::ExtData::PhobosFog_FullMapVisibleUntilFrame`
- `HouseClass::SpySatActive`

Meaning:

- Effective state is a single house's raw state plus full-map hard-visible overrides.
- SpySat persistent visibility is effective state, not a full-vector rewrite.
- Full-map visible hold is effective state, not a full-vector rewrite.

Debt:

- SpySat and full-map reveal can both make every cell effectively `Visible`, but they have different source semantics and different INI gates.
- `PhobosFog.SpySatellite.DeactivateHoldFrames` is explicit-only and does not inherit from `PhobosFog.RevealSources.VisibleHoldFrames`.

Cleanup rule:

- When discussing "all map visible", specify whether it came from SpySat persistent visible, full-map reveal hold, or raw per-cell visible writes.

### 2.3 Viewer Or Allies Merged State

Source anchors:

- `HouseExt::ExtData::TryGetEffectivePhobosFogCellStateForViewerOrAllies(...)`
- `HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies(...)`

Meaning:

- Presentation visibility merges the viewer and valid allied houses.
- Any hard `Visible` result wins.
- If no house is `Visible` but at least one is `Explored`, the merged state is `Explored`.
- Otherwise the merged state is `Unknown`.

Debt:

- This helper is hot and broad. It is used by world render gates, UI gates, radar, overlay classification, and some combat gates.
- Future optimizations can easily break ally semantics if they cache only the viewer's raw state.

Cleanup rule:

- Any viewer-facing presentation gate must use viewer-plus-allies semantics unless the task explicitly states otherwise.
- Any attacker/command/combat gate must use attacker-owner plus valid allied semantics, not `CurrentPlayer`.

### 2.4 Raw Version vs Overlay Effective Version

Source anchors:

- `HouseExt::ExtData::PhobosFog_StateVersion`
- `HouseExt::ExtData::PhobosFog_OverlayEffectiveVersion`
- `HouseExt::ExtData::BeginPhobosFogOverlayEffectiveBatch()`
- `HouseExt::ExtData::TrackPhobosFogOverlayOriginalCell(...)`
- `HouseExt::ExtData::EndPhobosFogOverlayEffectiveBatch()`
- `BuildEffectiveVisibilityVersionHash(...)`
- `BuildOverlayEffectiveVisibilityVersionHash(...)`

Meaning:

- `PhobosFog_StateVersion` is a raw mutation and diagnostic version.
- `PhobosFog_OverlayEffectiveVersion` is a house-local effective overlay dirty key.
- Overlay effective batching compares original and final house-local effective state and touches the overlay-effective version only when the final effective result changed.
- The overlay cache key uses viewer identity plus viewer/allies `OverlayEffectiveVisibilityVersionHash`, viewport signature, and overlay config hash.
- The overlay cache key intentionally does not use raw `PhobosFog_StateVersion`.

Debt:

- Reports and perf fields still expose both raw and overlay-effective hashes. This is useful, but the distinction is easy to forget.
- Overlay-effective version is house-local. It is not already the viewer-plus-allies merged result; the merge happens when building the hash.

Cleanup rule:

- Do not use raw `PhobosFog_StateVersion` as a future overlay cache dirty key.
- Do not treat `PhobosFog_OverlayEffectiveVersion` as a gameplay or save/load state.

## 3. User-Facing Feature Semantics

### 3.1 Master Gate And Feature Gates

Source anchors:

- `RulesExt::ExtData::PhobosFog_Enabled`
- `RulesExt::ExtData::PhobosFog_*` feature switches

Meaning:

- `PhobosFog.Enabled=false` must preserve upstream-equivalent behavior.
- Feature switches are opt-in and should not have side effects unless the master gate is enabled.
- `PhobosFog.Debug` gates correctness/debug logs.
- `PhobosFog.Perf.Enabled` gates low-frequency perf logs independently from debug logs.

Debt:

- Some diagnostics are tied to draw path reachability. A log being absent can mean the feature path did not run, not only that its own gate is off.
- Probe logs are not a stable user-facing behavior.

Cleanup rule:

- Keep docs explicit about which switches require `PhobosFog.Enabled`, which require a feature path, and which are debug/perf/probe-gated.

### 3.2 Building Snapshot vs Live Building Presentation

Source anchors:

- `TacticalClass_RenderLayers_DrawBefore` BuildingClass path in `src/Ext/Techno/Hooks.Pips.cpp`
- `FoggedObjectClass_Draw_PhobosFogHideBuildings`
- Building-attached animation handling in `src/Ext/Anim/Hooks.cpp`

Meaning:

- `PhobosFog.HideBuildings` gates live `BuildingClass` rendering by foundation hard visibility.
- Explored/fogged building snapshots may remain visible as last-known static information.
- Building animations and active/live sub-visuals should require hard `Visible`.

Debt:

- The tag name `HideBuildings` sounds like all building visuals vanish outside hard `Visible`, but the accepted semantic is closer to "gate live building presentation while preserving explored snapshots".
- Earlier experiments and reports used "hide buildings" to mean different things.

Cleanup rule:

- Do not reinterpret `HideBuildings` as total building erasure without an explicit new task.
- If this becomes user-facing documentation, describe it as live-building presentation gating.

### 3.3 World Animations And Particles

Source anchors:

- `PhobosFogWorldAnim::ShouldHide(...)`
- `PhobosFogParticle::AnalyzeParticle(...)`
- `PhobosFogParticle::AnalyzeParticleSystem(...)`

Meaning:

- Building-attached anims follow building visibility/foundation semantics.
- Non-building map-space anims prefer owner techno visibility, then fallback to map-space cell visibility.
- Particle and particle-system hiding tries owner metadata first and falls back to map-space/source-cell checks.
- Particle-system hidden hold and visible release are render-only stabilizers.

Debt:

- Particle ownership remains less clean than object/building visibility.
- Some particle families are object-owned, some are system-owned, and some are effectively map-space.
- Several smoke-leak probes and fallback attempts are historical context, not a final clean design.

Cleanup rule:

- Do not broaden particle metadata or owner scanning without a dedicated perf and correctness task.
- If particle work resumes, classify each particle family as cell-owned, object-owned, or system-owned before changing gates.

### 3.4 UI, Command, And Combat Gating

Source anchors:

- `PhobosFogTooltip` in `src/Ext/TechnoType/Hooks.cpp`
- `PhobosFogAutoTarget` in `src/Ext/Techno/Hooks.Misc.cpp`
- `PhobosFogAutoFire` and `PhobosFogForceFire` in `src/Ext/Techno/Hooks.Firing.cpp`

Meaning:

- Hover cursor, tooltip/name, and health bar are presentation gates using current player plus valid allies.
- Hidden object click command gating downgrades hidden object clicks toward cell-command behavior.
- Auto-target gating rejects non-hard-visible enemy live object candidates.
- Fire-time object gating clears hidden enemy live object targets and returns cannot-fire.
- Force-fire cell gating rejects direct `CellClass*` and terrain-backed cell targets if the target cell is not hard visible.

Debt:

- Command/presentation gates and combat gates are intentionally different semantic layers, but their names all say "gate".
- `GateAutoFire` and `GateForceFireCells` are closer to simulation control than render-only hiding.
- Current force-fire gate has no command-stage hook; it is fire-time only.

Cleanup rule:

- Keep P6 behavior frozen unless a dedicated regression task is opened.
- Do not use `CurrentPlayer` in attacker-owner combat gates.
- Do not add projectile-after-launch behavior under these tags without a new design phase.

### 3.5 Radar And Minimap

Source anchors:

- Radar object dot gates and radar background override paths in existing hooks.
- `QueuePhobosFogRadarCellRefresh(...)`
- `ScheduleAllPhobosFogRadarCells()`
- `QueuePhobosFogFullMapRadarRefreshOnEdge(...)`

Meaning:

- Radar object dots are presentation-gated by hard visibility.
- Radar background override maps PhobosFog state to visible/explored/unknown colors.
- Full-map hard-visible edges can schedule radar refresh in a budgeted way.

Debt:

- Radar state and overlay state both use effective visibility but have different invalidation and refresh pipelines.
- Radar refresh may still need broader profiling after overlay cleanup finishes.

Cleanup rule:

- Do not fold radar cache or radar dirty logic into overlay cache.

### 3.6 Reveal Synchronization

Source anchors:

- Warhead reveal sync in `src/Ext/WarheadType/Detonate.cpp`
- Spy plane reveal sync in `src/Ext/Aircraft/Hooks.cpp`
- SpySat edge sync in `src/Ext/Scenario/Body.cpp`
- Full-map crate sync in `src/Misc/Hooks.Crates.cpp`

Meaning:

- Vanilla reveal remains intact.
- PhobosFog mirrors selected reveal sources into explored state or temporary hard-visible windows.
- Warhead reveal hold priority is per-warhead, WarheadReveal, optional FullMapReveal for full-map sources, then global fallback.
- Spy plane reveal can inherit from the global fallback.
- SpySat deactivate hold is explicit-only through `PhobosFog.SpySatellite.DeactivateHoldFrames`.

Debt:

- "Reveal" can mean vanilla reveal, PhobosFog explored marking, temporary per-cell hard visibility, full-map effective visibility override, or SpySat persistent visibility.
- SpySat deactivate hold not inheriting global fallback is intentional in current code but easy to misconfigure.

Cleanup rule:

- When adding reveal behavior, specify source, storage model, duration, and whether it is raw cell state or effective override.

## 4. Explored Overlay Semantics

### 4.1 Active Region-Mask Pipeline

Source anchor:

- `PhobosFogExploredOverlay::Draw()` in `src/Misc/Hooks.VeinholeMonster.cpp`

Current active main path:

1. Compute padded viewport cell range.
2. Classify explored overlay source cells from viewer-plus-allies state.
3. Project each source cell to screen space.
4. Select rect or diamond shape through `PhobosFog.ExploredOverlayShape` and frontier classification.
5. Use geometry templates for rect/diamond row spans.
6. Instantiate clipped template rows directly into row buckets.
7. Run per-row alpha-max union to produce `mainUnionSpans`.
8. Compact `mainUnionSpans` for final-region input and fallback dilation input.
9. Generate height-discontinuity face spans for screen-facing offsets `(1,0)` and `(0,1)`.
10. Generate fallback screen-down dilation spans from compact main union.
11. Merge `compactMainUnionSpans + verticalFaceSpans + fallbackDilationSpans`.
12. Draw final union spans through strict vertical rect batching.
13. Optionally reuse the single-entry final-region cache when allowed.

Debt:

- Older docs and code still contain "mainSpans" vocabulary, but the active main path now skips the `mainSpans` intermediate vector.
- `RawMainSpans` in perf output now means accepted clipped template rows / row-bucket input spans, not a materialized global vector.

Cleanup rule:

- Use `source cells`, `row-bucket input spans`, `main union spans`, and `final region union spans` for new reports.

### 4.2 Legacy Cliff Cover vs Height-Discontinuity Face Coverage

Source anchors:

- Dormant `CollectCliffCoverEdges(...)`
- Active `TryAddHeightDiscontinuityFaceSpans(...)`
- `UseLegacyCliffCoverInRegionMaskPrototype=false`
- `UseHeightDiscontinuityFacePrototype=true`

Meaning:

- The old cliff-cover probe/edge collector is dormant and bypassed.
- The active face coverage path is region-based height-discontinuity face coverage, not the legacy cliff-cover experiment.
- `PhobosFog.ExploredOverlayCliffCover` still requests the broad concept of cliff/height cover, but the actual accepted drawing model is the C2 region height-face model plus later geometry tuning.

Debt:

- The names `CliffCover`, `CliffProbe`, `HeightFace`, and `VerticalFace` are used across multiple experimental stages.
- Failed R0/R1/R2/R3 experiments remain useful as forensic history but should not be mistaken for the current baseline.
- Some local prototype constants still encode accepted behavior without user-facing tag names.

Cleanup rule:

- Do not reactivate `CollectCliffCoverEdges` or `cliffSpans` during final cleanup.
- If the old code is removed later, do it as a dedicated cleanup task with hook/build verification.
- If user docs mention cliff cover, describe the current behavior as height-discontinuity face coverage, not the failed legacy edge collector.

### 4.3 Soft Edge, Unknown Merge, Fade, And Cache

Meaning:

- Soft edge is a separate visual transition pass.
- Unknown merge is a boundary smoothing/merge pass, separate from main region union.
- Fade-in makes alpha time-dependent.
- The final-region cache is currently disabled when soft edge is enabled or fade-in is active.
- Full-map temporal visible holds do not disable cache outright; they store a first-invalid frame.

Debt:

- `PhobosFog.ExploredOverlaySoftEdgeAlpha` is legacy; current soft-edge semantics prefer visible/unknown alpha split tags.
- Cache eligibility can be misunderstood as visual eligibility. Cache disabled does not mean overlay disabled.

Cleanup rule:

- Keep cache and visual semantics separate in reports.
- Do not put `CurrentFrame` or remaining hold frames directly into the cache key.
- Do not scan `PhobosFog_LastVisibleFrames` in the overlay cache decision hot path.

### 4.4 Overlay Cache Semantics

Source anchors:

- `OverlayRegionCacheKey`
- `OverlayRegionCacheDecision`
- `OverlayFinalRegionCache`
- `AccumulateTemporalVisibilityWindowForViewerAndAllies(...)`

Meaning:

- Cache key is viewer identity, overlay-effective visibility hash, viewport hash, and overlay config hash.
- Cache hit draws only cached final-region union spans.
- Cache miss rebuilds the current C2/A3/B/C3/12F baseline path and may store the final union.
- Cache disabled rebuilds and draws but does not store.
- Temporal cache invalidation currently considers full-map visible holds only.
- Cell-level `LastVisibleFrames` are intentionally not scanned by cache decision.

Debt:

- Cache hit/miss counters are perf diagnostics, not gameplay semantics.
- Soft-edge caching remains unresolved.
- The cache is single-entry; it is not a general tile cache or multi-viewport cache.

Cleanup rule:

- Keep cache key changes separate from visual changes.
- Do not use raw mutation hash for cache invalidation.
- Do not reintroduce cell-level expiry scans into draw hot paths.

## 5. Runtime State, Serialization, And Save/Load Debt

Current state:

- INI-backed RulesExt and WarheadTypeExt fields are serialized through existing ExtData patterns.
- PhobosFog now persists explored-only memory through an optional HouseExt `PFOG` tail payload.
- PhobosFog visual/cache state, temporary visibility, and diagnostics remain runtime-only.

Runtime-only examples:

- `PhobosFog_CellStates` current `Visible` state is not persisted as visible; save/load stores only whether each cell has been explored.
- `PhobosFog_LastVisibleFrames`
- `PhobosFog_StateVersion`
- `PhobosFog_OverlayEffectiveVersion`
- `PhobosFog_FullMapVisibleUntilFrame`
- `PhobosFog_LastSpySatActive`
- overlay cache entries and scratch buffers
- debug/probe/perf counters

Debt:

- PhobosFog is now broad enough that save/load semantics need a deliberate decision before finalization.
- SaveLoad-B1 confirmed that the previous public `PhobosStreamReader` API did not expose remaining bytes or safe magic peek. Appending an optional tagged `PFOG` payload to `HouseExt::ExtData` was therefore not safe without either a generic stream helper or accepting a new incompatible save format.
- SaveLoad-B2A added the minimal stream helper foundation: remaining-byte queries and non-consuming byte / dword peek.
- SaveLoad-B2B added an optional `PFOG` payload for explored-only persistence after the existing HouseExt payload.
- `HouseExt::ExtData::LoadFromStream(...)` now resets runtime-only fog state separately so restored explored bits are not cleared by the old full reset path.
- Save/load currently restores only explored memory. It does not restore temporary hard-visible windows or exact visible state.

Cleanup rule:

- Before claiming save/load support, classify every state as configuration, authoritative gameplay/presentation state, temporary visual state, cache, or diagnostic state.
- Do not serialize overlay cache or debug counters.
- Keep the current explored-only persistence contract unless a dedicated task expands it: save `Unknown` as 0 and both `Explored` and `Visible` as 1. Do not persist `LastVisibleFrames`, full-map visible holds, overlay cache data, row buckets, template cache, or perf/debug counters.
- Optional payload loading must continue to avoid consuming unrecognized magic on old saves.

## 6. Diagnostics, Probes, And Perf Logs

### 6.1 Debug Logs

Meaning:

- Controlled by `PhobosFog.Debug`.
- Used for correctness and low-frequency state summaries.
- Includes refresh summaries, RegionMask summaries, source-specific reveal logs, and targeted probes.

Debt:

- Some probe logs are historical and should not be considered stable diagnostics.

Cleanup rule:

- Temporary probes should either be removed, kept tightly gated, or explicitly documented as dormant investigation aids.

### 6.2 Perf Logs

Meaning:

- Controlled by `PhobosFog.Perf.Enabled`.
- Independent of `PhobosFog.Debug`.
- Intended for low-frequency manual profiling without enabling verbose debug logs.

Debt:

- Perf field names now span several generations of the overlay pipeline.
- Some fields retained for continuity no longer map to materialized intermediate vectors.

Cleanup rule:

- Update algorithm inventory whenever a perf counter's meaning changes.
- Prefer additive or clarified field names over silent semantic changes.

### 6.3 Probe Logs

Meaning:

- Particle and cliff/building probes are targeted investigation tools.
- They can be noisy and can distort performance if enabled broadly.

Debt:

- Probe code can accumulate in hot files after a bug is solved.

Cleanup rule:

- In final cleanup, audit whether each probe is still needed:
  - keep if it is low-frequency and useful;
  - disable behind a local constant if still useful only for development;
  - remove if it was tied to a failed experiment.

## 7. Current High-Priority Semantic Debts

### 7.1 Terminology Drift In Overlay Code

Problem:

- `CliffCover`, `HeightFace`, `VerticalFace`, `Dilation`, `SoftEdge`, and `UnknownMerge` describe different generations and passes.
- Some names are historical and can mislead future maintenance.

Risk:

- A final polish task may modify the wrong pass or reactivate failed geometry.

Recommended cleanup:

- Add short code comments near prototype constants describing the active pipeline.
- Keep the algorithm inventory as the source of truth.
- Optionally rename only local variables in a dedicated cleanup task if it does not affect behavior.

Priority:

- High before final overlay polish.

### 7.2 `HideBuildings` Name vs Accepted Semantics

Problem:

- The accepted behavior is explored building snapshot semantics plus hard-visible live presentation gating.
- The tag name can imply complete building removal.

Risk:

- Future code may hide snapshots too aggressively or leave active sub-visuals visible in explored fog.

Recommended cleanup:

- Keep the current tag for compatibility in this branch.
- Document it as live-building presentation gating.
- If a clearer tag is desired, add only as an alias in a future compatibility task, not during final cleanup.

Priority:

- High for documentation and regression testing.

### 7.3 Particle Ownership Model

Problem:

- Smoke and death particles required multiple probes and fallbacks.
- The current implementation mixes owner-based, source-cell, hidden-hold, and conservative particle-system handling.

Risk:

- Future particle families may regress visibility or performance.

Recommended cleanup:

- Freeze current behavior if manual testing remains acceptable.
- Defer owner-metadata redesign until a concrete leak reappears.

Priority:

- Medium.

### 7.4 Combat Gate Naming And Scope

Problem:

- `GateAutoTargets`, `GateAutoFire`, and `GateForceFireCells` sit closer to gameplay command/fire control than render-only presentation.
- They are still opt-in but are no longer purely visual.

Risk:

- Future "presentation-only" assumptions may accidentally include these tags.

Recommended cleanup:

- Keep P6 regression freeze as the source of truth.
- In user docs, place combat gates in a separate section from render/UI gates.

Priority:

- Medium to high before final documentation.

### 7.5 SpySat Deactivate Hold Inheritance

Problem:

- `PhobosFog.SpySatellite.DeactivateHoldFrames` is explicit-only and does not inherit `PhobosFog.RevealSources.VisibleHoldFrames`.
- Other source-specific hold tags use `-1` inheritance.

Risk:

- Users may expect global reveal hold settings to affect SpySat deactivation.

Recommended cleanup:

- Keep explicit-only semantics documented clearly. Only change inheritance/default behavior in a dedicated task.
- Do not change this silently during final cleanup.

Priority:

- Medium.

### 7.6 Soft Edge And Cache Split

Problem:

- Soft edge remains a separate pass and disables the final-region cache.
- Main overlay can be cached, but the current cache decision conservatively disables when soft edge is active.

Risk:

- Manual performance tests with soft edge enabled may not reflect the optimized cache-hit path.

Recommended cleanup:

- Keep this as a known limitation.
- A future task can either fold soft-edge spans into the cached final region or build a separate soft-edge cache.

Priority:

- Medium for P9 performance, low for correctness.

### 7.7 Fog State Persistence Boundary

Problem:

- Current PhobosFog state is broad, but only explored memory is save/load-authoritative.

Risk:

- Save/load behavior may be surprising if users expect exact hard `Visible` state, temporary visible holds, or overlay cache contents to persist.

Recommended cleanup:

- Keep user-facing docs explicit that save/load restores explored memory only.
- If exact visible-state persistence becomes a requirement, open a separate design phase instead of extending the optional `PFOG` payload ad hoc.

Priority:

- Medium.

### 7.8 Report Sprawl And Failed Experiment Marking

Problem:

- The repo root contains many phase reports and failed experiment histories.
- Some failed reports are useful but not baseline.

Risk:

- Future work may follow a failed experiment report instead of the living algorithm inventory.

Recommended cleanup:

- Keep living references in `devDocs/`.
- Leave phase reports in place unless the user approves a broader documentation move.
- Mark failed experiments explicitly in living docs and final handoff.

Priority:

- Medium.

## 8. Lower-Priority Semantic Debts

### 8.1 Prototype Constants Not Promoted To INI

Current state:

- Several overlay internals are local prototype constants.

Debt:

- They are accepted implementation details, not stable user controls.

Cleanup rule:

- Do not promote them to INI unless the visual model and performance cost are stable.

### 8.2 Duplicate Visibility Helper Patterns

Current state:

- Several files contain local helper wrappers around viewer/allies hard visibility.

Debt:

- This avoids broad shared API churn but creates repeated patterns.

Cleanup rule:

- Do not centralize just for neatness.
- Consider shared helpers only if final cleanup identifies repeated bugs or measurable overhead.

### 8.3 `PhobosFog.ExploredOverlaySoftEdgeAlpha` Legacy Compatibility

Current state:

- The tag remains but current behavior prefers visible/unknown split alpha tags.

Debt:

- The tag may be perceived as primary even though it is largely legacy.

Cleanup rule:

- Keep it in the INI registry as legacy tuning.
- Avoid basing new behavior on it without an explicit compatibility decision.

## 9. Final Cleanup Preparation Checklist

Before entering final cleanup, verify these points:

- `PhobosFog_INI_Tags.md` lists every current tag, default, clamp, and changed workstream semantic.
- `devDocs/PhobosFog_Algorithm_Inventory_For_Performance.md` matches the active overlay path:
  - direct template-to-row-bucket main fill;
  - row-bucket union;
  - compacted final merge input;
  - height-discontinuity face spans;
  - fallback dilation;
  - strict vertical draw rect batching;
  - single-entry final-region cache;
  - full-map temporal first-invalid-frame cache relaxation.
- P6 combat/command gating remains documented as separate from render-only presentation.
- SpySat deactivate hold explicit-only behavior is documented and remains the accepted baseline unless a dedicated task changes it.
- Dormant legacy cliff cover / probe code is either kept intentionally or removed in a dedicated cleanup task.
- Final docs may claim explored-only persistence for PhobosFog memory, but must not claim exact `Visible` / temporary-hold / cache persistence.
- No final docs describe cache hit behavior as visual behavior.

## 10. Suggested Cleanup Order

1. Freeze current semantics in docs:
   - `PhobosFog_INI_Tags.md`;
   - `devDocs/PhobosFog_Algorithm_Inventory_For_Performance.md`;
   - this document.
2. Keep SpySat deactivate hold explicit-only in final docs unless a dedicated task explicitly changes that behavior.
3. Audit dormant probes and failed-experiment paths:
   - old cliff cover collector;
   - cliff probe activation logs;
   - particle draw hit probes;
   - building probe logs.
4. Do a focused overlay terminology cleanup only if it does not change behavior.
5. Do a focused performance sanity pass with `PhobosFog.Perf.Enabled=true`.
6. Prepare final user-facing documentation only after the dev semantic baseline is stable.
7. Keep save/load semantics constrained to explored-only persistence unless final release scope explicitly expands it.

## 11. Current Non-Actions

- Do not rename INI tags in this cleanup document.
- Do not remove dormant legacy cliff-cover code solely as documentation cleanup.
- Do not introduce a global PhobosFog manager just to centralize semantics.
- Do not convert prototype overlay constants to INI tags without a dedicated task.
- Do not make temporary visible holds, overlay cache data, or perf/debug counters persistent without a separate save/load phase.
- Do not alter P6 command/combat gates while doing overlay cleanup.
- Do not reintroduce `PhobosFog_LastVisibleFrames` full scans into overlay cache decision.
- Do not use raw `PhobosFog_StateVersion` as the overlay cache dirty key.
