# Phobos Air Systems Final Freeze

This document records the current local boundary for aircraft-related systems in the PhobosFog P2 branch.

## Stable Direction

The current air-system direction keeps these features separate:

- DockedAircraftAutoAttack handles docked aircraft that automatically attack enemy targets.
- AircraftAltitude handles effective aircraft altitude, sight multipliers, DAAA filtering, CanFire filtering, and weapon-selection filtering.
- ReconCommand is reserved for a future manual reconnaissance command.
- PhobosFog consumes `TechnoExt::GetSight()` for visible-cell refresh.

## DockedAircraftAutoAttack

DockedAircraftAutoAttack remains an attack-oriented system.

It must not be reused as ReconCommand behavior. Its dispatch path uses enemy targets, attack missions, dock unlinking, target markers, reload waiting, and target validation. These are not compatible with a manual reconnaissance command.

`DockedAircraftAutoAttack.DisableOnDeploy` remains a reserved deploy-toggle field. The deploy toggle behavior is not implemented in the current branch.

## AircraftAltitude

AircraftAltitude remains the shared altitude system.

It currently provides:

- Aircraft altitude fields.
- Ground / Low / Medium / High sight multipliers through `TechnoExt::GetSight()`.
- DAAA altitude filtering.
- Generic CanFire deny-only filtering.
- Weapon-selection filtering.

AircraftAltitude does not implement reconnaissance behavior, aircraft mission changes, aircraft locomotor changes, or UI.

## ReconCommand

ReconCommand is currently contract plus fields-only.

It defines intended future AircraftType fields:

- `ReconCommand.Enabled`
- `ReconCommand.RequireDock`
- `ReconCommand.LoiterFrames`
- `ReconCommand.ReturnWhenDone`
- `ReconCommand.Range`

The behavior implementation is deferred. ReconCommand currently does not add a command, hotkey, button, aircraft movement, loiter behavior, return-to-dock behavior, AI dispatch, or PhobosFog reveal logic.

## Deferred Work

The following work is intentionally deferred:

- ReconCommand behavior.
- DAAA deploy toggle behavior.
- Automatic AWACS or scout-plane scheduling.
- Docked-aircraft loiter behavior.
- Return-to-dock behavior outside existing vanilla flows.
- Dock slot reservation experiments.
- Failed dispatch guards.
- WeaponAmmo and aircraft ammo experiments.

## Required Evidence Before Behavior Work

Before any ReconCommand behavior implementation, prove the following in isolated phases:

1. A safe manual command entry for selected aircraft.
2. A safe docked-aircraft departure path to a cell or coordinate.
3. A safe loiter or hold path that does not trigger combat target acquisition.
4. A safe return-to-dock path.
5. A clear conflict policy with DockedAircraftAutoAttack.
6. Manual tests showing no dock or radio-link lifecycle regressions.
