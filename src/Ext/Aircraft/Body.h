#pragma once
#include <Ext/Techno/Body.h>

// TODO: Implement proper extended AircraftClass.

class AircraftExt
{
public:
	static void FireWeapon(AircraftClass* pThis, AbstractClass* pTarget);
	static bool IsDockedForAutoAttack(AircraftClass* pThis);
	static AbstractClass* FindDockedAutoAttackTarget(AircraftClass* pThis);
	static bool TryDockedAutoAttack(AircraftClass* pThis);
	static bool PlaceReinforcementAircraft(AircraftClass* pThis, CellStruct edgeCell);
	static DirType GetLandingDir(AircraftClass* pThis, BuildingClass* pDock = nullptr);
};
