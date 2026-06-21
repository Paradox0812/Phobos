# Phobos Air Systems Final Freeze

## Current Branch / Baseline

This document records the current frozen air-system baseline for `feature/phobos-fog-p2`.

The current baseline includes:

- DockedAircraftAutoAttack.
- AircraftAltitude.
- PhobosFog GetSight integration.
- ReconCommand contract and fields-only tags.

## Completed Systems

### DockedAircraftAutoAttack

DockedAircraftAutoAttack is the completed docked-aircraft auto attack system.

It provides:

- Global and AircraftType enable tags.
- Scan range and scan interval.
- Vanilla ammo minimum.
- Weapon slot order.
- Optional PhobosFog visible-target requirement.
- TargetWeight scoring after all legality filters pass.
- AircraftAltitude candidate filtering.
- Dispatch through the normal aircraft attack mission path.

DAAA remains an attack-oriented system and must not be reused for ReconCommand behavior.

### AircraftAltitude

AircraftAltitude v1 core is complete.

It provides:

- TechnoType `AircraftAltitude` fields.
- AircraftClass default participation.
- Non-AircraftClass explicit opt-in.
- Ground / Low / Medium / High effective altitude.
- Ground / Low / Medium / High sight multipliers through `TechnoExt::GetSight()`.
- Global default allowed altitude mask.
- WeaponType `AllowedAircraftAltitudes`.
- DAAA altitude filtering.
- Generic CanFire deny-only filtering.
- Weapon-selection filtering.

AircraftAltitude does not change aircraft locomotion, render height, Z axis, projectile physics, or UI.

### PhobosFog GetSight Integration

PhobosFog visible refresh uses `TechnoExt::GetSight()`.

This means AircraftAltitude sight multipliers naturally affect PhobosFog visibility refresh without direct PhobosFog-specific altitude code.

## Contract / Fields-only Systems

### ReconCommand

ReconCommand is contract plus fields-only.

Current fields:

- `ReconCommand.Enabled`
- `ReconCommand.RequireDock`
- `ReconCommand.LoiterFrames`
- `ReconCommand.ReturnWhenDone`
- `ReconCommand.Range`

No behavior is implemented. There is no command, button, hotkey, aircraft mission behavior, loiter behavior, return-to-dock behavior, AI scheduling, or direct PhobosFog reveal behavior.

## Explicitly Removed / Not Continued

The following work is not part of the current baseline:

- WeaponAmmo.
- Aircraft-only WeaponAmmo.
- Dock slot reservation.
- Failed dispatch / blind dispatch experiments.
- DAAA deploy toggle behavior.
- ReconCommand behavior implementation.
- JumpJet automatic altitude recognition.
- `TargetAltitude` / `AllowedTargetAltitudes` aliases.

WeaponAmmo has been removed from the current mainline and is not part of this frozen baseline.

DAAA deploy toggle is not being continued in this freeze.

ReconCommand behavior is deferred until safe aircraft command, loiter, and return-to-dock prototypes exist.

## Known Limitations

- DAAA is not a full airport scheduler.
- DAAA does not reserve dock slots.
- DAAA does not use WeaponAmmo.
- AircraftAltitude only filters and scales sight. It does not modify actual flight height.
- Non-Aircraft altitude opt-in is fixed classification only.
- ReconCommand does not do anything at runtime yet.
- PhobosFog remains interval-based, so sight updates may have a small delay.

## Recommended Do-Not-Touch List

Do not resume these without a new contract and isolated recon:

- WeaponAmmo.
- Dock slot reservation.
- Failed dispatch / blind dispatch guards.
- DAAA deploy toggle.
- ReconCommand runtime behavior.
- JumpJet automatic altitude state.
- Airport traffic control.

## Future Work Only If Resumed

Future ReconCommand behavior requires:

1. A safe manual command entry for selected aircraft.
2. A safe docked-aircraft departure path to a cell or coordinate.
3. A safe loiter or hold path that does not trigger combat target acquisition.
4. A safe return-to-dock path.
5. A clear conflict policy with DockedAircraftAutoAttack.
6. Manual tests showing no dock or radio-link lifecycle regressions.
