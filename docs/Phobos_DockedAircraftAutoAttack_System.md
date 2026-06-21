# DockedAircraftAutoAttack System

## Overview

DockedAircraftAutoAttack lets docked AircraftTypes periodically scan for enemy targets and leave their dock to attack through the normal aircraft attack mission flow.

The system is an airport-aircraft auto attack helper. It is not a general aircraft scheduler, not a dock slot reservation system, and not an ammo or reload replacement.

## Design Goals

The implemented system is intentionally narrow:

1. Keep the aircraft docked until a valid target is found.
2. Use a configurable scan interval and scan radius.
3. Respect vanilla ammo, weapon, projectile, ownership, visibility, and dock lifecycle constraints.
4. Dispatch the aircraft through the existing attack mission path.
5. Allow target category preferences through TargetWeight after all legality filters pass.

## INI Tags

In `rulesmd.ini`:

```ini
[General]
DockedAircraftAutoAttack=false       ; boolean
DockedAircraftAutoAttack.Interval=15 ; integer, frames

[SOMEAIRCRAFT]                               ; AircraftType
DockedAircraftAutoAttack=false               ; boolean
DockedAircraftAutoAttack.Range=0             ; integer, cells
DockedAircraftAutoAttack.Interval=-1         ; integer, frames, -1 to use [General] -> DockedAircraftAutoAttack.Interval
DockedAircraftAutoAttack.MinAmmo=1           ; integer
DockedAircraftAutoAttack.WeaponOrder=0,1     ; list of integers
DockedAircraftAutoAttack.RequireVisibleTarget=true ; boolean
DockedAircraftAutoAttack.TargetWeight.Aircraft=0   ; integer
DockedAircraftAutoAttack.TargetWeight.Vehicle=0    ; integer
DockedAircraftAutoAttack.TargetWeight.Infantry=0   ; integer
DockedAircraftAutoAttack.TargetWeight.Building=0   ; integer
DockedAircraftAutoAttack.TargetWeight.Defense=0    ; integer
DockedAircraftAutoAttack.TargetWeight.Power=0      ; integer
DockedAircraftAutoAttack.TargetWeight.Factory=0    ; integer
DockedAircraftAutoAttack.DisableOnDeploy=false     ; boolean, reserved
```

`DockedAircraftAutoAttack.Range` is the scan radius for automatic target selection. It is not weapon range.

`DockedAircraftAutoAttack.Interval` controls scan cadence only. It is not a firing cooldown and does not change reload timing.

`DockedAircraftAutoAttack.MinAmmo` uses the vanilla single `TechnoClass::Ammo` value.

`DockedAircraftAutoAttack.WeaponOrder` supports weapon slots `0` and `1`. Invalid or duplicate entries are removed.

`DockedAircraftAutoAttack.DisableOnDeploy` is a reserved field. The deploy-toggle behavior is not implemented in this branch.

## Runtime Behavior

The feature only runs when:

1. `[General] -> DockedAircraftAutoAttack=true`.
2. The AircraftType has `DockedAircraftAutoAttack=true`.
3. The aircraft is alive, on-map, not in limbo, not an airstrike aircraft, not a spawned aircraft, and not assigned to a team that should own the aircraft's current behavior.
4. The aircraft is docked through a valid dock radio link.
5. The scan interval has elapsed.
6. The aircraft has at least `DockedAircraftAutoAttack.MinAmmo` vanilla ammo.
7. The AircraftType has a positive `DockedAircraftAutoAttack.Range`.

When a target is accepted, the aircraft sends `RadioCommand::NotifyUnlink` to release the normal dock link, then receives a destination, target, and attack mission using the existing aircraft attack path.

## Target Selection Pipeline

Candidate targets are filtered before TargetWeight scoring:

1. The target must be alive, enemy-owned, and not neutral or allied.
2. The target must be inside `DockedAircraftAutoAttack.Range`.
3. If `DockedAircraftAutoAttack.RequireVisibleTarget=true`, the target must satisfy the visibility rule.
4. The weapon slot must exist and pass projectile `AA` / `AG` compatibility for the target category.
5. The selected weapon must pass AircraftAltitude filtering if the target participates in the altitude system.
6. The selected weapon must pass normal `GetFireError` checks.

Only accepted targets are scored by TargetWeight.

## Visibility / PhobosFog Integration

`DockedAircraftAutoAttack.RequireVisibleTarget` controls whether DAAA candidates must be currently visible.

If `PhobosFog.Enabled=false`, the visibility requirement does not block targets because there is no PhobosFog visible-state source.

If `PhobosFog.Enabled=true` and `DockedAircraftAutoAttack.RequireVisibleTarget=true`, a candidate must be in cells hard-visible to the aircraft owner or allied vision. Explored and Unknown cells are not accepted.

If `DockedAircraftAutoAttack.RequireVisibleTarget=false`, invisibility alone does not reject a DAAA target. Ownership, range, weapon, ammo, and dock lifecycle filters still apply.

For building targets, any visible foundation cell is enough. If foundation information is unavailable, the building's current cell is used as fallback.

## AircraftAltitude Integration

DAAA uses `TechnoExt::IsAircraftAltitudeAllowedForWeapon` after projectile compatibility and before target scoring.

`AllowedAircraftAltitudes` can only reject a weapon candidate. It does not grant AA or AG capability.

## TargetWeight

TargetWeight only scores targets that already passed every legality filter.

If all TargetWeight values are `0`, DAAA keeps the first accepted target.

If any TargetWeight value is greater than `0`, accepted targets are scored by category:

- Aircraft targets use `TargetWeight.Aircraft`.
- Infantry targets use `TargetWeight.Infantry`.
- Unit targets use `TargetWeight.Vehicle`.
- Building targets use the maximum matching value from `TargetWeight.Building`, `TargetWeight.Defense`, `TargetWeight.Power`, and `TargetWeight.Factory`.
- Other TechnoClass targets score `0`.

Higher score wins. If scores are equal, the nearer target wins. If score and distance are equal, the first accepted target is kept.

TargetWeight does not affect manual attack orders, ordinary Guard behavior, normal weapon targeting, reload timing, dock unlinking, or the DAAA locked-target lifecycle.

## Locked Target / Reload Lifecycle

After dispatch, DAAA records a feature-owned marker target and state.

`Dispatching` means the aircraft is executing a dispatched attack path.

`LockedReloading` means the aircraft is waiting with the marker target preserved, usually because ammo or dispatch timing prevents immediate re-dispatch.

A higher-weight target appearing later does not retarget an aircraft that is already tracking a DAAA marker target. `RetargetOnHigherPriority` is not supported.

## Known Limitations

- DAAA does not implement docked in-place firing.
- DAAA does not implement dock slot reservation.
- DAAA does not implement a full airport traffic controller.
- DAAA does not implement WeaponAmmo or per-weapon ammo.
- DAAA uses vanilla ammo only.
- Team aircraft, airstrikes, and spawned aircraft are not taken over.
- Deploy-toggle behavior is reserved but not implemented.
- DAAA does not add UI, cursor feedback, or new player commands.

## Removed / Deferred Experiments

These experiments are not part of the frozen baseline:

- WeaponAmmo.
- Aircraft-only WeaponAmmo.
- Dock slot reservation.
- Failed dispatch / blind dispatch guards.
- DAAA deploy toggle behavior.

## Example INI

```ini
[General]
DockedAircraftAutoAttack=true
DockedAircraftAutoAttack.Interval=15

[ORCA]
DockedAircraftAutoAttack=true
DockedAircraftAutoAttack.Range=18
DockedAircraftAutoAttack.MinAmmo=1
DockedAircraftAutoAttack.WeaponOrder=0,1
DockedAircraftAutoAttack.RequireVisibleTarget=true
DockedAircraftAutoAttack.TargetWeight.Vehicle=100
DockedAircraftAutoAttack.TargetWeight.Building=25
```
