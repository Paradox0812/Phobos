# DockedAircraftAutoAttack Phase 1 Handoff

## Phase Goal

DockedAircraftAutoAttack Phase 1 establishes a minimal baseline where configured aircraft can leave a dock building to attack nearby enemy targets, return to dock, reload, and re-dispatch while the remembered target is still valid.

R5 is a stabilization-only pass. It does not add behavior. It only disables temporary diagnostics by default and updates documentation.

Phase 1 plus `DockedAircraftAutoAttack.RequireVisibleTarget` is now closed as a manually validated baseline. R4.3 NotifyUnlink dispatch, R5 diagnostics cleanup, and RequireVisibleTarget V1/V2/V3 are complete.

## Phase Timeline

- Phase 1A fields-only: added global, AircraftType, and runtime fields with defaults, INI loading, cleanup, and serialization where required.
- Phase 1B minimal loop: added the first docked aircraft scan and attack dispatch path.
- Range semantics fix: made `DockedAircraftAutoAttack.Range` the dedicated scan radius in cells instead of relying on weapon range.
- Projectile filter fix: added projectile `AA`/`AG` coarse target-category filtering while keeping `GetFireError` diagnostic-only during scan.
- Mission dispatch fix: adjusted basic attack dispatch ordering for script aircraft.
- Runtime marker fields: added the runtime-only state, target marker, and last dispatch frame fields.
- LockedReloading behavior: allowed the feature-owned marker target to survive reload wait and trigger re-dispatch when ammo is restored.
- Marker target safety fix: validates the runtime target marker through `TechnoClass::Array` before using it.
- ReDispatch wakeup cooldown: added a re-dispatch cooldown and wakeup attempt for docked `Mission::Sleep` aircraft.
- NotifyUnlink dispatch: added `RadioCommand::NotifyUnlink` before automatic attack dispatch so airport-bound aircraft release the dock link through the normal radio path.
- R5 stabilization: disables DockedAircraftAutoAttack diagnostics by default and records the Phase 1 runtime lifecycle notes.
- RequireVisibleTarget V1: added the AircraftType-level `DockedAircraftAutoAttack.RequireVisibleTarget=true` field, default, INI loading, and serialization.
- RequireVisibleTarget V2: connected PhobosFog visibility filtering to normal scan and LockedReloading marker target validation without modifying PhobosFog implementation files.
- RequireVisibleTarget V3: updated user documentation, handoff notes, and runtime lifecycle notes.

All items above through RequireVisibleTarget V3 are considered complete for Phase 1 handoff. Manual testing confirmed the R4.3 NotifyUnlink dispatch path, the R5 diagnostic cleanup default behavior, and the RequireVisibleTarget V1/V2/V3 configuration and behavior path.

## Modified Files

- `src/Ext/Rules/Body.h` and `src/Ext/Rules/Body.cpp`: global configuration fields, defaults, INI loading, cleanup, and serialization for DockedAircraftAutoAttack.
- `src/Ext/TechnoType/Body.h` and `src/Ext/TechnoType/Body.cpp`: AircraftType-level fields, defaults, INI loading, cleanup, and serialization, including `DockedAircraftAutoAttack.RequireVisibleTarget`.
- `src/Ext/Techno/Body.h` and `src/Ext/Techno/Body.cpp`: runtime fields for scan timing, deploy-disable state, feature state, target marker, and last dispatch frame.
- `src/Ext/Aircraft/Body.cpp`: minimal scan and dispatch loop, range semantics, projectile AA/AG filtering, neutral/allied filtering, RequireVisibleTarget filtering, LockedReloading overlay, safe marker target lookup, NotifyUnlink dispatch, and compile-time diagnostic gates.
- `docs/New-or-Enhanced-Logics.md`: user-facing Phase 1 documentation.
- `docs/Phobos_DockedAircraftAutoAttack_Phase1_Handoff.md`: this handoff.
- `docs/Phobos_DockedAircraftAutoAttack_Runtime_Lifecycle_Notes.md`: developer notes for the docked aircraft lifecycle.

## New INI Tags

No new INI tags were added by V3 docs.

Phase 1 currently uses these tags:

```ini
[General]
DockedAircraftAutoAttack=false
DockedAircraftAutoAttack.Interval=15

[AircraftType]
DockedAircraftAutoAttack=false
DockedAircraftAutoAttack.Range=0
DockedAircraftAutoAttack.Interval=-1
DockedAircraftAutoAttack.MinAmmo=1
DockedAircraftAutoAttack.RequireVisibleTarget=true
DockedAircraftAutoAttack.WeaponOrder=0,1
DockedAircraftAutoAttack.DisableOnDeploy=false
```

## Current Behavior

- The feature is disabled by default globally and per AircraftType.
- A docked aircraft only participates when the global tag and its AircraftType tag are both enabled.
- `DockedAircraftAutoAttack.Range` is the docked auto attack scan radius in cells. It is not weapon range.
- `DockedAircraftAutoAttack.Interval` is the scan interval in frames. It is not an attack cooldown or reload control.
- `DockedAircraftAutoAttack.MinAmmo` uses vanilla single `TechnoClass::Ammo`; Phase 1 does not add independent aircraft weapon ammo.
- `DockedAircraftAutoAttack.RequireVisibleTarget` is true by default and only affects docked auto attack target selection.
- When PhobosFog is disabled, `DockedAircraftAutoAttack.RequireVisibleTarget` does not block targets.
- When PhobosFog is enabled and `DockedAircraftAutoAttack.RequireVisibleTarget=true`, only targets in cells hard-visible to the aircraft owner or allied vision are accepted.
- When PhobosFog is enabled and `DockedAircraftAutoAttack.RequireVisibleTarget=false`, the AircraftType ignores the PhobosFog Visible requirement for docked auto attack.
- Explored and Unknown cells do not trigger docked auto attack when the Visible requirement is active.
- Building targets are accepted if any covered foundation cell is visible. If foundation information is unavailable, the building's current cell is used as a fallback.
- `DockedAircraftAutoAttack.WeaponOrder` selects weapon slot 0 and 1 test order only.
- The scan loop accepts valid enemy targets inside `Range` with an available weapon slot from `WeaponOrder`.
- Neutral and allied targets are rejected.
- Projectile `AA`/`AG` flags are used as a coarse target-category filter.
- `GetFireError` is retained as diagnostic information and is not a hard scan rejection.
- LockedReloading and marker target re-dispatch validation reuse the same visibility requirement. If the remembered target leaves Visible state while `DockedAircraftAutoAttack.RequireVisibleTarget=true`, the target becomes invalid and the feature marker is cleared.
- Automatic dispatch sends `RadioCommand::NotifyUnlink` before setting destination, queuing `Mission::Attack`, and setting target.
- Team aircraft, airstrikes, and spawned aircraft are not intentionally taken over by the feature.
- Temporary diagnostics remain in the source but are gated behind compile-time constants that are false by default, so default builds should not spam `debug.log`.

## Validation Status

Manual testing has passed for the current Phase 1 baseline:

- Docked aircraft can automatically leave the airport and attack a valid target.
- The dock link is released through NotifyUnlink.
- The aircraft can return to dock, reload, and re-dispatch when the remembered target remains valid.
- The R4.3 NotifyUnlink test path behaved normally before R5 diagnostics cleanup.
- R5 diagnostic cleanup leaves DockedAircraftAutoAttack diagnostics disabled by default, so normal builds should not spam `debug.log`.
- With `PhobosFog.Enabled=false`, `DockedAircraftAutoAttack.RequireVisibleTarget=true` does not prevent valid Range targets from triggering auto attack.
- With `PhobosFog.Enabled=true` and `DockedAircraftAutoAttack.RequireVisibleTarget=true`, Visible targets trigger auto attack while Explored and Unknown targets do not.
- With `PhobosFog.Enabled=true` and `DockedAircraftAutoAttack.RequireVisibleTarget=false`, the AircraftType ignores the Visible requirement but keeps the other filters.
- LockedReloading and marker target re-dispatch honor the same RequireVisibleTarget policy.

Additional regression coverage is still recommended before expanding the feature:

- Ground and air target cases with different projectile `AA`/`AG` combinations.
- Neutral and allied targets inside `Range` are not attacked.
- Out-of-range targets are not selected.
- Insufficient ammo prevents scan and dispatch.
- `WeaponOrder=0,1` and `WeaponOrder=1,0` choose usable slots as expected.
- Disabling `[General] -> DockedAircraftAutoAttack` restores baseline behavior.
- Default Debug builds do not flood `debug.log` with DockedAircraftAutoAttack diagnostics.

## Not Implemented

- No new hook.
- No PhobosFog implementation file changes.
- No target weight system.
- No dual ammo or `AircraftWeaponAmmo`.
- No UI or cameo ammo display.
- No deploy-toggle behavior beyond the existing reserved runtime field.
- No docked in-place firing.
- No Team, Airstrike, or Spawned aircraft takeover.

## Runtime Notes

- Airport-bound aircraft are special because a docked aircraft remains linked to the dock building through the radio system.
- Queueing `Mission::Attack` alone can leave the aircraft in `Mission::Sleep` with the dock link still present.
- NotifyUnlink is the key behavior that mirrors the player-command path closely enough for the dock to release the aircraft.
- `DockedAircraftAutoAttack.RequireVisibleTarget` is target validity policy only. It does not affect manual attacks, normal weapon firing, ordinary Guard behavior, aircraft Sight, map reveal, or PhobosFog state refresh.
- Owner plus allied visibility uses `HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies`.
- The feature state is an overlay on top of vanilla aircraft missions. It tracks whether the feature is dispatching or waiting for reload without replacing the base mission system.
- The feature-owned target marker is runtime-only and is not serialized as a pointer. It is validated through `TechnoClass::Array` before reuse.
- The feature only clears `Target` when it owns that target marker and the marker has become invalid.

## Rollback Path

- Runtime rollback: set `[General] -> DockedAircraftAutoAttack=false`.
- AircraftType rollback: set `DockedAircraftAutoAttack=false` on the affected AircraftType.
- R5-only rollback: restore the diagnostic compile-time gates and revert the documentation changes.
- Full feature rollback: revert the Phase 1 field, serialization, scan, state overlay, and dispatch changes as a separate reviewed change.

## Suggested Next Phases

- Phase 1E: design TargetWeight only after a separate contract is approved.
- Phase 1F: decide whether to remove temporary diagnostics entirely or keep a formal developer-only diagnostic gate.
- Phase 2A: draft the `AircraftWeaponAmmo` contract only after a separate ammo-system contract is approved.
