# ReconCommand Contract

## Overview

ReconCommand is a planned manual command system for scout aircraft, AWACS aircraft, electronic reconnaissance aircraft, and similar non-combat air units.

This phase only adds INI fields and freezes the contract. It does not implement the command, aircraft movement, loiter behavior, return-to-dock behavior, UI, hotkeys, or AI scheduling.

## Design Goal

The intended future behavior is:

1. The player manually selects a compatible aircraft.
2. The player manually invokes ReconCommand and chooses a target cell or coordinate.
3. The aircraft leaves its dock.
4. The aircraft travels to the chosen reconnaissance area.
5. The aircraft loiters or holds near that area for a configured time.
6. Existing `Sight`, `TechnoExt::GetSight()`, and PhobosFog visible refresh reveal the area.
7. The aircraft returns to its dock when done, if configured to do so.

## Manual-only Command Positioning

ReconCommand is a manual command capability.

It is not automatic patrol logic. It does not automatically choose scout points. It does not automatically dispatch aircraft from airports. It does not reveal the whole map. It does not run timed loop patrols. It does not schedule AI AWACS behavior.

The first design target is a player-controlled aircraft that is explicitly sent to an explicitly chosen reconnaissance point.

## INI Tags

These tags are currently read and serialized from TechnoType extension data. They are intended for AircraftTypes only.

In `rulesmd.ini`:

```ini
[SOMEAIRCRAFT]                    ; AircraftType
ReconCommand.Enabled=false        ; boolean
ReconCommand.RequireDock=true     ; boolean
ReconCommand.LoiterFrames=450     ; integer, frames
ReconCommand.ReturnWhenDone=true  ; boolean
ReconCommand.Range=80             ; integer, cells
```

`ReconCommand.Enabled` enables this AircraftType for a future manual ReconCommand implementation. It is disabled by default.

`ReconCommand.RequireDock` reserves the first implementation for aircraft that start from a dock or airport. It defaults to true.

`ReconCommand.LoiterFrames` is the intended time the aircraft should remain near the selected reconnaissance area. Negative values are sanitized to 0.

`ReconCommand.ReturnWhenDone` records whether the future behavior should attempt to return to dock after the loiter phase. It defaults to true.

`ReconCommand.Range` is reserved for future command range, point validation, or candidate limits. Negative values are sanitized to 0.

## Intended Future Behavior

Future behavior should be implemented only after separate prototypes prove the aircraft lifecycle is safe:

```text
Manual ReconCommand
-> docked aircraft receives a selected cell or coordinate
-> aircraft leaves the dock
-> aircraft flies to the selected reconnaissance point
-> aircraft loiters or holds for ReconCommand.LoiterFrames
-> normal sight and PhobosFog refresh reveal visible cells
-> aircraft returns to dock when ReconCommand.ReturnWhenDone=true
```

## Relation to AircraftAltitude

ReconCommand does not modify AircraftAltitude.

Recon aircraft are expected to pair well with `AircraftAltitude=High`, increased `Sight`, and `AircraftAltitude.SightMultiplier.High`, but that is a data design choice for mod authors rather than behavior implemented by ReconCommand.

## Relation to PhobosFog

ReconCommand should rely on existing sight refresh behavior. PhobosFog already consumes `TechnoExt::GetSight()` when refreshing house visible cells.

ReconCommand must not directly modify PhobosFog cell states, force map reveal, or bypass the Unknown / Explored / Visible state model.

## Relation to DockedAircraftAutoAttack

ReconCommand must not reuse DockedAircraftAutoAttack as its behavior path.

DockedAircraftAutoAttack is an enemy-target and attack-mission driven system. ReconCommand is intended to be a manual reconnaissance command that targets a cell or coordinate.

The first behavior implementation should treat ReconCommand and DockedAircraftAutoAttack as mutually exclusive on the same AircraftType. If both are configured:

```ini
ReconCommand.Enabled=true
DockedAircraftAutoAttack=true
```

the behavior is undefined in this fields-only phase. A future implementation should either warn about this conflict or refuse one system from controlling the aircraft.

## Why Behavior Is Not Implemented Yet

Read-only AWACS reconnaissance found no safe reusable docked aircraft loiter primitive.

SpyPlane behavior is a super weapon / overfly reveal path. It is not a reusable docked-aircraft state machine.

`Mission::Area_Guard` and `Mission::Patrol` can involve guard or attack logic. They are not pure reconnaissance loiter behavior.

DockedAircraftAutoAttack dispatch uses enemy targets, `Mission::Attack`, and the dock / radio-link attack lifecycle. It is not suitable for manual reconnaissance.

Dock, radio-link, return-to-dock, and aircraft mission transitions are the largest risks and need isolated prototypes before behavior is implemented.

## Known Risks

- There is no confirmed safe loiter mission for a docked aircraft sent to a coordinate.
- There is no confirmed safe return-to-dock prototype for this manual command.
- Reusing attack or guard missions may accidentally trigger combat behavior.
- Reusing DockedAircraftAutoAttack may corrupt its marker target and reload state.
- Dock and radio-link handling can cause aircraft to fail to return, compete for docks, or loop between takeoff and landing.
- PhobosFog refresh is interval-based, so sight effects may appear with a small delay.

## Non-goals

- No behavior is implemented in this phase.
- No button is added.
- No hotkey is added.
- No UI state indicator is added.
- No AI automatic dispatch is added.
- No automatic map exploration is added.
- No multi-aircraft coordination is added.
- No threat avoidance is added.
- No automatic scout-point selection is added.
- No JumpJet or non-AircraftClass support is added.
- No WeaponAmmo support is added.
- No DockedAircraftAutoAttack behavior is changed.
- No PhobosFog behavior is changed.

## Future Implementation Prerequisites

Before implementing ReconCommand behavior, complete and validate these prerequisites:

1. Manual command entry recon.
2. A safe prototype for sending a docked aircraft to a cell destination.
3. A safe loiter or hold prototype.
4. A safe return-to-dock prototype.
5. Explicit mutual-exclusion handling with DockedAircraftAutoAttack.
6. Manual testing that does not reproduce dock or radio-link errors.
