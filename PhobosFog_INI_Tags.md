# PhobosFog INI Tags

This document is the live registry for PhobosFog INI tags in this development branch.

Maintenance rule:

- Whenever a PhobosFog INI tag is added, removed, renamed, or has its default, clamp, or behavior changed, update this document in the same task.
- Keep this document aligned with `RulesExt::ExtData` and `WarheadTypeExt::ExtData` declarations, defaults, INI reads, and serialization.
- This is a workstream registry, not the final upstream user documentation.

Current source anchors:

- `[General]` declaration/defaults: `src/Ext/Rules/Body.h`
- `[General]` INI loading/clamp: `src/Ext/Rules/Body.cpp`
- `[General]` serialization: `src/Ext/Rules/Body.cpp`
- `[WarheadType]` declaration/defaults: `src/Ext/WarheadType/Body.h`
- `[WarheadType]` INI loading/clamp: `src/Ext/WarheadType/Body.cpp`
- `[WarheadType]` serialization: `src/Ext/WarheadType/Body.cpp`

Current baseline snapshot:

- Default-off compatibility remains the primary rule. With `PhobosFog.Enabled=false`, PhobosFog should not change rendering, radar, UI, command, AI, reveal, or gameplay behavior.
- The current accepted runtime model uses house-level `Unknown`, `Explored`, and `Visible` cell state.
- Hard visibility checks merge the current player and valid allies.
- `PhobosFog.Perf.Enabled` is intentionally independent from `PhobosFog.Debug`; perf summaries can be emitted while debug summaries remain disabled.
- Enemy FootClass, live BuildingClass, world animations, world particles, tiberium-spawner terrain, hover UI, cursor presentation, radar dots, and hidden-object command dispatch are controlled by separate opt-in tags.
- Explored building snapshots may remain visible as last-known static information when `PhobosFog.HideBuildings=true`; building animations, hover UI, cursor, command, and health/pip presentation remain hard-`Visible` gated.
- `PhobosFog.HideBuildings` is live-building presentation gating. It is not a total building erase switch and does not suppress accepted explored building snapshot semantics.
- The explored-overlay final-rectangle cache supports main and soft-edge passes with or without fade-in. It reuses rectangles only while the local rendering inputs, including the actual faded alpha values, remain unchanged.
- Current manual acceptance notes: enemy FootClass hiding, allied visible merge, radar background override, reveal-source synchronization, current overlay edge behavior, command/cursor/tooltip gating, and explored building snapshot semantics have passed their latest manual checks unless a later phase reopens them.

## 1. Complete Tag Index

### 1.1 `[General]`

```ini
[General]
PhobosFog.Enabled=false                         ; boolean
PhobosFog.Debug=false                           ; boolean
PhobosFog.Perf.Enabled=false                    ; boolean
PhobosFog.Perf.IntervalFrames=300               ; integer, reset to 300 if <= 0
PhobosFog.UpdateInterval=30                      ; integer, minimum 1

PhobosFog.DrawExploredOverlay=false             ; boolean
PhobosFog.ExploredOverlayAlpha=96               ; integer, 0..255
PhobosFog.ExploredOverlayCellWidth=68           ; integer, reset to 68 if <= 0
PhobosFog.ExploredOverlayCellHeight=38          ; integer, reset to 38 if <= 0
PhobosFog.ExploredOverlayPaddingX=4             ; integer, minimum 0
PhobosFog.ExploredOverlayPaddingY=4             ; integer, minimum 0
PhobosFog.ExploredOverlayViewportPaddingCells=2 ; integer, 0..8
PhobosFog.ExploredOverlaySoftEdge=true          ; boolean
PhobosFog.ExploredOverlaySoftEdgeAlpha=20       ; integer, 0..255
PhobosFog.ExploredOverlaySoftEdgeVisibleAlpha=20 ; integer, 0..255
PhobosFog.ExploredOverlaySoftEdgeUnknownAlpha=0 ; integer, 0..255
PhobosFog.ExploredOverlayFadeInFrames=6         ; integer, 0..60
PhobosFog.ExploredOverlayUnknownMerge=true      ; boolean
PhobosFog.ExploredOverlayUnknownMergeAlpha=36   ; integer, 0..255
PhobosFog.ExploredOverlayUnknownMergePadding=18 ; integer, 0..64
PhobosFog.ExploredOverlaySoftEdgePadding=10     ; integer, 0..64
PhobosFog.ExploredOverlayAlphaVariance=4        ; integer, 0..64
PhobosFog.ExploredOverlayFrontierMode=8         ; integer, normalized to 4 or 8
PhobosFog.ExploredOverlayShape=1                ; integer, 0..2
PhobosFog.ExploredOverlayDiamondBandHeight=3    ; integer, 1..8
PhobosFog.ExploredOverlayMaxDrawRects=24000     ; integer, 1000..100000
PhobosFog.ExploredOverlayHeightAware=false      ; boolean
PhobosFog.ExploredOverlayHeightYOffset=0        ; integer, -256..256
PhobosFog.ExploredOverlayCliffCover=false       ; boolean
PhobosFog.ExploredOverlayCliffCoverHeight=48    ; integer, 0..256
PhobosFog.ExploredOverlayCliffCoverAlpha=96     ; integer, 0..255

PhobosFog.HideEnemyFoot=false                   ; boolean
PhobosFog.HideBuildings=false                   ; boolean
PhobosFog.HideTiberiumSpawners=false            ; boolean
PhobosFog.HideWorldAnim=false                   ; boolean
PhobosFog.HideWorldParticles=false              ; boolean

PhobosFog.HideHoverCursor=false                 ; boolean
PhobosFog.HideHoverTooltip=false                ; boolean
PhobosFog.HideHoverHealthBar=false              ; boolean
PhobosFog.GateHiddenObjectCommands=false        ; boolean
PhobosFog.GateAutoTargets=false                 ; boolean
PhobosFog.GateAutoFire=false                    ; boolean
PhobosFog.GateForceFireCells=false              ; boolean

PhobosFog.HideRadarObjects=false                ; boolean
PhobosFog.OverrideRadarFog=false                ; boolean

PhobosFog.SyncSpySatellite=false                ; boolean
PhobosFog.SpySatellite.MarkExplored=true        ; boolean
PhobosFog.SpySatellite.PersistentVisible=true   ; boolean
PhobosFog.SpySatellite.DeactivateHoldFrames=0   ; integer, 0..900
PhobosFog.SyncFullMapReveal=false               ; boolean
PhobosFog.FullMapReveal.MarkExplored=true       ; boolean
PhobosFog.FullMapReveal.VisibleHoldFrames=-1    ; integer, -1..900
PhobosFog.WarheadReveal.VisibleHoldFrames=-1    ; integer, -1..900
PhobosFog.SpyPlaneReveal.VisibleHoldFrames=-1   ; integer, -1..900
PhobosFog.RevealSources.VisibleHoldFrames=0     ; integer, 0..900
```

### 1.2 `[WarheadType]`

```ini
[SOMENAME] ; WarheadType
PhobosFog.Warhead.RevealVisibleHoldFrames=-1 ; integer, -1..900
```

### 1.3 `[WeaponType]`

```ini
[SOMENAME] ; WeaponType
PhobosFog.AllowForceFireExploredCells=false ; boolean
```

## 2. Global Control And State Refresh

| Tag | Field | Default | Use | Meaning and notes |
|---|---|---:|---|---|
| `PhobosFog.Enabled` | `PhobosFog_Enabled` | `false` | Master feature gate. | When `false`, PhobosFog should not change gameplay, rendering, radar, UI, command behavior, or runtime fog state behavior. |
| `PhobosFog.Debug` | `PhobosFog_Debug` | `false` | Debug logging gate. | Enables PhobosFog debug logs such as refresh summaries and source-specific debug output. It must not change gameplay or rendering behavior. |
| `PhobosFog.Perf.Enabled` | `PhobosFog_Perf_Enabled` | `false` | Independent performance logging gate. | Enables low-frequency `[PhobosFog][Perf]` logs even when `PhobosFog.Debug=false`. This is deliberately separate from debug logging; perf mode should not enable RegionMask, CliffProbe, or debug summaries by itself. |
| `PhobosFog.Perf.IntervalFrames` | `PhobosFog_Perf_IntervalFrames` | `300` | Performance logging cadence. | Controls how many game frames must pass between `[PhobosFog][Perf]` lines. Values `<= 0` are corrected to `300`. |
| `PhobosFog.UpdateInterval` | `PhobosFog_UpdateInterval` | `30` | Fog refresh cadence in frames. | Controls how often PhobosFog refreshes hard visibility from sight providers. Values `<= 0` are corrected to `1`. Lower values are more responsive but cost more CPU. Explicit INI values override the default (an existing `PhobosFog.UpdateInterval=3` still uses 3); forced refreshes still run immediately. Rendering continues every frame, independently of this logic cadence. |

## 3. Explored Overlay Rendering

The overlay caches final main and soft-edge rectangles, including with the default `PhobosFog.ExploredOverlayFadeInFrames=6`. Actual local cell kinds and faded alpha values decide whether those rectangles can be reused. Active fades are rechecked on the next frame; future visibility timestamps are rechecked when reached. Cache hits retain the main-before-edge blending order and shared rectangle budget. Performance-log `DrawRects` and `HitMaxDrawRects` include the edge pass.

The explored overlay draws only when both `PhobosFog.Enabled=true` and `PhobosFog.DrawExploredOverlay=true`.

A cheap house-state check includes immediate full-map visibility (such as Spy Satellite changes), not just the periodic fog refresh version. When it changes, the renderer compares the viewport source cells and a one-cell neighbor halo before rebuilding; unrelated off-screen movement does not by itself invalidate the final rectangles. Local geometry is still checked every draw, and those projections are reused during rebuilds. This avoids assuming that terrain height is immutable. Scenario teardown clears the cached rectangles.

This adds bounded geometry-validation work each frame. Units changing the visible frontier, camera scrolling, terrain changes and changing fade alpha can still require rebuilds; this is not a claim that all large-unit-count stalls are eliminated. `GeometryProbes` counts actual projection calls and `VisibilityQueries` counts per-house state/timestamp reads during the overlay draw. Compare these with existing cache hits/rebuilds and submitted `DrawRects`; they are operation counters, not stage timings.

| Tag | Field | Default | Use | Meaning and notes |
|---|---|---:|---|---|
| `PhobosFog.DrawExploredOverlay` | `PhobosFog_DrawExploredOverlay` | `false` | Enables the custom explored-area overlay pass. | This is the main visual gray/black fog overlay switch. |
| `PhobosFog.ExploredOverlayAlpha` | `PhobosFog_ExploredOverlayAlpha` | `96` | Base alpha. | Base opacity for explored overlay cells. Clamped to `0..255`. |
| `PhobosFog.ExploredOverlayCellWidth` | `PhobosFog_ExploredOverlayCellWidth` | `68` | Per-cell overlay width. | Screen-space width of the current overlay shape. Values `<= 0` reset to `68`. |
| `PhobosFog.ExploredOverlayCellHeight` | `PhobosFog_ExploredOverlayCellHeight` | `38` | Per-cell overlay height. | Screen-space height of the current overlay shape. Values `<= 0` reset to `38`. |
| `PhobosFog.ExploredOverlayPaddingX` | `PhobosFog_ExploredOverlayPaddingX` | `4` | Horizontal placement adjustment. | Extra X offset/padding used by the overlay draw pass. Negative values are corrected to `0`. |
| `PhobosFog.ExploredOverlayPaddingY` | `PhobosFog_ExploredOverlayPaddingY` | `4` | Vertical placement adjustment. | Extra Y offset/padding used by the overlay draw pass. Negative values are corrected to `0`. |
| `PhobosFog.ExploredOverlayViewportPaddingCells` | `PhobosFog_ExploredOverlayViewportPaddingCells` | `2` | Viewport overdraw margin. | Extra cells drawn around the visible viewport to reduce edge gaps. Clamped to `0..8`. |
| `PhobosFog.ExploredOverlaySoftEdge` | `PhobosFog_ExploredOverlaySoftEdge` | `true` | Enables soft-edge pass. | Adds softer frontier drawing around explored cells. The soft-edge pass can reuse cached final rectangles while its local rendering inputs remain unchanged. |
| `PhobosFog.ExploredOverlaySoftEdgeAlpha` | `PhobosFog_ExploredOverlaySoftEdgeAlpha` | `20` | Legacy soft-edge alpha. | Retained for compatibility with earlier PhobosFog overlay tuning. Current drawing prefers the visible/unknown alpha split below. Clamped to `0..255`. |
| `PhobosFog.ExploredOverlaySoftEdgeVisibleAlpha` | `PhobosFog_ExploredOverlaySoftEdgeVisibleAlpha` | `20` | Soft edge toward visible cells. | Alpha for explored frontier cells neighboring hard `Visible` cells. Clamped to `0..255`. |
| `PhobosFog.ExploredOverlaySoftEdgeUnknownAlpha` | `PhobosFog_ExploredOverlaySoftEdgeUnknownAlpha` | `0` | Soft edge toward unknown cells. | Alpha for explored frontier cells neighboring `Unknown` cells or outside-map neighbors. Default `0` avoids extra gray bloom at unknown borders. Clamped to `0..255`. |
| `PhobosFog.ExploredOverlayFadeInFrames` | `PhobosFog_ExploredOverlayFadeInFrames` | `6` | Visual fade-in duration. | Frames used to fade explored overlay in after a cell leaves hard `Visible`. `0` disables fade-in. Clamped to `0..60`. Positive durations retain their original per-frame alpha behavior; unchanged local alpha permits cache reuse, while a changing alpha invalidates the cached rectangles. |
| `PhobosFog.ExploredOverlayUnknownMerge` | `PhobosFog_ExploredOverlayUnknownMerge` | `true` | Enables unknown-boundary merge pass. | Draws an additional merge pass at unknown borders before the soft edge and main overlay passes. |
| `PhobosFog.ExploredOverlayUnknownMergeAlpha` | `PhobosFog_ExploredOverlayUnknownMergeAlpha` | `36` | Unknown merge alpha. | Alpha for the unknown-boundary merge pass. Clamped to `0..255`. |
| `PhobosFog.ExploredOverlayUnknownMergePadding` | `PhobosFog_ExploredOverlayUnknownMergePadding` | `18` | Unknown merge padding. | Extra screen-space padding for the unknown-boundary merge pass. Clamped to `0..64`. |
| `PhobosFog.ExploredOverlaySoftEdgePadding` | `PhobosFog_ExploredOverlaySoftEdgePadding` | `10` | Soft-edge padding. | Extra screen-space padding for the soft-edge pass. Clamped to `0..64`. |
| `PhobosFog.ExploredOverlayAlphaVariance` | `PhobosFog_ExploredOverlayAlphaVariance` | `4` | Stable visual breakup. | Stable per-cell alpha variance based on cell index. It should not shimmer frame to frame. Clamped to `0..64`. |
| `PhobosFog.ExploredOverlayFrontierMode` | `PhobosFog_ExploredOverlayFrontierMode` | `8` | Neighbor sampling mode. | Values `<= 4` become `4`; all other values become `8`. |
| `PhobosFog.ExploredOverlayShape` | `PhobosFog_ExploredOverlayShape` | `1` | Overlay shape mode. | `0` means rectangle, `1` means frontier diamond, `2` means diamond for all explored overlay cells. Clamped to `0..2`. |
| `PhobosFog.ExploredOverlayDiamondBandHeight` | `PhobosFog_ExploredOverlayDiamondBandHeight` | `3` | Diamond approximation detail. | Height of each horizontal band used to approximate diamond shapes. Clamped to `1..8`. |
| `PhobosFog.ExploredOverlayMaxDrawRects` | `PhobosFog_ExploredOverlayMaxDrawRects` | `24000` | Draw-call budget. | Per-frame cap for overlay rectangle draw calls. Clamped to `1000..100000`. |
| `PhobosFog.ExploredOverlayHeightAware` | `PhobosFog_ExploredOverlayHeightAware` | `false` | Height-aware anchoring. | Uses `CellClass::GetCellCoords()` for explored overlay screen anchoring so height, ramps, and levels can affect placement. Default `false` preserves the flat-cell anchor. |
| `PhobosFog.ExploredOverlayHeightYOffset` | `PhobosFog_ExploredOverlayHeightYOffset` | `0` | Height-aware Y adjustment. | Screen-space Y offset applied after height-aware or flat overlay anchoring. Negative values move the overlay up, positive values move it down. Clamped to `-256..256`. |
| `PhobosFog.ExploredOverlayCliffCover` | `PhobosFog_ExploredOverlayCliffCover` | `false` | Cliff-face cover pass. | Enables an extra screen-space cover pass for significant height-drop edges. It uses 4-neighbor height-drop checks and fills the projected strip between the high explored cell edge and the lower neighbor edge, instead of drawing a fixed full-cell skirt. The pass is terrain-driven and can cover explored-to-explored cliff faces, not only fog frontier edges. Small hills and gentle height changes are filtered out. |
| `PhobosFog.ExploredOverlayCliffCoverHeight` | `PhobosFog_ExploredOverlayCliffCoverHeight` | `48` | Maximum cliff-cover height. | Maximum projected strip depth in pixels for the cliff-face cover. The actual strip depth is based on the projected height difference between the high explored cell and the lower neighbor, with a small cliff-tile fallback for low-projection cliff faces. Clamped to `0..256`. |
| `PhobosFog.ExploredOverlayCliffCoverAlpha` | `PhobosFog_ExploredOverlayCliffCoverAlpha` | `96` | Cliff-cover face alpha. | Alpha used by significant height-drop face-cover spans. Clamped to `0..255`. |

## 4. Render-only World Hiding

All switches in this section require `PhobosFog.Enabled=true`. These are presentation/rendering filters and should not affect AI, damage, targeting logic, object ownership, or simulation state.

| Tag | Field | Default | Use | Meaning and notes |
|---|---|---:|---|---|
| `PhobosFog.HideEnemyFoot` | `PhobosFog_HideEnemyFoot` | `false` | Hides enemy moving objects. | Hides enemy `FootClass` objects when their cell is not hard `Visible` to the current player or valid allies. Does not hide allied or owned objects. |
| `PhobosFog.HideBuildings` | `PhobosFog_HideBuildings` | `false` | Gates live building presentation. | Hides live `BuildingClass` rendering unless the building foundation is hard `Visible`. This is not a total building erase switch: explored frozen/fogged building snapshots may still draw as last-known static information, while building animations, hover UI, cursor, command, and health/pip presentation remain hard-`Visible` gated. |
| `PhobosFog.HideTiberiumSpawners` | `PhobosFog_HideTiberiumSpawners` | `false` | Hides terrain/tiberium-spawner visuals. | Hides terrain or tiberium-spawner visuals when not hard `Visible`. Intended for terrain-like reveal leaks. |
| `PhobosFog.HideWorldAnim` | `PhobosFog_HideWorldAnim` | `false` | Hides map-space animations. | Hides map-space world animations when their map-space visibility is not hard `Visible`. |
| `PhobosFog.HideWorldParticles` | `PhobosFog_HideWorldParticles` | `false` | Hides world particles. | Hides world particles when their map-space visibility is not hard `Visible`. This is still render-only. |

## 5. UI Presentation And Command Gating

All switches in this section require `PhobosFog.Enabled=true`.

| Tag | Field | Default | Use | Meaning and notes |
|---|---|---:|---|---|
| `PhobosFog.HideHoverCursor` | `PhobosFog_HideHoverCursor` | `false` | Cursor presentation gating. | Forces tactical hover cursor presentation to default/move-style presentation when the hovered object is not hard `Visible` to the current player or valid allies. |
| `PhobosFog.HideHoverTooltip` | `PhobosFog_HideHoverTooltip` | `false` | Tooltip/name presentation gating. | Hides tactical object hover tooltip and object name text when the object is not hard `Visible` to the current player or valid allies. |
| `PhobosFog.HideHoverHealthBar` | `PhobosFog_HideHoverHealthBar` | `false` | Health bar presentation gating. | Hides hover-triggered or permanent tactical health bar UI when the techno is not hard `Visible` to the current player or valid allies. |
| `PhobosFog.GateHiddenObjectCommands` | `PhobosFog_GateHiddenObjectCommands` | `false` | Player command gating. | Downgrades clicks on non-allied hidden `TechnoClass` objects to ordinary cell clicks by clearing the command target object before vanilla command dispatch. Does not affect AI. |

## 6. Combat Target And Fire Gating

All switches in this section require `PhobosFog.Enabled=true`. These switches use attacker-owner visibility plus valid allied visibility, not local `CurrentPlayer` UI visibility.

| Tag | Field | Default | Use | Meaning and notes |
|---|---|---:|---|---|
| `PhobosFog.GateAutoTargets` | `PhobosFog_GateAutoTargets` | `false` | Auto target acquisition gating. | Rejects enemy live object candidates in `TechnoClass::CanAutoTargetObject` when the target cell or building foundation is not hard `Visible` to the attacker owner or valid allies. This does not clear missions, does not clear existing targets, and does not affect already launched projectiles. |
| `PhobosFog.GateAutoFire` | `PhobosFog_GateAutoFire` | `false` | Object fire-time safety gate. | In `TechnoClass::CanFire`, clears enemy live object targets and returns cannot-fire when the object is not hard `Visible` to the attacker owner or valid allies. Does not handle already launched projectiles, cell targets, or ground fire. |
| `PhobosFog.GateForceFireCells` | `PhobosFog_GateForceFireCells` | `false` | Cell target fire-time gate. | In `TechnoClass::CanFire`, rejects direct `CellClass*` and terrain-backed force-fire or ground-fire targets when the resolved cell is not hard `Visible` to the attacker owner or valid allies. `TechnoClass` and `BuildingClass` object targets are left to object fire-time gating. This does not add command-stage gating and does not clear missions or queues. |
| `[WeaponType] PhobosFog.AllowForceFireExploredCells` | `WeaponTypeExt::PhobosFog_AllowForceFireExploredCells` | `false` | Weapon-level explored-cell exception for force-fire gating. | Only applies when global `PhobosFog.GateForceFireCells=true`. For this weapon, direct `CellClass*` and terrain-backed force-fire targets are allowed when the resolved cell is `Explored` or `Visible` to the attacker owner or valid allies. `Unknown` cells remain blocked. This does not affect hidden object targets, auto target acquisition, object fire-time gating, projectiles, missions, or queues. |

## 7. Radar And Minimap

All switches in this section require `PhobosFog.Enabled=true`.

| Tag | Field | Default | Use | Meaning and notes |
|---|---|---:|---|---|
| `PhobosFog.HideRadarObjects` | `PhobosFog_HideRadarObjects` | `false` | Radar object dot hiding. | Hides enemy techno object dots on the radar/minimap when the object is not hard `Visible` to the current player or valid allies. Buildings use foundation visibility. |
| `PhobosFog.OverrideRadarFog` | `PhobosFog_OverrideRadarFog` | `false` | Radar background fog coloring. | Replaces radar/minimap background cell colors using PhobosFog state. `Visible` keeps vanilla color, `Explored` darkens vanilla color, and `Unknown` becomes black. |

## 8. Event-driven Reveal Sources

These tags synchronize vanilla or game-state reveal sources into PhobosFog state. They do not replace vanilla reveal behavior. They only mirror or augment PhobosFog cell state.

### 7.1 Source enable switches

| Tag | Field | Default | Use | Meaning and notes |
|---|---|---:|---|---|
| `PhobosFog.SyncSpySatellite` | `PhobosFog_SyncSpySatellite` | `false` | Enables SpySat synchronization. | When enabled, PhobosFog tracks `HouseClass::SpySatActive` and can mark all cells explored or treat all cells as effectively hard `Visible` while SpySat is active. |
| `PhobosFog.SpySatellite.MarkExplored` | `PhobosFog_SpySatellite_MarkExplored` | `true` | SpySat activation explored sync. | When SpySat becomes active, marks all valid cells for that house as at least `Explored`. Only used when `PhobosFog.SyncSpySatellite=true`. |
| `PhobosFog.SpySatellite.PersistentVisible` | `PhobosFog_SpySatellite_PersistentVisible` | `true` | SpySat persistent hard visibility. | Treats every cell as effective hard `Visible` while `SpySatActive` is true. This is a house-level effective-visibility override and does not rewrite the cell vector to all `Visible`. |
| `PhobosFog.SyncFullMapReveal` | `PhobosFog_SyncFullMapReveal` | `false` | Enables full-map reveal synchronization. | Synchronizes one-shot full-map reveal events such as Warhead `Reveal < 0` and reveal-map crates into PhobosFog. |
| `PhobosFog.FullMapReveal.MarkExplored` | `PhobosFog_FullMapReveal_MarkExplored` | `true` | Full-map reveal explored sync. | When a synchronized full-map reveal event fires, marks all valid cells for that house as at least `Explored`. |

### 7.2 Hold-frame tags

`-1` means inherit for source-specific tags. `0` means MarkExplored only with no temporary hard `Visible`. Positive values keep affected cells hard `Visible` for that many frames.

| Tag | Field | Default | Clamp | Use | Meaning and priority |
|---|---|---:|---|---|---|
| `PhobosFog.RevealSources.VisibleHoldFrames` | `PhobosFog_RevealSources_VisibleHoldFrames` | `0` | `0..900` | Global fallback for event-driven reveal sources. | Used only when a source-specific tag inherits with `-1`. This tag itself cannot inherit. Negative values are corrected to `0`. |
| `PhobosFog.WarheadReveal.VisibleHoldFrames` | `PhobosFog_WarheadReveal_VisibleHoldFrames` | `-1` | `-1..900` | Source-specific hold for Warhead `Reveal=`. | Used for Warhead `Reveal > 0` and as the first general fallback for Warhead `Reveal < 0`. `-1` inherits `PhobosFog.RevealSources.VisibleHoldFrames`. |
| `PhobosFog.SpyPlaneReveal.VisibleHoldFrames` | `PhobosFog_SpyPlaneReveal_VisibleHoldFrames` | `-1` | `-1..900` | Source-specific hold for spy plane reveal. | `-1` inherits `PhobosFog.RevealSources.VisibleHoldFrames`. `0` makes spy plane sync only mark the affected cell spread as `Explored`. |
| `PhobosFog.FullMapReveal.VisibleHoldFrames` | `PhobosFog_FullMapReveal_VisibleHoldFrames` | `-1` | `-1..900` | Source-specific hold for generic full-map reveal events. | `-1` inherits `PhobosFog.RevealSources.VisibleHoldFrames`. For Warhead `Reveal < 0`, this is used after the per-warhead and WarheadReveal settings. |
| `PhobosFog.SpySatellite.DeactivateHoldFrames` | `PhobosFog_SpySatellite_DeactivateHoldFrames` | `0` | `0..900` | Optional SpySat deactivation afterglow. | Explicit-only hold duration. When SpySat changes from active to inactive, this can keep all cells effectively hard `Visible` for N frames. It does not inherit from `PhobosFog.RevealSources.VisibleHoldFrames` and is never affected by the generic reveal-source fallback. |

### 7.3 Per-warhead override

| Tag | Field | Default | Clamp | Use | Meaning and priority |
|---|---|---:|---|---|---|
| `PhobosFog.Warhead.RevealVisibleHoldFrames` | `PhobosFog_Warhead_RevealVisibleHoldFrames` | `-1` | `-1..900` | Per-`WarheadType` override for that warhead's `Reveal=` PhobosFog sync. | Highest-priority Warhead reveal hold setting. For `Reveal > 0`, priority is per-warhead, then `PhobosFog.WarheadReveal.VisibleHoldFrames`, then `PhobosFog.RevealSources.VisibleHoldFrames`, then `0`. For `Reveal < 0`, priority is per-warhead, then `PhobosFog.WarheadReveal.VisibleHoldFrames`, then `PhobosFog.FullMapReveal.VisibleHoldFrames`, then `PhobosFog.RevealSources.VisibleHoldFrames`, then `0`. |

Example:

```ini
[General]
PhobosFog.RevealSources.VisibleHoldFrames=0
PhobosFog.WarheadReveal.VisibleHoldFrames=30
PhobosFog.SpyPlaneReveal.VisibleHoldFrames=300
PhobosFog.FullMapReveal.VisibleHoldFrames=450
PhobosFog.SpySatellite.DeactivateHoldFrames=150

[ScoutRevealWH]
Reveal=12
PhobosFog.Warhead.RevealVisibleHoldFrames=600

[MapPingRevealWH]
Reveal=8
PhobosFog.Warhead.RevealVisibleHoldFrames=0
```

## 8. Changed Or Rewritten PhobosFog Tags

This section records PhobosFog tags whose default value or meaning changed during the workstream. These are not upstream Phobos tags from `develop`; they are PhobosFog workstream tags that existed earlier in this branch and were redefined.

| Tag | Previous behavior | Current behavior |
|---|---|---|
| `PhobosFog.RevealSources.VisibleHoldFrames` | Default was `90`. It acted as the single hold duration for reveal-source synchronization. | Default is now `0`. It is now a global fallback used only when source-specific tags inherit. Explicitly setting it to `90` restores the earlier global hold duration. |
| `PhobosFog.FullMapReveal.VisibleHoldFrames` | Default was `0`. Negative values were corrected to `0`, so it could not inherit. | Default is now `-1`. `-1` inherits `PhobosFog.RevealSources.VisibleHoldFrames`; `0` means MarkExplored only; positive values keep full-map hard `Visible` temporarily. |
| `PhobosFog.ExploredOverlaySoftEdgeAlpha` | Used as the main soft-edge alpha in earlier overlay iterations. | Retained as a legacy compatibility/tuning tag. Current soft-edge behavior prefers `PhobosFog.ExploredOverlaySoftEdgeVisibleAlpha` and `PhobosFog.ExploredOverlaySoftEdgeUnknownAlpha`. |
| `PhobosFog.HideBuildings` | Earlier workstream iterations experimented with hiding live buildings and their explored/fogged snapshots together. | Live `BuildingClass` rendering remains hard-`Visible` gated, while explored/fogged building snapshots may draw as last-known static information. This keeps explored building silhouettes visible but suppresses live-only presentation leaks through separate hard-visible gates. |

## 9. Original Phobos Or Vanilla Tags Affected

No existing upstream Phobos INI tag is renamed, removed, or overwritten by PhobosFog at this stage. There are currently no rewritten upstream Phobos tags.

The following existing vanilla or game-facing behaviors are mirrored into PhobosFog state, but their original semantics are not rewritten:

| Existing tag or source | Original owner | PhobosFog interaction |
|---|---|---|
| `[WarheadType] Reveal` | Vanilla/YRpp warhead behavior, exposed through `WarheadTypeExt::ExtData::Reveal`. | PhobosFog observes the existing reveal result and synchronizes affected PhobosFog cells to `Explored` or temporary hard `Visible`, depending on the PhobosFog hold-frame tags. The vanilla reveal behavior remains intact. |
| `[WarheadType] CreateGap` | Vanilla/YRpp warhead behavior, exposed through `WarheadTypeExt::ExtData::CreateGap`. | Not modified by PhobosFog in this branch. Listed here because it was repeatedly called out as forbidden scope in earlier tasks. |
| Spy plane reveal behavior | Vanilla aircraft/spy-plane mission logic. | PhobosFog mirrors spy-plane revealed cells into PhobosFog state when enabled. It does not change vanilla spy-plane mission logic or hook addresses in this task. |
| Spy satellite active state | `HouseClass::SpySatActive`. | PhobosFog can treat all cells as effectively hard `Visible` while SpySat is active, and can optionally retain hard visibility after deactivation. It does not change the underlying SpySatActive state. |
| Reveal-map crate/full-map reveal events | Vanilla map reveal behavior. | PhobosFog can synchronize these events into PhobosFog state when `PhobosFog.SyncFullMapReveal=true`. The vanilla map reveal call remains intact. |

## 9.1 Workstream Feature Baseline

| Area | Current state | Main tags |
|---|---|---|
| Master gate and refresh | Implemented and disabled by default. Uses house-level `Unknown`, `Explored`, and `Visible` state with a configurable refresh cadence. | `PhobosFog.Enabled`, `PhobosFog.Debug`, `PhobosFog.UpdateInterval` |
| Explored overlay | Implemented as a custom render overlay with diamond-oriented coverage, optional soft edges, viewport padding, height-aware placement, and cliff-face cover. | `PhobosFog.DrawExploredOverlay`, `PhobosFog.ExploredOverlay*` |
| Enemy FootClass hiding | Implemented as render-only hiding when the object cell is not hard `Visible` to the current player or valid allies. | `PhobosFog.HideEnemyFoot` |
| Building presentation | Live building draw is hard-`Visible` gated. Explored/fogged snapshots may remain visible as last-known static information. Building animation, hover UI, cursor, command, and health/pip presentation are separately hard-`Visible` gated. | `PhobosFog.HideBuildings`, `PhobosFog.HideHoverHealthBar`, `PhobosFog.HideHoverCursor`, `PhobosFog.HideHoverTooltip`, `PhobosFog.GateHiddenObjectCommands` |
| World visual leak hiding | Map-space animation, particle, tiberium-spawner terrain, and ore/mining-device leak paths are controlled by opt-in render-only gates. | `PhobosFog.HideWorldAnim`, `PhobosFog.HideWorldParticles`, `PhobosFog.HideTiberiumSpawners` |
| Cursor, tooltip, health bar, and commands | Presentation and click-target gating are split from simulation logic. Hidden-object clicks are downgraded to ordinary cell commands when enabled. | `PhobosFog.HideHoverCursor`, `PhobosFog.HideHoverTooltip`, `PhobosFog.HideHoverHealthBar`, `PhobosFog.GateHiddenObjectCommands` |
| Combat target and fire gating | Auto-target candidates, object fire-time targets, and direct cell force-fire targets can be rejected when they are not hard `Visible` to the attacker owner or valid allies. Individual weapons can opt into explored-cell force-fire while still blocking unknown cells. | `PhobosFog.GateAutoTargets`, `PhobosFog.GateAutoFire`, `PhobosFog.GateForceFireCells`, `[WeaponType] PhobosFog.AllowForceFireExploredCells` |
| Radar/minimap | Enemy radar dots can be hidden by hard visibility. Radar background colors can be replaced from PhobosFog state. | `PhobosFog.HideRadarObjects`, `PhobosFog.OverrideRadarFog` |
| Reveal source integration | Warhead reveal, spy-plane reveal, SpySat, and full-map reveal events can be mirrored into PhobosFog state with source-specific hold-frame controls. | `PhobosFog.SyncSpySatellite`, `PhobosFog.SyncFullMapReveal`, `PhobosFog.*VisibleHoldFrames`, `PhobosFog.Warhead.RevealVisibleHoldFrames` |

## 10. Serialization Status

All INI-backed `RulesExt::ExtData` fields listed in this document are included in `RulesExt::ExtData::Serialize(...)`.

The per-warhead tag `PhobosFog.Warhead.RevealVisibleHoldFrames` is included in `WarheadTypeExt::ExtData::Serialize(...)`.

Runtime-only fog state fields are intentionally not listed as INI tags:

- `HouseExt::ExtData::PhobosFog_CellStates`
- `HouseExt::ExtData::PhobosFog_LastVisibleFrames`
- `HouseExt::ExtData::PhobosFog_LastRefreshProviders`
- `HouseExt::ExtData::PhobosFog_LastRefreshVisibleCells`
- `HouseExt::ExtData::PhobosFog_LastRefreshResized`
- `HouseExt::ExtData::PhobosFog_FullMapVisibleUntilFrame`
- `HouseExt::ExtData::PhobosFog_LastSpySatActive`
- `HouseExt::ExtData::PhobosFog_LastFullMapHardVisible`

`PhobosFog_LastVisibleFrames` is a transient visual timing cache used by overlay fade-in and is not serialized.

`PhobosFog_FullMapVisibleUntilFrame`, `PhobosFog_LastSpySatActive`, and `PhobosFog_LastFullMapHardVisible` are runtime-only full-map source integration state. Save/load behavior for temporary full-map visibility should be revisited if the feature needs exact persistence across saves.

## 11. Diagnostic Presets

Use this preset when debugging hard visibility invalidation:

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.UpdateInterval=1
PhobosFog.Debug=true

PhobosFog.HideEnemyFoot=true
PhobosFog.HideBuildings=true
PhobosFog.HideTiberiumSpawners=true
PhobosFog.HideWorldAnim=true
PhobosFog.HideWorldParticles=true
PhobosFog.HideHoverCursor=true
PhobosFog.HideHoverTooltip=true
PhobosFog.HideHoverHealthBar=true
PhobosFog.HideRadarObjects=true
PhobosFog.OverrideRadarFog=true
PhobosFog.GateHiddenObjectCommands=true
PhobosFog.GateAutoTargets=true
PhobosFog.GateAutoFire=true
PhobosFog.GateForceFireCells=true

PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlayAlpha=96
PhobosFog.ExploredOverlayCellWidth=68
PhobosFog.ExploredOverlayCellHeight=38
PhobosFog.ExploredOverlayPaddingX=4
PhobosFog.ExploredOverlayPaddingY=4
PhobosFog.ExploredOverlayViewportPaddingCells=2

PhobosFog.ExploredOverlaySoftEdge=false
PhobosFog.ExploredOverlayAlphaVariance=0
PhobosFog.ExploredOverlayShape=0
PhobosFog.ExploredOverlayFadeInFrames=0
```

Use this preset when evaluating current overlay visuals:

```ini
[General]
PhobosFog.Enabled=true
PhobosFog.UpdateInterval=30

PhobosFog.DrawExploredOverlay=true
PhobosFog.ExploredOverlayAlpha=96
PhobosFog.ExploredOverlayCellWidth=68
PhobosFog.ExploredOverlayCellHeight=38
PhobosFog.ExploredOverlayPaddingX=4
PhobosFog.ExploredOverlayPaddingY=4
PhobosFog.ExploredOverlayViewportPaddingCells=2

PhobosFog.ExploredOverlaySoftEdge=true
PhobosFog.ExploredOverlaySoftEdgeVisibleAlpha=20
PhobosFog.ExploredOverlaySoftEdgeUnknownAlpha=0
PhobosFog.ExploredOverlayUnknownMerge=true
PhobosFog.ExploredOverlayUnknownMergeAlpha=36
PhobosFog.ExploredOverlayUnknownMergePadding=18
PhobosFog.ExploredOverlaySoftEdgePadding=10
PhobosFog.ExploredOverlayAlphaVariance=4
PhobosFog.ExploredOverlayFrontierMode=8
PhobosFog.ExploredOverlayFadeInFrames=6

PhobosFog.ExploredOverlayShape=1
PhobosFog.ExploredOverlayDiamondBandHeight=3
PhobosFog.ExploredOverlayMaxDrawRects=24000
PhobosFog.ExploredOverlayHeightAware=false
PhobosFog.ExploredOverlayHeightYOffset=0
PhobosFog.ExploredOverlayCliffCover=false
PhobosFog.ExploredOverlayCliffCoverHeight=48
PhobosFog.ExploredOverlayCliffCoverAlpha=96
```
