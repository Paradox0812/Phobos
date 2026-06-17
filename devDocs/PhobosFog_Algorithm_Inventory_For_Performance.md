# PhobosFog Algorithm Inventory For Performance Optimization

This document catalogs the algorithms currently used by the PhobosFog workstream and the scenes where each algorithm is active. It is a performance-optimization work document, not final upstream user documentation.

Maintenance rule:

- Update this document whenever a PhobosFog algorithm, hot path, draw path, reveal synchronization path, or diagnostic counter changes.
- Keep it aligned with `PhobosFog_INI_Tags.md` and `PhobosFog_CurrentBaseline_And_Feature_Summary.md`.
- Prefer recording real source anchors and runtime gates instead of only feature names.

## 1. High-Level Runtime Model

PhobosFog is built around a per-house cell-state vector:

```text
Unknown -> Explored -> Visible -> Explored
```

The state is runtime-only at the current stage. The main state containers are:

- `HouseExt::ExtData::PhobosFog_CellStates`
- `HouseExt::ExtData::PhobosFog_LastVisibleFrames`
- `HouseExt::ExtData::PhobosFog_StateVersion`
- `HouseExt::ExtData::PhobosFog_StateVersionTouchCount`
- `HouseExt::ExtData::PhobosFog_StateVersionTouchReasons`
- `HouseExt::ExtData::PhobosFog_OverlayEffectiveVersion`
- `HouseExt::ExtData::PhobosFog_OverlayEffectiveTouchCount`
- `HouseExt::ExtData::PhobosFog_FullMapVisibleUntilFrame`
- `HouseExt::ExtData::PhobosFog_LastSpySatActive`
- `HouseExt::ExtData::PhobosFog_LastFullMapHardVisible`

Primary source files:

- `src/Ext/House/Body.h`
- `src/Ext/House/Body.cpp`
- `src/Ext/Scenario/Body.cpp`
- `src/Misc/Hooks.VeinholeMonster.cpp`

Global gates:

- `PhobosFog.Enabled`
- Feature-specific gates such as `PhobosFog.DrawExploredOverlay`, `PhobosFog.HideEnemyFoot`, `PhobosFog.OverrideRadarFog`

Current P9 explored-overlay performance baseline:

- Main overlay fill uses geometry templates directly into row buckets.
- The old active `mainSpans` materialization path is no longer used for main fill.
- Per-row union produces `mainUnionSpans`.
- `compactMainUnionSpans`, height-discontinuity face spans, and fallback dilation are merged into one final region.
- A single-entry cache stores final region union spans when the cache key and temporal validity allow it.
- Soft edge and fade-in still disable final-region cache reuse in the current baseline.

## 2. State Storage And Cell State Transitions

### 2.1 State Vector Sizing

Source anchor:

- `HouseExt::ExtData::EnsurePhobosFogStateSize()`

Algorithm:

- Resize `PhobosFog_CellStates` to `MapClass::MaxCells`, defaulting to `Unknown`.
- Resize `PhobosFog_LastVisibleFrames` to `MapClass::MaxCells`, defaulting to `-1`.
- Return whether either vector was resized.

Scenario:

- Called during refresh and explicit marking helpers.
- Also protects against map transitions and late initialization.

Performance profile:

- Normal case is cheap size checks.
- Resize is O(`MapClass::MaxCells`) and should happen rarely.

Optimization notes:

- Avoid calling helpers that force `EnsurePhobosFogStateSize()` from very hot draw paths unless the state is expected to be ready.
- Resize diagnostics should remain visible in `[PhobosFog] DebugSummary`.
- Resize touches `PhobosFog_StateVersion` because the raw state storage visible to presentation caches has changed.

### 2.2 Visibility Degrade Pass

Source anchor:

- `HouseExt::ExtData::DegradePhobosFogVisibility(int currentFrame)`

Algorithm:

- Iterate the whole cell-state vector.
- If a cell is `Visible` and its `LastVisibleFrame` is older than the current frame, downgrade it to `Explored`.

Scenario:

- Called from `RefreshPhobosFogState()` for each eligible house at `PhobosFog.UpdateInterval`.

Performance profile:

- O(`MapClass::MaxCells`) per eligible house per refresh.
- This is one of the major CPU costs when `UpdateInterval` is low.

Optimization candidates:

- Track currently visible cell indices in a sparse list and degrade only those candidates.
- Maintain a frame-bucket or dirty-visible queue keyed by expiry frame.
- Preserve exact semantics for event-driven temporary visibility before changing this pass.
- Degrading at least one cell touches `PhobosFog_StateVersion`.

### 2.3 Mark Explored

Source anchors:

- `MarkPhobosFogCellExplored(CellStruct cell)`
- `MarkPhobosFogAreaExplored(CellStruct center, double radius)`
- `MarkPhobosFogCellSpreadExplored(CellStruct center, size_t spread)`
- `MarkAllPhobosFogCellsExplored()`

Algorithm:

- Single-cell marking changes only `Unknown` to `Explored`.
- Radius marking uses `CellRangeIterator`.
- Spread marking uses `CellSpreadIterator`.
- Full-map marking iterates valid map cells through `MapClass::Instance.CellIteratorNext()`.

Scenarios:

- Warhead reveal with zero hold frames.
- Spy-plane reveal with zero hold frames.
- SpySat activation when `PhobosFog.SpySatellite.MarkExplored=true`.
- Full-map reveal and reveal-map crate synchronization.

Performance profile:

- Single-cell is O(1).
- Radius/spread is O(affected cells).
- Full-map is O(valid map cells).

Optimization candidates:

- For repeated reveal sources, batch changed cells and queue radar refresh only for changed cells.
- Keep full-map operations edge-triggered, never every frame.
- Marking helpers touch `PhobosFog_StateVersion` only when a cell actually changes from `Unknown` to `Explored`.

### 2.4 Mark Visible Until

Source anchors:

- `MarkPhobosFogCellVisibleUntil(CellStruct cell, int visibleUntilFrame)`
- `MarkPhobosFogAreaVisibleUntil(CellStruct center, double radius, int visibleUntilFrame, PhobosFogRevealAreaStats* pStats)`
- `MarkPhobosFogCellSpreadVisibleUntil(CellStruct center, size_t spread, int visibleUntilFrame, PhobosFogRevealAreaStats* pStats)`
- `MarkAllPhobosFogCellsVisibleUntil(int visibleUntilFrame)`

Algorithm:

- Promote cells to `Visible`.
- Extend `LastVisibleFrames[index]` only when the new `visibleUntilFrame` is later.
- Optional stats count affected, invalid, promoted, extended, skipped, and bounds.

Scenarios:

- Sight refresh uses the current frame as the visible-until frame.
- Reveal sources use `currentFrame + holdFrames`.
- Full-map visible hold uses a house-level `FullMapVisibleUntilFrame` instead of rewriting every cell when possible.

Performance profile:

- Same shape as explored marking, but with extra per-cell expiry comparisons and optional stats.

Optimization candidates:

- Avoid collecting stats unless `PhobosFog.Debug=true`.
- Prefer full-map effective visibility override for full-map sources instead of touching every cell as `Visible`.
- Visible helpers touch `PhobosFog_StateVersion` only when a cell is promoted to `Visible` or its visible-until frame is extended.
- Full-map visible holds touch `PhobosFog_StateVersion` when the hold frame is extended.

### 2.5 StateVersion Touch Audit Counters

Source anchors:

- `HouseExt::PhobosFogStateTouchReason`
- `HouseExt::ExtData::TouchPhobosFogStateVersion(...)`
- `[PhobosFog][Perf]` in `PhobosFogExploredOverlay::Draw()`

Algorithm:

- Every real `PhobosFog_StateVersion` touch is classified by source reason.
- Runtime-only counters track total touches and per-reason touch counts.
- Counters are incremented only when `PhobosFog.Perf.Enabled=true`.
- Perf summary reports low-frequency deltas since the previous perf line.

Current reasons:

- `Reset`
- `EnsureResize`
- `DegradeVisibleToExplored`
- `MarkExplored`
- `MarkVisible`
- `MarkVisibleUntil`
- `MarkAreaVisible`
- `MarkCellSpreadVisible`
- `MarkAllExplored`
- `MarkAllVisible`
- `FullMapVisibleUntil`
- `SpySatPersistentVisibleEdge`
- `Unknown`
- `Other`

Performance profile:

- No extra counter work when `PhobosFog.Perf.Enabled=false`.
- When perf is enabled, every StateVersion touch does one counter increment plus one reason bucket increment.
- The perf overlay path sums counters for current viewer plus valid allies only when it already builds Stage A diagnostics.

Optimization candidates:

- Use A2 perf deltas to identify whether churn comes from normal sight `MarkVisible`, visible-until reveal holds, degrade, resize/reset, SpySat edge changes, or full-map visible holds.
- Do not deduplicate touch sources until runtime logs identify a clearly safe source.

## 3. Effective Visibility Queries

### 3.1 Single-House Effective State

Source anchors:

- `HouseExt::ExtData::GetEffectivePhobosFogCellState(int cellIndex) const`
- `HouseExt::ExtData::IsPhobosFogFullMapHardVisible() const`

Algorithm:

- Return `Visible` if SpySat persistent visibility or full-map hold is active.
- Otherwise return the stored cell vector state.
- Invalid or uninitialized state returns `Unknown`.

Scenarios:

- All render, UI, radar, command, and overlay decisions eventually rely on this semantic.

Performance profile:

- O(1), with small config and state checks.

Optimization candidates:

- Keep this function simple. It is used in hot paths.
- Avoid adding logging or allocation here.
- SpySat persistent visibility activation and deactivation edges touch `PhobosFog_StateVersion` because they change effective visibility without rewriting the raw cell vector.

### 3.2 Current Player Plus Allies Merge

Source anchors:

- `TryGetEffectivePhobosFogCellStateForViewerOrAllies(...)`
- `IsPhobosFogCellHardVisibleToViewerOrAllies(...)`

Algorithm:

- Query current player state first.
- If current player is `Visible`, return `Visible`.
- Otherwise scan eligible allied houses.
- Any allied `Visible` wins.
- If no visible state exists but any queried state is `Explored`, return `Explored`.
- Otherwise return `Unknown`.

Scenarios:

- Enemy FootClass hiding.
- Building foundation visibility.
- World animation and particle gating.
- Hover cursor, tooltip, health bar, command target gating.
- Radar object dots and radar background color.
- Explored overlay state classification.

Performance profile:

- O(number of houses) per query.
- This is extremely hot because many render paths query it per object, per cell, or per span source cell.

Optimization candidates:

- Add per-frame viewer/allied effective-state cache for visible viewport cells.
- Add fast path when there are no eligible allies.
- Avoid duplicating ally-merge helpers in multiple files if a shared fast helper becomes acceptable.

## 4. Sight Refresh

Source anchor:

- `RefreshPhobosFogState()` in `src/Ext/Scenario/Body.cpp`

Algorithm:

1. Abort if `PhobosFog.Enabled=false`.
2. Run only on frames matching `PhobosFog.UpdateInterval`.
3. Process pending radar cell refresh budget if radar override is active.
4. For each eligible house:
   - ensure vector size;
   - reset debug counters when needed;
   - update SpySat edge state;
   - queue full-map radar refresh on full-map hard-visible edge;
   - optionally queue visible radar cells;
   - degrade expired visible cells to explored.
5. For each eligible techno:
   - resolve sight via `TechnoExt::GetSight()`;
   - iterate cells in sight range with `CellRangeIterator`;
   - mark those cells visible for the owner house;
   - optionally queue radar refresh.
6. Emit debug summary every 900 frames when debug is enabled.

Scenarios:

- Main runtime state refresh.
- Drives hard visibility for all presentation gates.

Performance profile:

- Per refresh cost roughly:
  - O(eligible houses * map cells) for degrade;
  - O(eligible technos * sight area) for sight marking;
  - optional O(visible cells) radar queue work.

Existing protection:

- `PhobosFog.UpdateInterval` controls cadence.
- Radar scheduled full refresh is processed with `PhobosFogRadarRefreshBudgetPerFrame=512`.
- Debug summary is throttled to 900 frames.

Optimization candidates:

- Sparse degrade list.
- Dirty-cell radar refresh queue with deduplication.
- Per-house visible mark dirty counters.
- Per-frame sight provider budget, if needed.

## 5. Render-Only Object Hiding

### 5.1 Enemy FootClass Draw Hiding

Source anchor:

- `TacticalClass_RenderLayers_DrawBefore` in `src/Ext/Techno/Hooks.Pips.cpp`
- FootClass path at `R->Origin() == 0x6D9076`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.HideEnemyFoot`.
- Ignore invalid, observer, own, and allied objects.
- Convert object map cell to cell index.
- Hide if the cell is not hard `Visible` to current player or valid allies.
- Skip current FootClass draw only, using the verified render-only return path.

Scenario:

- Enemy moving object body hiding in world render.

Performance profile:

- O(number of allied houses) per enemy FootClass draw.

Optimization candidates:

- Cache object-cell visibility for current frame.
- Reuse the same effective-visibility result for body, shadow, particles, cursor, and radar when possible.

### 5.2 Live Building Draw Hiding

Source anchor:

- `TacticalClass_RenderLayers_DrawBefore` in `src/Ext/Techno/Hooks.Pips.cpp`
- BuildingClass path at `R->Origin() == 0x6D9134`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.HideBuildings`.
- Use the building foundation data, not only the building base cell.
- Query each valid foundation cell.
- If any foundation cell is hard `Visible`, keep the live building draw.
- If the query succeeds and no foundation cell is visible, skip the live building draw.

Scenario:

- Suppresses live-only building presentation in non-visible fog.
- Explored/fogged snapshots may still draw as last-known information.

Performance profile:

- O(foundation cells * allied houses) per building draw.

Optimization candidates:

- Cache foundation visibility per building per frame.
- Store foundation cell-index list if repeatedly recomputing offsets becomes measurable.

### 5.3 Fogged Building Snapshot Gating

Source anchor:

- `FoggedObjectClass_Draw_PhobosFogHideBuildings` in `src/Ext/Techno/Hooks.Pips.cpp`

Algorithm:

- Gate only building fogged snapshots.
- Hide the snapshot only when the snapshot cell is not known (`Unknown`) to current player or allies.
- This allows explored building snapshot semantics.

Scenario:

- Keeps last-known static building presence in explored fog.
- Avoids showing buildings in fully unknown areas.

Performance profile:

- O(number of allied houses) per snapshot draw.

Optimization candidates:

- Same viewer/allied cell-state cache as other presentation gates.

## 6. World Animation, Terrain, Ore, And Particle Leak Gating

### 6.1 Map-Space AnimClass Gating

Source anchor:

- `PhobosFogWorldAnim::ShouldHide(...)` in `src/Ext/Anim/Hooks.cpp`
- Draw hook: `AnimClass_DrawIt_Visibility`

Algorithm:

- Gate on `PhobosFog.Enabled`.
- Building-attached anims use parent-building foundation visibility.
- Non-building map-space anims first try owner techno visibility, then fall back to anim/map-space cell visibility.
- Hide only when a visibility query succeeds and returns not visible.

Scenarios:

- Building attached animations.
- Unit firing or trailer animations.
- Map-space world visual leaks.

Performance profile:

- Usually O(allied houses), or O(foundation cells * allied houses) for building-attached anims.

Optimization candidates:

- Cache parent-building visibility.
- Reduce repeated owner resolution for long-lived anims by storing metadata in `AnimExt`.

### 6.2 Terrain And Tiberium-Spawner Hiding

Source anchor:

- `PhobosFogTerrain::ShouldHide(...)` in `src/Ext/TerrainType/Hooks.cpp`
- Draw path near `TerrainClass_Draw_Palette`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.HideTiberiumSpawners`.
- Query terrain map cell hard visibility.
- Hide non-visible terrain/spawner visuals.

Scenarios:

- Terrain-like visual leaks such as tiberium-spawner ore generation visuals.

Performance profile:

- O(allied houses) per terrain draw candidate.

Optimization candidates:

- Cache terrain cell visibility by cell index per frame.

### 6.3 Ore Gatherer / Mining Device Visual Gating

Source anchor:

- `PhobosFogOreGath::ShouldHide(...)` in `src/Ext/TechnoType/Hooks.cpp`
- Hook: `UnitClass_DrawIt_OreGath`

Algorithm:

- Gate on PhobosFog rendering switches.
- Query unit/map cell visibility.
- Skip the ore-gather visual if it belongs to a hidden or non-visible owner context.

Scenarios:

- Ore gatherer or mining-device visual leak paths that are not covered by main object draw hiding.

Performance profile:

- O(allied houses) per draw candidate.

Optimization candidates:

- Share enemy FootClass visibility decision for the same unit when possible.

### 6.4 ParticleClass And ParticleSystemClass Gating

Source anchor:

- `PhobosFogParticle` namespace in `src/Ext/ParticleType/Hooks.cpp`
- Hooks:
  - `ParticleClass_Draw_PhobosFogVisibility`
  - `ParticleSystemClass_Draw_PhobosFogProbe`

Algorithm:

- Analyze particle or particle system owner candidates.
- Resolve possible owner techno from system owner, owner anim, damage/fire/spark/natural/railgun/firing system fields, target techno, and other known fields.
- Prefer enemy FootClass owner visibility when available.
- Fall back to particle/system map cell visibility when owner is not usable.
- Maintain a particle-system visibility cache with:
  - hidden hold frames;
  - visible release frames;
  - cache prune frames.

Scenarios:

- Damaged-unit smoke.
- Death smoke.
- Fire/spark/damage particle systems.
- Long-lived world particles.

Performance profile:

- High-risk hot path because particles can be numerous.
- Owner-candidate analysis and cache pruning are the main costs.

Existing protection:

- Probe logging is disabled by default.
- Runtime logs are capped/throttled.
- Cache entries are pruned after a long stale interval.

Optimization candidates:

- Replace vector cache lookup with stable key lookup if particle-system counts become high.
- Store owner metadata earlier at particle-system creation where safe.
- Avoid expensive owner-field scans after a cache hit.
- Keep all debug string construction behind explicit debug/probe gates.

## 7. UI, Cursor, Health Bar, And Command Gating

### 7.1 Tooltip And Object Name Gating

Source anchor:

- `PhobosFogTooltip::ShouldSuppressObjectTooltip(...)` in `src/Ext/TechnoType/Hooks.cpp`
- Hook: `DisplayClass_GetToolTip_EnemyUIName`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.HideHoverTooltip`.
- Convert hovered object to techno or object cell.
- Hide tooltip when object is not hard `Visible`.

Scenario:

- Prevent hover text from revealing hidden enemies.

Performance profile:

- O(allied houses), or foundation-cell cost for buildings.

### 7.2 Cursor Presentation Gating

Source anchor:

- `PhobosFogTooltip::ShouldSuppressObjectCursor(...)` in `src/Ext/TechnoType/Hooks.cpp`
- Hook: `DisplayClass_ConvertAction_PhobosFogCursor`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.HideHoverCursor`.
- If hovered object is hidden, compute a cell-only action and replace object-aware cursor presentation.

Scenario:

- Prevent attack/special cursor states from revealing hidden objects.

Performance profile:

- O(allied houses) per hover conversion.

### 7.3 Hidden Object Command Gating

Source anchor:

- `PhobosFogTooltip::ShouldGateHiddenObjectCommand(...)` in `src/Ext/TechnoType/Hooks.cpp`
- Hook: `DisplayClass_sub_4AE750_PhobosFogCommandGate`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.GateHiddenObjectCommands`.
- If target object is hidden and non-allied, clear the object target before vanilla command dispatch.
- The command degrades toward ordinary cell command behavior.

Scenario:

- Prevent click command paths from revealing or acting on hidden enemy objects.

Performance profile:

- O(allied houses) per click command.

Risk:

- This is closer to command path than pure presentation. Any future optimization must preserve the current separation from AI and autonomous logic.

### 7.4 Health Bar Gating

Source anchor:

- `ShouldSuppressPhobosFogHoverHealthBar(...)` in `src/Ext/Techno/Hooks.Pips.cpp`
- Hooks:
  - `TechnoClass_DrawExtras_DrawHealthBar`
  - `TechnoClass_DrawHealthBar_Buildings`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.HideHoverHealthBar`.
- Use techno visibility, with building foundation visibility for buildings.
- Suppress health bar drawing when hidden.

Scenario:

- Prevent health bars and pips from revealing hidden technos.

Performance profile:

- O(allied houses), or O(foundation cells * allied houses) for buildings.

Optimization candidates:

- Share cached building visibility with building draw and animation gates.

## 8. Radar And Minimap Algorithms

### 8.1 Radar Object Dot Hiding

Source anchor:

- `PhobosFogRadar::TryGetEnemyObjectVisibility(...)` in `src/Ext/Techno/Hooks.cpp`
- Hook: `RadarClass_ProcessPoint_RadarInvisible`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.HideRadarObjects`.
- Ignore own/allied objects.
- Buildings use foundation visibility.
- Non-buildings use object map cell visibility.
- If query succeeds and object is not visible, treat radar dot as invisible.

Scenario:

- Enemy object dot presentation on radar/minimap.

Performance profile:

- Per radar point. Cost can grow with number of visible map objects and radar refresh frequency.

Optimization candidates:

- Cache per-object radar visibility for the current frame.
- Share with world render visibility cache where object identity is stable.

### 8.2 Radar Background Cell Color Override

Source anchors:

- `PhobosFogRadar::ApplyRadarFogColors(...)`
- Hooks:
  - `RadarClass_CellColor_PhobosFog_FullRefresh`
  - `RadarClass_CellColor_PhobosFog_DirtyRefresh`

Algorithm:

- Gate on `PhobosFog.Enabled && PhobosFog.OverrideRadarFog`.
- Merge current player and allied effective states:
  - `Visible`: keep vanilla radar color.
  - `Explored`: darken vanilla color by half.
  - `Unknown`: black.
- If state query fails, treat as unknown.

Scenario:

- Radar background fog display.

Performance profile:

- Per radar cell color calculation.
- Full refresh can be expensive.

Existing protection:

- Radar debug stats are throttled to 900 frames.
- Full-map edge transitions schedule all radar cells but process refreshes with a per-frame budget.

Optimization candidates:

- Deduplicate radar refresh queue.
- Keep full-map radar refresh edge-triggered.
- Avoid queuing every visible cell every refresh if radar does not need it.

## 9. Explored Overlay Algorithms

### 9.1 Viewport Cell Scan

Source anchor:

- `PhobosFogExploredOverlay::Draw()` in `src/Misc/Hooks.VeinholeMonster.cpp`

Algorithm:

- Convert four screen viewport corners to map cells.
- Expand the min/max cell rectangle by `PhobosFog.ExploredOverlayViewportPaddingCells`.
- Reject if estimated cell count exceeds `MaxOverlayCellsPerFrame`.
- Iterate only this padded viewport cell rectangle.

Scenario:

- Main custom explored overlay draw pass.

Performance profile:

- O(viewport cells * cell classification cost).
- Viewport padding directly expands the scan area.

Optimization candidates:

- Precompute visible viewport cell list if camera movement is small.
- Cache per-frame cell classification in the overlay pass.
- Keep padding large enough to prevent edge gaps but no larger than needed.

### 9.2 Overlay Cell Classification

Source anchors:

- `GetOverlayCellKindForViewerAndAllies(...)`
- `ClassifyExploredOverlayFrontier(...)`

Algorithm:

- Classify cell as `Unknown`, `ExploredOverlay`, or `Visible` using current player plus allies.
- Frontier classification checks 4 or 8 neighbors depending on `PhobosFog.ExploredOverlayFrontierMode`.
- Tracks visible 4-neighbors for soft-edge pass.

Scenario:

- Determines whether a cell draws overlay, which shape it uses, and whether soft edge is emitted.

Performance profile:

- This is a hot multiplier inside viewport scan.
- Frontier mode 8 performs more neighbor state queries than mode 4.

Optimization candidates:

- Build a compact per-frame state grid for the padded viewport.
- Classify each cell once, then reuse it for main spans, soft edge, and future overlay passes.
- Fast path when no allies exist.

### 9.3 Screen Anchor And Height Awareness

Source anchor:

- `GetOverlayCellClient(...)`

Algorithm:

- Convert `CellStruct` to `CoordStruct`.
- If height-aware mode is enabled, use `CellClass::GetCellCoords()`.
- Convert world coords to client coords through `TacticalClass::CoordsToClient`.
- Apply `PhobosFog.ExploredOverlayHeightYOffset`.

Scenario:

- Main overlay placement.
- Soft edge placement.
- Legacy cliff-cover probe geometry.

Performance profile:

- Called for every source cell and additional neighbor cells.

Optimization candidates:

- Cache cell client centers for the viewport grid.
- Separate flat-center and height-aware center caches if both are needed.

### 9.4 Shape Span Generation

Source anchors:

- `AddRectSpans(...)`
- `AddDiamondSpans(...)`
- `AddOverlayShapeSpans(...)`
- `TryGetDiamondSpan(...)`

Algorithm:

- Rectangle mode emits one horizontal span for each covered screen row.
- Diamond mode computes row width from vertical distance to diamond center and emits clipped row spans.
- Shape mode:
  - `0`: rectangle.
  - `1`: diamond only on frontier cells.
  - `2`: diamond on every explored overlay cell.

Scenario:

- Main explored overlay mask.

Performance profile:

- O(cell overlay height) per source cell.
- Diamond math is small but repeated many times.

Optimization candidates:

- Precompute row half-width tables for common cell height/width.
- Avoid all-diamond mode for performance-sensitive testing unless needed.

### 9.5 Stable Alpha Variance And Fade-In

Source anchors:

- `StableCellNoise(...)`
- `ComputeCellAlpha(...)`
- `ApplyFadeInAlpha(...)`

Algorithm:

- Stable per-cell hash creates alpha variation.
- Fade-in uses max last-visible frame across current player and allies.
- Overlay alpha ramps from 0 to configured alpha over `PhobosFog.ExploredOverlayFadeInFrames`.

Scenario:

- Visual breakup and smoother transition from visible to explored.

Performance profile:

- O(allied houses) for fade-in frame lookup.

Optimization candidates:

- Cache last-visible frame merge alongside cell-state classification.
- Disable variance and fade during raw performance profiling.

### 9.6 Span Union

Source anchors:

- `MergeSpansToUnionSpans(...)`
- `DrawUnionSpans(...)`
- `DrawCoalescedRects(...)`

Algorithm:

- Sort spans by Y and X.
- For each Y row, convert spans to begin/end events.
- Sweep events to produce non-overlapping union spans, choosing the highest active alpha.
- Convert union spans to 1-pixel-high draw rects.
- Coalesce vertically adjacent rects with identical X, width, and alpha.
- Draw with `DSurface::Composite->FillRectTrans`.

Scenario:

- Main region fill.
- Soft-edge union.
- Height-discontinuity face and fallback-dilation region union.
- Dormant old cliff cover union when enabled by prototype constant.

Performance profile:

- Sorting dominates: O(spans log spans).
- Draw rect count is bounded by `PhobosFog.ExploredOverlayMaxDrawRects`.

Existing protection:

- `PhobosFog.ExploredOverlayMaxDrawRects`.
- `[PhobosFog][RegionMask]` summary includes raw/merged/final span and draw counts.

Optimization candidates:

- Reuse vectors to reduce per-frame allocations.
- Bucket spans by row before sorting.
- Reduce span emission before union.

### 9.7 Region Height Face, Fallback Dilation, And Close-Gap Union

Source anchors:

- `TryAddHeightDiscontinuityFaceSpans(...)`
- `AddCliffFaceStripSpans(...)`
- `GenerateDownDilationSpans(...)`
- `MergeRegionSpansWithCloseGap(...)`

Algorithm:

- Current P9 main fill starts from `template rows -> clipped spans -> row buckets -> per-row union -> mainUnionSpans`.
- The old `mainSpans -> MergeSpansToUnionSpans -> mainUnionSpans` path is no longer the active main-fill baseline.
- For each explored overlay source cell, scan only screen-facing offsets `(1,0)` and `(0,1)`.
- Compare the current cell edge with the neighbor opposite edge in screen space.
- Compute per-endpoint drops:

```text
dropA = neighborEdgeA.Y - currentEdgeA.Y
dropB = neighborEdgeB.Y - currentEdgeB.Y
```

- Normalize edge endpoint order before computing drops.
- If the projected drop is significant, build a vertical face quad using per-endpoint clamped drops.
- Reuse `AddCliffFaceStripSpans(...)` only as a generic quad-to-span rasterizer.
- Compact `mainUnionSpans` with `MergeRegionSpansWithCloseGap(...)` into `compactMainUnionSpans`.
- Generate a small fallback down dilation from `compactMainUnionSpans`, not raw `mainSpans`.
- Combine `compactMainUnionSpans + verticalFaceSpans + fallbackDilationSpans`.
- Run final close-gap union and draw once.
- Main, height face, and fallback dilation are not drawn as separate alpha layers.
- Task12C3-B switches the real final merge input to `compactMainUnionSpans` after Task12C3-A confirmed equivalence.

Scenario:

- Current replacement for the old cliff-cover geometry line and broad down-dilation prototype.
- Intended to cover height discontinuity seams with actual projected face depth, while keeping a small fallback for rounding gaps.

Performance profile:

- Height face scan is O(explored source cells * 2 directions).
- Height face rasterization cost depends on accepted face height and screen span width.
- Compact-main generation is one close-gap union over `mainUnionSpans`.
- Fallback dilation is O(`compactMainUnionSpans * VerticalFaceFallbackDilationY`).
- Final close-gap union is over compact main spans, height-face spans, and fallback dilation spans.

Current prototype constants:

```text
UseHeightDiscontinuityFacePrototype=true
VerticalFaceMinDropPixels=4
VerticalFaceMaxDropPixels=64
VerticalFaceFallbackDilationY=2
UseRegionDilationPrototype=true
RegionDilationY=VerticalFaceFallbackDilationY
RegionCloseGapPixels=2
RegionDilationAlphaNumerator=1
RegionDilationAlphaDenominator=1
UseLegacyCliffCoverInRegionMaskPrototype=false
```

Optimization candidates:

- Cache projected cell centers and diamond edge endpoints for the padded viewport grid.
- Reduce repeated `TryGetCellAt` and `CoordsToClient` calls inside the two-direction face scan.
- Keep fallback dilation small. Increasing it broadly can cause spill and span blowup.
- Fuse dilation generation with row-bucket union to avoid extra full vector sort.
- Use the Task12C2 compact-before-dilation counters to confirm whether fallback dilation input is shrinking during cache misses.
- Keep old cliff-cover disabled unless a new geometry model is proven correct.

### 9.8 Soft Edge

Source anchors:

- `AddRectangularSoftEdgeSpans(...)`
- `AddDiamondSoftEdgeTowardNeighborSpans(...)`
- `AddDiamondSoftEdgeSideSpans(...)`

Algorithm:

- Emit edge spans where explored cells border visible cells.
- Rectangular mode emits layered strips along the relevant side.
- Diamond mode emits edge-side spans toward the neighbor.
- Soft-edge spans are unioned and drawn after the final main region draw.

Scenario:

- Visual transition from visible area to explored overlay.

Performance profile:

- O(frontier cells * softEdgePadding).
- Extra draw pass after main region draw.

Optimization candidates:

- Integrate soft edge into a single region-mask pipeline if overlap darkening or extra draw cost returns.
- Cache frontier neighbor directions from overlay classification.

### 9.9 Legacy Cliff Cover And Probe Path

Source anchors:

- `CollectCliffCoverEdges(...)`
- `AddCliffFaceStripSpans(...)`
- `LogCliffProbe...` helpers

Current state:

- The old cliff-cover path is bypassed in the current region-mask prototype by:

```text
UseLegacyCliffCoverInRegionMaskPrototype=false
```

Algorithm when enabled:

- Check 4-neighbor height-drop candidates.
- Filter by height delta, level delta, cliff tile flags, and forward-offset rules.
- Build a face quad and rasterize it as spans.
- Emit detailed probe logs on throttled debug frames.

Scenario:

- Earlier attempt to cover cliff vertical faces.
- Currently retained as dormant code and diagnostics, not active baseline.

Performance profile:

- Potentially expensive because it adds neighbor cell coordinate conversion, height projection, probe logging, and polygon rasterization inside the overlay cell loop.

Optimization guidance:

- Do not re-enable this path for performance work unless the geometry model is redesigned and accepted.
- If reactivated, keep probe logging strictly throttled and behind `PhobosFog.Debug=true`.

### 9.10 Overlay Cache State-Version And Key Plumbing

Source anchors:

- `HouseExt::ExtData::PhobosFog_StateVersion`
- `HouseExt::ExtData::TouchPhobosFogStateVersion(...)`
- `HouseExt::ExtData::PhobosFog_StateVersionTouchCount`
- `HouseExt::ExtData::PhobosFog_StateVersionTouchReasons`
- `HouseExt::ExtData::PhobosFog_OverlayEffectiveVersion`
- `HouseExt::ExtData::TouchPhobosFogOverlayEffectiveVersion()`
- `HouseExt::ExtData::BeginPhobosFogOverlayEffectiveBatch()`
- `HouseExt::ExtData::TrackPhobosFogOverlayOriginalCell(...)`
- `HouseExt::ExtData::EndPhobosFogOverlayEffectiveBatch()`
- `PhobosFogExploredOverlay::BuildOverlayCacheStageADiagnostics(...)`
- `[PhobosFog][Perf]` in `PhobosFogExploredOverlay::Draw()`

Current state:

- Task12C-A adds cache-key diagnostics only.
- Task12C-A2 adds raw state-version touch reason counters.
- Task12C-A3 splits raw mutation diagnostics from overlay-effective dirty-key diagnostics.
- Task12C-B enables a single-entry cache for `finalRegionUnionSpans`.
- Task12C2 reduces cache-miss rebuild span budget by compacting `mainUnionSpans` before fallback dilation and by reusing key scratch vectors.
- Task12C3-A adds a perf-only double-path final merge comparison. It checks whether the compact main union can replace the old main union in a future phase, but it does not change real draw output or cache contents.
- Task12C3-B switches the real final merge input and cached final union to `compactMainUnionSpans + verticalFaceSpans + fallbackDilationSpans`.
- Task12E-B keeps the existing strict vertical rect batching in `DrawCoalescedRects(...)`, but reuses `OverlayScratchDrawRects` in `DrawUnionSpans(...)` to avoid rebuilding a local draw-rect vector allocation on every draw call.
- Task12F-B verified that row-bucket main union output is equivalent to the previous global `MergeSpansToUnionSpans(mainSpans)` output.
- Task12F-C switches the real main union path to row buckets. `mainSpans` are bucketed by clipped screen Y and each row uses the same max active alpha event sweep as the old global union path.
- Task12F-E-C-B freezes the P9 main fill baseline as direct template-to-row-bucket generation. The active path no longer materializes `mainSpans`; `RawMainSpans` now means clipped template rows accepted into row buckets.
- Task12T-B relaxes temporal visibility cache handling. Active temporary visible holds no longer disable the overlay cache outright. Cache entries store the first frame where temporal visibility can become invalid, and a matching cache entry is forced to miss only when that frame is reached.
- Task12T-B-Fix1 removes the full `PhobosFog_LastVisibleFrames` scan from the overlay cache decision hot path. The temporal cache constraint now only uses viewer and valid allied `PhobosFog_FullMapVisibleUntilFrame` values.
- A cache hit skips viewport scan, main span generation, height-face generation, fallback dilation, and close-gap union. It only redraws the cached final union spans.

Algorithm:

- `PhobosFog_StateVersion` starts at `1`, is runtime-only, and is not serialized.
- `PhobosFog_StateVersion` is the raw mutation and diagnostic version. It remains useful for churn analysis, but is no longer the preferred future overlay cache dirty key.
- Task12C-A2 adds runtime-only touch reason counters for audit only.
- Touch counters are not serialized and are not gameplay state.
- `PhobosFog_OverlayEffectiveVersion` starts at `1`, is runtime-only, and is not serialized.
- Overlay-effective version tracks house-local effective overlay state changes, not viewer-plus-allies merged state.
- The refresh path wraps each eligible house in an overlay-effective batch:
  - begin before `DegradePhobosFogVisibility(...)`;
  - mark original house-local effective cell state before each raw write;
  - include techno sight writes in the same house batch;
  - end after the techno sight loop for all eligible houses.
- Batch end compares original house-local effective state with final house-local effective state and touches `PhobosFog_OverlayEffectiveVersion` once only when at least one final effective state changed.
- Reset, resize, reveal, SpyPlane, full-map hold, and SpySat persistent edge paths touch `PhobosFog_OverlayEffectiveVersion` directly when they change house-local overlay-effective state outside the refresh batch.
- The raw effective visibility hash includes:
  - current viewer identity;
  - current viewer `PhobosFog_StateVersion`;
  - valid allied house identities;
  - valid allied house `PhobosFog_StateVersion`.
- The overlay-effective visibility hash includes the same viewer and valid allied identities, but uses each house's `PhobosFog_OverlayEffectiveVersion`.
- The viewport hash includes:
  - `DSurface::ViewBounds`;
  - viewport cell range;
  - first viewport cell;
  - a projected sentinel point.
- The config hash includes overlay settings and C2 prototype constants that affect region span generation.
- `PhobosFog.ExploredOverlayFadeInFrames > 0` disables future caching because alpha is frame-dependent.
- Active full-map visible hold state sets `TemporalVisibilityActive=true` and computes `TemporalCacheFirstInvalidFrame`.
- Temporal cache decision uses the runtime `>= CurrentFrame` semantics. If `FullMapVisibleUntilFrame` is still effective on the current frame, the cache entry records `untilFrame + 1` as the first invalid frame, with overflow clamped to a safe maximum.
- Cell-level `PhobosFog_LastVisibleFrames` are intentionally not scanned by overlay cache decision. They can be numerous and are hot during unit movement, targeting, and sight refresh. Their effective visual changes are still represented through `PhobosFog_OverlayEffectiveVersion`.
- Temporal state is intentionally not written into `OverlayConfigHash` or the cache key. A cache hit instead checks the stored `TemporalCacheFirstInvalidFrame`; if `CurrentFrame >= TemporalCacheFirstInvalidFrame`, the entry is treated as a miss and rebuilt with the same key.
- `PhobosFog.ExploredOverlaySoftEdge=true` disables the first cache implementation because soft edge remains a separate non-cached pass.
- `PhobosFog.ExploredOverlayAlphaVariance` is deterministic stable noise, so it is included in the config hash instead of disabling cache.
- The Task12C-B cache key is:
  - current viewer identity;
  - `OverlayEffectiveVisibilityVersionHash`;
  - `OverlayViewportHash`;
  - `OverlayConfigHash`.
- The cache key intentionally does not use `PhobosFog_StateVersion` or `RawEffectiveVisibilityVersionHash`.
- Cache miss with `OverlayCacheAllowed=true` rebuilds the C2/A3 baseline `finalRegionUnionSpans`, stores them in the single-entry cache, then draws them.
- Cache-miss rebuilds use the direct template-to-row-bucket main path. They do not build the old active `mainSpans` vector for main fill.
- During cache-miss rebuild, fallback dilation and final merge both use `compactMainUnionSpans`.
- The cached final union is now built from `compactMainUnionSpans + verticalFaceSpans + fallbackDilationSpans`.
- Cache-miss rebuild reuses scratch vectors for main, edge, height-face, compact, dilation, final-region, and union spans to reduce repeated heap allocation while panning or exploring.
- Cache disabled rebuilds and draws the baseline path, but does not update the cache.
- A draw-budget failure invalidates or avoids storing the cache entry.

Performance profile:

- The cache decision is independent of `PhobosFog.Perf.Enabled`; cache hit/miss behavior still runs when perf logging is disabled.
- `PhobosFog.Perf.Enabled=false` does not construct perf strings.
- Temporal visibility detection scans viewer and valid allied `FullMapVisibleUntilFrame` for holds greater than or equal to the current frame. It no longer scans `LastVisibleFrames`.
- `TemporalCellExpiryScanSkipped`, `TemporalFullMapFirstInvalidFrame`, `TemporalCacheFirstInvalidFrame`, `TemporalCacheExpiresInFrames`, and `TemporalCacheHitBlockedByExpiry` in `[PhobosFog][Perf]` show whether temporal state is active and whether an otherwise matching cache entry was forced to rebuild because its full-map temporal hold expired.
- When perf is enabled, StateVersion touch counters for current viewer plus valid allies are summed for low-frequency audit deltas.
- Overlay-effective batch tracking still runs when perf logging is disabled because it is now semantic dirty-key plumbing, but it avoids string construction and perf-only aggregate scans.
- Cache hits reduce the hottest overlay work to drawing cached spans. Cache misses retain the C2/A3 visual baseline.
- Cache misses now report pre-compact, compacted, fallback dilation, and final merge input span counts so dynamic panning/exploration costs can be separated from stable cache hits.
- Task12C3-B removes the old-vs-compact double merge diagnostic from the default path, so `PhobosFog.Perf.Enabled=true` no longer pays that extra comparison cost.
- Task12E-B adds `UnionSpans`, `DrawRectBatchReduction`, and `DrawRectScratchCapacity` to perf output. `DrawRects` still reports actual `FillRectTrans(...)` calls after strict vertical rect batching, while `UnionSpans` reports the region union span count sent to `DrawUnionSpans(...)`.
- Task12F-C reports `RowBucketInputSpans`, `RowBucketNonEmptyRows`, and `RowBucketMainUnionSpans` after cache-miss rebuilds. The old 12F-B equivalence comparison is disabled by default and no longer performs a second union/compare when perf logging is enabled.
- Task12F-E-A adds geometry-only rect / diamond cell shape templates for the explored overlay main fill. The template stores only local row geometry (`LocalY`, `X1Offset`, `X2Offset`) keyed by cell overlay width, height, and shape type. Runtime alpha, fade-in, frontier shape selection, clipping, row-bucket union, final merge, cache key, and cache storage semantics are unchanged. The first prototype still instantiates template rows into `mainSpans`.
- Task12F-E-B adds viewport-clipped template instantiation. The template path computes the visible local row range from the projected rect top and current viewport bounds before iterating rows. It also skips rows whose exclusive X span is fully outside the viewport. `TryAddClippedSpan(...)` remains the final clipping authority, and the path still writes to `mainSpans` before row-bucket union.
- Task12F-E-C-A adds a perf-only direct template-to-row-bucket equivalence diagnostic. Cache-miss rebuilds still use the real `template -> mainSpans -> row-bucket union` path. When `PhobosFog.Perf.Enabled=true`, a separate diagnostic path rescans the same source cells, reuses frontier shape selection, templates, viewport preclip, alpha/fade, and clipping semantics, writes directly into row buckets, unions per row, and compares `Y / X1 / X2 / Alpha` against the real `mainUnionSpans`.
- Task12F-E-C-B switches the real main overlay path to `template -> clipped spans -> row buckets -> per-row union`. It removes the `mainSpans` intermediate vector and disables the E-C-A double-path diagnostic by default. `RawMainSpans` now means actual clipped main template rows accepted into row buckets, matching `RowBucketInputSpans`.
- In the current P9 baseline, `TemplateInstantiatedRows` counts template rows surviving viewport Y preclip before X rejection, while `TemplateClippedRows` counts rows that survived final clipping and entered row buckets.

Optimization candidates:

- Keep overlay cache decision free of full `PhobosFog_LastVisibleFrames` scans. Reintroducing a cell-level temporal scan in the draw hot path is a known performance hazard.
- Task12C-A2 perf deltas should be used to identify high-churn touch reasons before any deduplication.
- Task12C-A3 overlay-effective deltas should be compared against raw deltas to verify that refresh churn no longer invalidates future overlay caches when the final visible/explored/unknown result is unchanged.
- Soft-edge remains a separate pass and disables the first cache version. A later phase can either fold soft-edge spans into the cached region or split it into its own safe cache.

## 10. Reveal Source Synchronization

### 10.1 Warhead Reveal

Source anchor:

- `src/Ext/WarheadType/Detonate.cpp`

Algorithm:

- For `Reveal > 0`, keep vanilla `RevealArea2` behavior and mirror affected cells into PhobosFog.
- Hold-frame priority:
  - per-warhead `PhobosFog.Warhead.RevealVisibleHoldFrames`;
  - `PhobosFog.WarheadReveal.VisibleHoldFrames`;
  - `PhobosFog.RevealSources.VisibleHoldFrames`;
  - default `0`.
- If hold is positive, mark affected radius visible until expiry.
- If hold is zero, mark affected radius explored.
- For `Reveal < 0`, full-map reveal uses full-map source hold priority.

Scenario:

- Warhead reveal and map reveal weapons.

Performance profile:

- Radius reveal is O(affected cells).
- Full-map reveal is O(valid map cells) only when marking explored; hard-visible full-map hold uses the house-level override.

Optimization candidates:

- Keep full-map visibility as an effective override rather than per-cell visible writes.
- Use stats only under debug.

### 10.2 Spy Plane Reveal

Source anchor:

- `src/Ext/Aircraft/Hooks.cpp`

Algorithm:

- Sync during spy plane approach and overfly phases.
- Throttle repeated sync per aircraft with a small slot table.
- Convert raw radius to a bounded cell spread.
- Positive hold marks spread visible until expiry.
- Zero hold marks spread explored.

Scenario:

- Spy plane camera reveal.

Performance profile:

- O(spread cells) per accepted sync.
- Throttling avoids repeated per-frame heavy updates for the same aircraft.

Optimization candidates:

- Keep the per-aircraft throttle.
- Avoid raising spread beyond vanilla reveal radius semantics.

### 10.3 Spy Satellite And Full-Map Reveal

Source anchors:

- `UpdatePhobosFogSpySatelliteState(...)`
- `MarkPhobosFogCrateFullMapReveal(...)`
- `MarkPhobosFogFullMapReveal(...)`

Algorithm:

- SpySat activation optionally marks all valid cells explored.
- Persistent SpySat treats every cell as effective hard visible without rewriting every cell.
- SpySat deactivation can extend a house-level full-map visible hold.
- Full-map reveal sources can mark all cells explored and optionally set full-map hard-visible hold.

Scenario:

- SpySat building activation.
- Reveal-map crate.
- Warhead full-map reveal.

Performance profile:

- Activation/full-map mark explored is O(valid map cells).
- Persistent visible and hold checks are O(1) during visibility query.

Optimization candidates:

- Keep full-map effects edge-triggered.
- Avoid scheduling full radar refresh repeatedly while full-map state is unchanged.

## 11. Diagnostics And Profiling Hooks

### 11.1 State Summary

Log prefix:

- `[PhobosFog] Frame=... DebugSummary`
- `[PhobosFog] House=... VectorSize=... Unknown=... Explored=... Visible=...`

Source anchors:

- `RefreshPhobosFogState()`
- `LogPhobosFogDebugSummary()`

Cadence:

- Every 900 frames when `PhobosFog.Debug=true`.

Use:

- Confirm state vector consistency.
- Confirm `Unknown + Explored + Visible == VectorSize`.
- Watch provider and visible mark counts.

### 11.2 RegionMask Summary

Log prefix:

- `[PhobosFog][RegionMask]`

Fields:

- `SourceCells`
- `RawMainSpans`
- `MergedMainSpans`
- `DilationEnabled`
- `DilationY`
- `DilationRawSpans`
- `HeightFaceEnabled`
- `HeightFaceDirections`
- `HeightFaceCandidates`
- `HeightFaceAccepted`
- `HeightFaceRawSpans`
- `HeightFaceMinDrop`
- `HeightFaceMaxDrop`
- `FallbackDilationY`
- `FallbackDilationRawSpans`
- `FinalMergedSpans`
- `ClosedGaps`
- `DrawRects`
- `MaxDrawRects`
- `SoftEdgeEnabled`
- `LegacyCliffCoverBypassed`
- `HitMaxDrawRects`

Use:

- Main overlay performance baseline.
- Detect span explosion.
- Detect draw-budget failures.

### 11.3 Independent Perf Summary

Log prefix:

- `[PhobosFog][Perf]`

Source anchor:

- `PhobosFogExploredOverlay::Draw()` in `src/Misc/Hooks.VeinholeMonster.cpp`

Runtime gates:

- `PhobosFog.Perf.Enabled=true`
- `PhobosFog.DrawExploredOverlay=true`
- `PhobosFog.Enabled=true`

Cadence:

- Every `PhobosFog.Perf.IntervalFrames` game frames, default `300`.
- Values `<= 0` are corrected to `300`.

Fields:

- `Frame`
- `ElapsedMs`
- `FrameDelta`
- `FPS`
- `OverlayDrawCalls`
- `SourceCells`
- `RawMainSpans`
- `MergedMainSpans`
- `PreCompactMainSpans`
- `CompactMainSpans`
- `CompactClosedGaps`
- `HeightFaceEnabled`
- `HeightFaceAccepted`
- `HeightFaceRawSpans`
- `HeightFaceMaxDrop`
- `FallbackDilationY`
- `FallbackDilationRawSpans`
- `FinalMergeInputSpans`
- `FinalMergedSpans`
- `ClosedGaps`
- `UnionSpans`
- `DrawRects`
- `DrawRectBatchReduction`
- `DrawRectScratchCapacity`
- `RowBucketInputSpans`
- `RowBucketNonEmptyRows`
- `RowBucketMainUnionSpans`
- `TemplateEnabled`
- `RectTemplateRows`
- `DiamondTemplateRows`
- `TemplateCandidateRows`
- `TemplateVisibleRows`
- `TemplateInstantiatedRows`
- `TemplateClippedRows`
- `TemplateYRejectedRows`
- `TemplateXRejectedRows`
- `TemplateFallbacks`
- `MaxDrawRects`
- `HitMaxDrawRects`
- `SoftEdgeEnabled`
- `LegacyCliffCoverBypassed`
- `PhobosFogStateVersion`
- `StateVersionDelta`
- `RawEffectiveVisibilityVersionHash`
- `RawEffectiveHashChanged`
- `OverlayEffectiveVersion`
- `OverlayEffectiveVersionDelta`
- `OverlayEffectiveVisibilityVersionHash`
- `OverlayEffectiveHashChanged`
- `OverlayEffectiveBatchTouchedCells`
- `OverlayEffectiveBatchChangedCells`
- `OverlayEffectiveTouchesSinceLastPerf`
- `OverlayViewportHash`
- `ViewportHashChanged`
- `OverlayConfigHash`
- `ConfigHashChanged`
- `OverlayCacheEnabled`
- `OverlayCacheAllowed`
- `OverlayCacheDisabledReason`
- `OverlayCacheHit`
- `OverlayCacheMiss`
- `OverlayCacheRebuild`
- `OverlayCacheHitsSinceLastPerf`
- `OverlayCacheMissesSinceLastPerf`
- `OverlayCacheRebuildsSinceLastPerf`
- `OverlayCacheHitStreak`
- `OverlayCacheCachedFinalSpans`
- `TemporalVisibilityActive`
- `StageAOnly`
- `StateTouchesSinceLastPerf`
- `TouchUnknown`
- `TouchReset`
- `TouchEnsureResize`
- `TouchDegradeVisibleToExplored`
- `TouchMarkExplored`
- `TouchMarkVisible`
- `TouchMarkVisibleUntil`
- `TouchMarkAreaVisible`
- `TouchMarkCellSpreadVisible`
- `TouchMarkAllExplored`
- `TouchMarkAllVisible`
- `TouchFullMapVisibleUntil`
- `TouchSpySatPersistentVisibleEdge`
- `TouchOther`

Use:

- Low-frequency tactical overlay profiling while `PhobosFog.Debug=false`.
- Avoid Alt+Tab or external profiler interaction during manual YR testing.
- Correlate FPS with current overlay span and draw-rect counts.
- Validate Task12C-A overlay cache dirty-key plumbing and Task12C-B single-entry cache reuse.
- Use Task12C-A2 touch reason deltas to identify whether dirty churn comes from normal sight refresh, temporary reveal hold, degrade, reset/resize, full-map hold, or SpySat edges.
- Use Task12C-B cache hit/miss/rebuild counters to confirm whether viewport movement, config changes, or overlay-effective version changes are invalidating reuse.
- Use Task12C2 compact span counters to confirm whether cache-miss rebuild cost is dominated by main union, compacted dilation input, height-face spans, or final merge input.
- After Task12C3-B, use `FinalMergeInputSpans` and `FinalMergedSpans` to measure the compact final path directly.

Performance rule:

- `PhobosFog.Perf.Enabled=false` must avoid log formatting and additional span traversal.
- Task12A records only the tactical explored-overlay draw path. Global frame, radar, and sight-refresh counters are deferred to later P9 tasks.

### 11.4 Radar Summary

Log prefix:

- `[PhobosFogRadar]`

Use:

- Confirm full vs dirty radar refresh traffic.
- Confirm visible/explored/unknown radar cell color application.
- Detect query failure rate.

### 11.5 Particle Probe Logs

Log prefixes:

- `[PhobosFogParticle]`
- `[PhobosFog][ParticleDrawHit]`

Use:

- Diagnose damaged-unit smoke and death smoke visibility leaks.
- Confirm whether `ParticleClass::Draw` and `ParticleSystemClass::Draw` are hit.
- Inspect owner resolution and fallback cell visibility.

Performance rule:

- Keep detailed particle logs disabled or capped. They are too verbose for normal play.

## 12. Performance Hotspot Ranking

This ranking is based on code shape and current runtime placement.

| Rank | Algorithm area | Why it is hot | First optimization direction |
|---:|---|---|---|
| 1 | Explored overlay viewport scan and span union | Runs every draw frame, emits many spans, sorts and coalesces them. | Cache per-frame viewport cell states and row centers; reduce raw span count. |
| 2 | Sight refresh | Iterates houses, all visible expiry cells, and every sight provider's range. | Sparse visible expiry list; preserve update interval. |
| 3 | Effective visibility ally merge | Used by almost every render/UI/radar gate. | Per-frame viewer/allied cell-state cache and no-ally fast path. |
| 4 | Particle owner analysis | Can run for many particles and systems. | Cache owner decisions; store metadata earlier. |
| 5 | Radar background override | Per radar cell refresh with ally merge. | Deduplicate dirty refresh queue and avoid visible-cell full scans. |
| 6 | Building foundation visibility | Repeated for building body, anims, radar, health, tooltip. | Per-building per-frame foundation visibility cache. |

## 13. Recommended Performance Optimization Order

1. Add measurement first:
   - Use `[PhobosFog][Perf]` for low-frequency overlay profiling when `PhobosFog.Debug=false`.
   - Use `[PhobosFog][RegionMask]` and `[PhobosFogRadar]` for debug-gated detail.
   - Add new counters behind either `PhobosFog.Debug=true` or a dedicated opt-in perf/probe gate.
2. Optimize overlay classification:
   - Build one padded-viewport classification buffer.
   - Store state, last-visible frame, screen center, and frontier directions.
3. Optimize span generation:
   - Reuse vectors.
   - Precompute diamond row widths.
   - Consider row buckets before full sort.
4. Optimize visibility queries:
   - Add a no-allies fast path.
   - Add per-frame effective-state cache if profiling proves ally merge cost.
5. Optimize sight refresh:
   - Replace full-vector degrade with sparse expiry tracking.
6. Optimize particle and building gates:
   - Cache per-frame owner/building visibility results.

## 14. Manual Profiling Presets

### 14.1 Overlay Baseline

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.Debug=false
PhobosFog.Perf.Enabled=true
PhobosFog.Perf.IntervalFrames=300
PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlaySoftEdge=false
PhobosFog.ExploredOverlayAlphaVariance=0
PhobosFog.ExploredOverlayFadeInFrames=0
PhobosFog.ExploredOverlayCliffCover=false
```

Use this to profile the main region union without soft-edge noise.

### 14.2 Overlay With Current Edge Polish

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.Debug=false
PhobosFog.Perf.Enabled=true
PhobosFog.Perf.IntervalFrames=300
PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlaySoftEdge=true
PhobosFog.ExploredOverlayAlphaVariance=4
PhobosFog.ExploredOverlayFadeInFrames=6
PhobosFog.ExploredOverlayCliffCover=false
```

Use this to compare final visual cost against the baseline.

### 14.3 Full Presentation Stress

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.Debug=true
PhobosFog.UpdateInterval=1
PhobosFog.HideEnemyFoot=true
PhobosFog.HideBuildings=true
PhobosFog.HideTiberiumSpawners=true
PhobosFog.HideWorldAnim=true
PhobosFog.HideWorldParticles=true
PhobosFog.HideHoverCursor=true
PhobosFog.HideHoverTooltip=true
PhobosFog.HideHoverHealthBar=true
PhobosFog.GateHiddenObjectCommands=true
PhobosFog.HideRadarObjects=true
PhobosFog.OverrideRadarFog=true
PhobosFog.DrawExploredOverlay=true
```

Use this only for stress testing. It intentionally enables many hot paths.
