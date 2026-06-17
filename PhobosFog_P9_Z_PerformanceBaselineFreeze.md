# PhobosFog P9-Z Performance Baseline Freeze

This document freezes the current P9 explored-overlay performance baseline for the PhobosFog development branch. It is a development handoff document, not final upstream user documentation.

## 1. Goal

The goal of this freeze is to record the accepted performance-oriented overlay architecture before entering final cleanup and targeted optimization work.

This document does not introduce new runtime behavior. It records the current implementation contract so later tasks can avoid accidentally restoring old experimental paths.

## 2. Frozen Baseline

The accepted P9 baseline is:

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

The active main overlay path no longer materializes the old `mainSpans` vector. Perf fields that still say `RawMainSpans` now mean clipped main template rows accepted into row buckets.

## 3. Active Cache Contract

The final-region cache is a single-entry cache. Its key uses:

- viewer identity;
- `OverlayEffectiveVisibilityVersionHash`;
- `OverlayViewportHash`;
- `OverlayConfigHash`.

The cache key intentionally does not use:

- raw `PhobosFog_StateVersion`;
- `RawEffectiveVisibilityVersionHash`;
- current frame;
- remaining temporary-visible hold frames.

Temporal full-map holds are handled by `TemporalCacheFirstInvalidFrame`. A matching cache entry may be reused until the first invalid frame is reached.

Cell-level `PhobosFog_LastVisibleFrames` are intentionally not scanned by overlay cache decision.

Save/load restores PhobosFog explored memory only. Loaded saves touch both raw and overlay-effective versions and request one forced fog refresh, but they do not serialize or restore the final-region cache.

## 4. Cache Disable Rules

The current cache is disabled when:

- `PhobosFog.ExploredOverlayFadeInFrames > 0`;
- active visible soft edge is enabled through `PhobosFog.ExploredOverlaySoftEdge`;
- the stage or viewer is invalid;
- the draw path hits `PhobosFog.ExploredOverlayMaxDrawRects` or otherwise cannot safely store the result.

Soft edge and fade-in still draw when enabled. These rules disable final-region cache reuse only.

## 5. Dormant Or Bypassed Paths

The legacy cliff-cover collector is intentionally bypassed:

```text
UseLegacyCliffCoverInRegionMaskPrototype=false
```

The active height coverage path is the C2 height-discontinuity face path. The old cliff probe and old cliff span collection must not be restored during performance cleanup unless a new task explicitly reopens that geometry model.

The old main span materialization path is also not the active baseline. Do not reintroduce `template -> mainSpans -> row-bucket union` as the real path unless an equivalence task explicitly requires it.

## 6. Perf Counter Semantics

Important current counter meanings:

- `SourceCells`: explored overlay source cells scanned in the padded viewport.
- `RawMainSpans`: clipped main template rows accepted into row buckets.
- `MergedMainSpans`: output spans from per-row union.
- `PreCompactMainSpans`: main union spans before close-gap compaction.
- `CompactMainSpans`: close-gap compacted main union.
- `FallbackDilationRawSpans`: fallback down-dilation spans generated from compact main union.
- `FinalMergeInputSpans`: compact main plus height face plus fallback dilation before final close-gap union.
- `FinalMergedSpans`: final region union span count.
- `UnionSpans`: union spans passed to `DrawUnionSpans`.
- `DrawRects`: actual `FillRectTrans(...)` draw calls after strict vertical rect batching.
- `TemplateInstantiatedRows`: template rows surviving viewport Y preclip before X rejection.
- `TemplateClippedRows`: rows that survived final clipping and entered row buckets.

`PhobosFog.Perf.Enabled` is independent from `PhobosFog.Debug`. Perf logging must not enable RegionMask, CliffProbe, or debug summaries.

## 7. Manual Baseline Preset

Use this preset to measure cacheable main-region performance:

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

Use the normal visual preset when checking final appearance. Expect cache to be disabled if soft edge or fade-in are active.

## 8. Cleanup Guardrails

Future cleanup may:

- simplify names around raw state versus overlay-effective state;
- remove stale failed-experiment comments and reports;
- consolidate perf field documentation;
- improve row-bucket scratch reuse if profiling proves it useful.

Future cleanup must not:

- change hook addresses or return paths;
- change overlay visual output without a visual task;
- re-enable legacy cliff cover;
- add full `LastVisibleFrames` scans to overlay cache decision;
- couple `PhobosFog.Perf.Enabled` to `PhobosFog.Debug`;
- treat `PhobosFog.HideBuildings` as total building erase.
- serialize overlay final-region cache, row buckets, geometry templates, or perf/debug counters.

## 9. Next Recommended Step

Proceed with semantic cleanup and naming cleanup in small phases. Keep each phase independently buildable and compare `[PhobosFog][Perf]` output before and after changes when touching the overlay draw hot path.
