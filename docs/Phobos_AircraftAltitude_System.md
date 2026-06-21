# AircraftAltitude System

## Overview

AircraftAltitude is an altitude-envelope system for air-target filtering and aircraft sight scaling.

It separates an object's effective air target altitude from vanilla projectile `AA` / `AG` compatibility. The altitude system only restricts existing legal targeting and sight behavior. It does not grant weapons new AA or AG capability.

## Design Principles

1. Default behavior remains unchanged.
2. AircraftClass objects participate in the altitude system by default.
3. Non-AircraftClass objects do not participate unless explicitly configured.
4. `AllowedAircraftAltitudes` is deny-only.
5. Aircraft sight changes flow through `TechnoExt::GetSight()`.
6. PhobosFog consumes `GetSight()` naturally and does not need special altitude code.

## Effective Altitude Model

AircraftClass targets:

- Grounded, docked, or not-in-air aircraft use effective altitude `Ground`.
- Airborne aircraft use the configured `AircraftAltitude`.
- If no `AircraftAltitude` is configured, airborne aircraft fall back to `Medium`.

Non-AircraftClass targets:

- Do not participate by default.
- If explicitly configured with `AircraftAltitude=Low`, `AircraftAltitude=Medium`, or `AircraftAltitude=High`, they participate as a fixed-altitude target.
- They are not automatically detected from JumpJet, Hover, bridge, terrain, locomotor, or takeoff state.

## INI Tags

In `rulesmd.ini`:

```ini
[SOMETECHNO]                    ; TechnoType
AircraftAltitude=Medium         ; Low, Medium, High

[General]
AircraftAltitude.SightMultiplier.Ground=1.00 ; double
AircraftAltitude.SightMultiplier.Low=1.00    ; double
AircraftAltitude.SightMultiplier.Medium=1.00 ; double
AircraftAltitude.SightMultiplier.High=1.00   ; double
AircraftAltitude.DefaultAllowedAircraftAltitudes= ; list of Ground, Low, Medium, High

[SOMEWEAPON]                    ; WeaponType
AllowedAircraftAltitudes=       ; list of Ground, Low, Medium, High
```

`AircraftAltitude=Ground` is not valid on TechnoTypes. Ground is an effective runtime altitude for not-in-air aircraft.

## Sight Multipliers

Aircraft sight starts from the TechnoType `Sight` value.

For targets participating in the altitude system, `TechnoExt::GetSight()` multiplies the base sight by the relevant Rules multiplier:

- `Ground` uses `AircraftAltitude.SightMultiplier.Ground`.
- `Low` uses `AircraftAltitude.SightMultiplier.Low`.
- `Medium` uses `AircraftAltitude.SightMultiplier.Medium`.
- `High` uses `AircraftAltitude.SightMultiplier.High`.

Rules multipliers default to `1.00`.

Negative multipliers are invalid and fall back to `1.00`.

`0.00` is allowed.

Values above `10.00` are clamped to `10.00`.

If `RulesExt::Global()` is unavailable during early lifecycle windows, `GetSight()` skips the altitude multiplier and behaves as multiplier `1.00`.

## Weapon Altitude Envelope

`AllowedAircraftAltitudes` can be set on WeaponTypes.

When set, the weapon can only target objects whose effective altitude is included in the mask.

When unset, the weapon falls back to `[General] -> AircraftAltitude.DefaultAllowedAircraftAltitudes` if that global mask is set.

When both the weapon mask and global fallback are unset, no altitude filtering is applied.

Invalid tokens in `AllowedAircraftAltitudes` or `AircraftAltitude.DefaultAllowedAircraftAltitudes` fail closed to `None`.

## General Fallback

`AircraftAltitude.DefaultAllowedAircraftAltitudes` provides a global fallback mask for weapons that do not explicitly configure `AllowedAircraftAltitudes`.

It is unset by default, so existing targeting behavior is preserved unless a mod opts in globally.

## DAAA Integration

DockedAircraftAutoAttack applies altitude filtering while scanning candidates.

The scan pipeline first checks enemy relation, range, visibility, projectile compatibility, and weapon slot availability. Then it applies `IsAircraftAltitudeAllowedForWeapon` before target scoring.

TargetWeight only scores targets accepted by this filter.

## CanFire Integration

Generic `TechnoClass::CanFire` applies a deny-only altitude filter when the target participates in the altitude system.

This check rejects disallowed altitude combinations after existing target and weapon compatibility checks. It does not authorize otherwise illegal targets.

## Weapon Selection Integration

Weapon selection filters altitude-disallowed weapon candidates:

- `PickWeaponIndex`.
- MultiWeapon selection.
- Gattling weapon validity.
- `ForceWeapon` / `ForceAAWeapon` candidate return.

This reduces cases where a unit chooses a weapon that CanFire later rejects only because of altitude.

## Non-Aircraft Explicit Opt-In

Non-AircraftClass targets can opt into the altitude system by explicitly configuring `AircraftAltitude=Low`, `AircraftAltitude=Medium`, or `AircraftAltitude=High`.

This is a fixed target classification. It does not track actual vertical motion or JumpJet lifecycle.

## Compatibility With AA / AG

`AllowedAircraftAltitudes` only restricts targeting.

It cannot make a non-AA weapon attack airborne aircraft.

It cannot make a non-AG weapon attack ground targets.

Projectile `AA` / `AG`, vanilla target legality, Warhead / armor checks, `CanTarget`, `GetFireError`, and related systems remain the underlying compatibility gates.

## Examples

High-altitude aircraft with stronger airborne sight:

```ini
[General]
AircraftAltitude.SightMultiplier.Ground=0.15
AircraftAltitude.SightMultiplier.High=2.00

[ORCA]
AircraftAltitude=High
Sight=8
```

Only one weapon can attack high-altitude aircraft:

```ini
[General]
AircraftAltitude.DefaultAllowedAircraftAltitudes=Ground,Low,Medium

[FlakWeaponHighTest]
AllowedAircraftAltitudes=High
```

## Known Limitations

- No aircraft locomotor, render, Z-axis, or projectile trajectory changes.
- No UI or cursor feedback is added.
- No JumpJet automatic altitude detection.
- No `TargetAltitude` or `AllowedTargetAltitudes` aliases.
- No target acquisition rewrite beyond current weapon selection filtering.
- Non-Aircraft opt-in is fixed classification only.

## Testing Matrix

Recommended local tests:

1. Default multipliers and no masks: existing AA behavior remains unchanged.
2. High aircraft plus fallback mask excluding High: ordinary AA does not fire.
3. A specific weapon with `AllowedAircraftAltitudes=High`: that weapon can be selected and CanFire succeeds against High targets if normal AA compatibility also allows it.
4. Grounded aircraft with `AircraftAltitude.SightMultiplier.Ground=0.15`: sight is reduced while docked or not in air.
5. Airborne High aircraft with `AircraftAltitude.SightMultiplier.High=2.00`: sight grows while airborne.
6. Non-Aircraft target without explicit `AircraftAltitude`: altitude filters do not apply.
7. Non-Aircraft target with explicit `AircraftAltitude=Low`: altitude filters apply as Low.
