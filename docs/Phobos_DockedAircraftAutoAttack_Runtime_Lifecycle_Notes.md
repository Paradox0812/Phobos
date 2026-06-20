# DockedAircraftAutoAttack Runtime Lifecycle Notes

## Purpose

This document records the Phase 1 runtime lifecycle findings for DockedAircraftAutoAttack. It is a developer note, not a new user-facing feature contract.

Phase 1 plus `DockedAircraftAutoAttack.RequireVisibleTarget` is now closed as a manually validated baseline. R4.3 NotifyUnlink dispatch, R5 diagnostics cleanup, and RequireVisibleTarget V1/V2/V3 have all been completed and manually tested.

TargetWeight TW1 through TW4 is also complete. It adds scan-time target priority only and does not change the dock release or re-dispatch lifecycle.

## Why Docked Aircraft Are Special

Airport-bound aircraft can sit in `Mission::Sleep` while still linked to their dock building through the radio system. A normal target assignment is not enough to make every docked aircraft leave the pad. The dock link can keep the aircraft physically parked even when the queued mission changes to attack.

The player-command path releases the dock relationship before the aircraft starts its attack flow. Phase 1 mirrors that critical part by sending `RadioCommand::NotifyUnlink` before automatic dispatch.

## Dispatch Sequence

The automatic dispatch path is intentionally narrow:

1. Validate that the feature, AircraftType settings, ammo, range, target relation, visibility policy, and projectile category are acceptable.
2. Resolve the selected weapon slot from `DockedAircraftAutoAttack.WeaponOrder`.
3. Send `RadioCommand::NotifyUnlink` through the existing first radio link.
4. Wake docked `Mission::Sleep` aircraft with `EnterIdleMode(false, true)` when needed.
5. Set the feature state and runtime target marker.
6. Call `SetDestination(target, false)`.
7. Queue `Mission::Attack`.
8. Call `SetTarget(target)`.

This sequence does not manually clear the dock link, `DockNowHeadingTo`, `Target`, or `ArchiveTarget`.

## Overlay State

DockedAircraftAutoAttack keeps a small runtime overlay in `TechnoExt`:

- `DockedAircraftAutoAttack_State`
- `DockedAircraftAutoAttack_Target`
- `DockedAircraftAutoAttack_LastDispatchFrame`

The overlay is used to distinguish feature-owned dispatch and reload waiting from unrelated vanilla aircraft orders. It does not replace the vanilla mission system.

## Target Ownership And Safety

The runtime target marker is treated as feature-owned only when the aircraft current `Target` matches `DockedAircraftAutoAttack_Target`.

Before reusing the marker, Phase 1 searches `TechnoClass::Array` to confirm that the pointer still identifies a live techno object in the current object array. If the marker target cannot be found, the feature clears its own marker and does not re-dispatch.

`DockedAircraftAutoAttack.RequireVisibleTarget` is part of target validity. It is checked during normal scan and again before LockedReloading re-dispatch. If the target changes from Visible to Explored or Unknown while the Visible requirement is active, the target is treated as invalid and the auto attack marker is cleared.

The feature only clears `Target` when it owns the target marker. Manual player retargeting should be treated as an override and should clear only the feature overlay.

This visibility policy does not affect player manual attack orders, normal weapon firing, ordinary Guard behavior, or PhobosFog state refresh.

## Visibility Policy

`DockedAircraftAutoAttack.RequireVisibleTarget` is an AircraftType-level target selection policy for docked auto attack only.

- When `PhobosFog.Enabled=false`, the policy does not block targets.
- When `DockedAircraftAutoAttack.RequireVisibleTarget=false`, the AircraftType ignores the PhobosFog Visible requirement for docked auto attack.
- When `PhobosFog.Enabled=true` and `DockedAircraftAutoAttack.RequireVisibleTarget=true`, the target cell must be hard-visible to the aircraft owner or allied vision.
- Building targets are accepted if any foundation cell is visible. If foundation information is unavailable, the current building cell is used.

The owner plus allied visibility query uses `HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies` and does not modify PhobosFog implementation files.

## TargetWeight Selection Layer

`DockedAircraftAutoAttack.TargetWeight.*` is a scan-time selection layer. It runs after the existing legality filters and before the first automatic dispatch.

The filtering order remains:

1. Basic target validity.
2. Enemy, neutral, and allied relation checks.
3. `DockedAircraftAutoAttack.Range`.
4. `DockedAircraftAutoAttack.RequireVisibleTarget` and PhobosFog visibility policy.
5. Projectile `AA`/`AG` compatibility.
6. Weapon slot availability from `DockedAircraftAutoAttack.WeaponOrder`.
7. TargetWeight scoring.

TargetWeight does not bypass any legality filter. A high-weight target that is out of range, invisible under the active visibility policy, allied, neutral, or projectile-incompatible is rejected before scoring.

When all TargetWeight values are 0, the scan keeps the Phase 1 first-accepted target selection behavior. When any TargetWeight value is greater than 0, accepted targets are scored by `score = categoryWeight`.

The tie-break rule is deterministic:

1. Higher score wins.
2. If score is equal, the nearer target wins.
3. If score and distance are both equal, the first accepted target is kept.

TargetWeight does not modify the dock link, `RadioCommand::NotifyUnlink`, wakeup, destination, mission queueing, re-dispatch cooldown, marker target safety, or PhobosFog state refresh.

Once the aircraft enters `Dispatching` or `LockedReloading`, the feature continues to track the existing marker target. It does not redirect the aircraft to a newly appeared higher-priority target during reload. `RetargetOnHigherPriority` is not supported.

## Serialization Boundary

The target marker is runtime-only and is not serialized as a pointer. This avoids persisting a raw object pointer across save/load boundaries.

Phase 1 serializes persistent configuration and simple runtime fields where the existing extension lifecycle requires it, but it does not attempt to restore an in-flight auto attack target from a save.

## Diagnostics

R5 keeps temporary diagnostics in source for future investigation, but both compile-time gates are false by default:

```cpp
DockedAircraftAutoAttackDiag = false
DockedAircraftAutoAttackLifecycleDiag = false
```

Default builds should not write DockedAircraftAutoAttack diagnostic spam to `debug.log`.

Manual testing confirmed the R5 stabilization goal: ordinary Phase 1 validation can run without the earlier high-volume DockedAircraftAutoAttack diagnostics enabled by default.

## Validated Phase 1 Scenarios

The current manually validated baseline covers these scenarios:

- Docked aircraft can auto-dispatch from an airport to attack a valid enemy target.
- Automatic dispatch releases the dock link through `RadioCommand::NotifyUnlink`.
- Aircraft can return to dock, reload, and re-dispatch while the remembered target remains valid.
- R5 diagnostic cleanup keeps DockedAircraftAutoAttack logging disabled by default.
- `DockedAircraftAutoAttack.RequireVisibleTarget=true` does not block valid targets when `PhobosFog.Enabled=false`.
- With `PhobosFog.Enabled=true` and `DockedAircraftAutoAttack.RequireVisibleTarget=true`, only Visible targets trigger docked auto attack. Explored and Unknown targets do not trigger it.
- With `PhobosFog.Enabled=true` and `DockedAircraftAutoAttack.RequireVisibleTarget=false`, that AircraftType ignores the PhobosFog Visible requirement for docked auto attack while retaining range, relation, ammo, weapon slot, and projectile compatibility checks.
- LockedReloading re-dispatch validates the remembered marker target through the same target safety and visibility policy.
- TargetWeight keeps first-accepted selection when all category weights are 0.
- TargetWeight selects the highest-scoring accepted target when category weights are configured, with distance as a deterministic tie-breaker.
- TargetWeight does not retarget during LockedReloading.

## Future Boundaries

Future phases should keep these boundaries separate:

- Target weighting: completed as scan-time target selection priority only.
- `AircraftWeaponAmmo`: separate ammo storage and UI contract only.
- Deploy switching: only after a dedicated deploy-state contract.

These phases should not remove the NotifyUnlink dispatch step unless a replacement is verified against the player-command airport aircraft path.
