#include "Body.h"

#include <algorithm>
#include <BuildingClass.h>
#include <CellClass.h>
#include <Ext/BuildingType/Body.h>
#include <Ext/House/Body.h>
#include <Ext/Rules/Body.h>
#include <Ext/WeaponType/Body.h>
#include <MapClass.h>
#include <Unsorted.h>
#include <Utilities/Debug.h>

// TODO: Implement proper extended AircraftClass.

namespace
{
	constexpr bool DockedAircraftAutoAttackDiag = false;
	constexpr bool DockedAircraftAutoAttackLifecycleDiag = false;

	bool IsDockedAircraftAutoAttackDiagEnabled()
	{
		return DockedAircraftAutoAttackDiag;
	}

	bool IsDockedAircraftAutoAttackLifecycleDiagEnabled()
	{
		return DockedAircraftAutoAttackLifecycleDiag;
	}

	struct DockedAutoAttackScanStats
	{
		int TotalTechnos = 0;
		int AliveTechnos = 0;
		int EnemyTechnos = 0;
		int InRangeTechnos = 0;
		int WeaponChecked = 0;
		int ProjectileRejected = 0;
		int WeaponUsable = 0;
		int FireErrorRejected = 0;
		int Accepted = 0;
		int CandidateLogs = 0;
	};

	bool ShouldLogDockedAutoAttack(AircraftClass* pThis)
	{
		return IsDockedAircraftAutoAttackDiagEnabled() && pThis && ((Unsorted::CurrentFrame + pThis->UniqueID) % 30) == 0;
	}

	bool ShouldLogDockedAutoAttackLifecycle(AircraftClass* pThis, int interval)
	{
		return IsDockedAircraftAutoAttackLifecycleDiagEnabled()
			&& pThis
			&& interval > 0
			&& ((Unsorted::CurrentFrame + pThis->UniqueID) % interval) == 0;
	}

	bool ShouldLogDockedAutoAttackEntry(AircraftClass* pThis)
	{
		if (!IsDockedAircraftAutoAttackLifecycleDiagEnabled())
			return false;

		if (!pThis)
			return (Unsorted::CurrentFrame % 300) == 0;

		return ((Unsorted::CurrentFrame + pThis->UniqueID) % 300) == 0;
	}

	void LogDockedAutoAttackEntryState(AircraftClass* pThis, const char* eventName, const char* rejectReason = "None", int effectiveInterval = -1)
	{
		if (!ShouldLogDockedAutoAttackEntry(pThis))
			return;

		const auto pRulesExt = RulesExt::Global();
		const auto pExt = pThis ? TechnoExt::ExtMap.Find(pThis) : nullptr;
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;
		const auto pLink = pThis && pThis->HasAnyLink() ? pThis->GetNthLink(0) : nullptr;
		const bool hasDockLink = pThis ? TechnoExt::HasRadioLinkWithDock(pThis) : false;

		Debug::Log("[DockedAircraftAutoAttack][Entry] %s RejectReason=%s Frame=%d Aircraft=%p TypeID=%s Owner=%s IsAlive=%d InLimbo=%d GlobalEnabled=%d GlobalInterval=%d TypeEnabled=%d Range=%d TypeInterval=%d MinAmmo=%d Ammo=%d DisabledByDeploy=%d Mission=%d MissionStatus=%d IsInAir=%d HasAnyLink=%d HasDockLink=%d Target=%p Destination=%p ArchiveTarget=%p DockNowHeadingTo=%p Link0=%p Airstrike=%p Spawned=%d Team=%p EffectiveInterval=%d\n",
			eventName,
			rejectReason ? rejectReason : "None",
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			pThis ? pThis->get_ID() : "<null>",
			pThis && pThis->Owner ? pThis->Owner->PlainName : "<null>",
			pThis ? pThis->IsAlive : false,
			pThis ? pThis->InLimbo : false,
			pRulesExt ? pRulesExt->DockedAircraftAutoAttack.Get() : false,
			pRulesExt ? pRulesExt->DockedAircraftAutoAttack_Interval.Get() : -1,
			pTypeExt ? pTypeExt->DockedAircraftAutoAttack.Get() : false,
			pTypeExt ? pTypeExt->DockedAircraftAutoAttack_Range.Get() : -1,
			pTypeExt ? pTypeExt->DockedAircraftAutoAttack_Interval.Get() : -1,
			pTypeExt ? pTypeExt->DockedAircraftAutoAttack_MinAmmo.Get() : -1,
			pThis ? pThis->Ammo : -1,
			pExt ? pExt->DockedAircraftAutoAttack_DisabledByDeploy : false,
			pThis ? static_cast<int>(pThis->GetCurrentMission()) : -1,
			pThis ? pThis->MissionStatus : -1,
			pThis ? pThis->IsInAir() : false,
			pThis ? pThis->HasAnyLink() : false,
			hasDockLink,
			pThis ? static_cast<void*>(pThis->Target) : nullptr,
			pThis ? static_cast<void*>(pThis->Destination) : nullptr,
			pThis ? static_cast<void*>(pThis->ArchiveTarget) : nullptr,
			pThis ? static_cast<void*>(pThis->DockNowHeadingTo) : nullptr,
			static_cast<void*>(pLink),
			pThis ? static_cast<void*>(pThis->Airstrike) : nullptr,
			pThis ? pThis->Spawned : false,
			pThis ? static_cast<void*>(pThis->Team) : nullptr,
			effectiveInterval);
	}

	void LogDockedAutoAttackState(AircraftClass* pThis, const char* reason, int effectiveInterval = -1)
	{
		if (!ShouldLogDockedAutoAttack(pThis))
			return;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt->TypeExtData;
		const auto pRulesExt = RulesExt::Global();
		const auto pLink = pThis->HasAnyLink() ? pThis->GetNthLink(0) : nullptr;
		const bool hasDockLink = TechnoExt::HasRadioLinkWithDock(pThis);

		Debug::Log("[DockedAircraftAutoAttack][Diag] Reject=%s Frame=%d Aircraft=%p Type=%s Owner=%s Mission=%d MissionStatus=%d Target=%p Destination=%p DockNowHeadingTo=%p Link0=%p ArchiveTarget=%p HasAnyLink=%d HasDockLink=%d IsInAir=%d IsAlive=%d InLimbo=%d Airstrike=%p Spawned=%d Team=%p Ammo=%d MinAmmo=%d Range=%d TypeInterval=%d EffectiveInterval=%d LastScanFrame=%d GlobalEnabled=%d TypeEnabled=%d DisabledByDeploy=%d IsFiring=%d IsLocked=%d Returning=%d\n",
			reason,
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			pThis->get_ID(),
			pThis->Owner ? pThis->Owner->PlainName : "<null>",
			static_cast<int>(pThis->GetCurrentMission()),
			pThis->MissionStatus,
			static_cast<void*>(pThis->Target),
			static_cast<void*>(pThis->Destination),
			static_cast<void*>(pThis->DockNowHeadingTo),
			static_cast<void*>(pLink),
			static_cast<void*>(pThis->ArchiveTarget),
			pThis->HasAnyLink(),
			hasDockLink,
			pThis->IsInAir(),
			pThis->IsAlive,
			pThis->InLimbo,
			static_cast<void*>(pThis->Airstrike),
			pThis->Spawned,
			static_cast<void*>(pThis->Team),
			pThis->Ammo,
			pTypeExt->DockedAircraftAutoAttack_MinAmmo.Get(),
			pTypeExt->DockedAircraftAutoAttack_Range.Get(),
			pTypeExt->DockedAircraftAutoAttack_Interval.Get(),
			effectiveInterval,
			pExt->DockedAircraftAutoAttack_LastScanFrame,
			pRulesExt->DockedAircraftAutoAttack.Get(),
			pTypeExt->DockedAircraftAutoAttack.Get(),
			pExt->DockedAircraftAutoAttack_DisabledByDeploy,
			pThis->IsFiring,
			pThis->IsLocked,
			pThis->IsReturningFromAttackRun);
	}

	void LogDockedAutoAttackTarget(AircraftClass* pThis, TechnoClass* pTarget, int weaponIndex, FireError fireError, int distance, const char* result)
	{
		if (!ShouldLogDockedAutoAttack(pThis))
			return;

		Debug::Log("[DockedAircraftAutoAttack][Diag] Target%s Frame=%d Aircraft=%p Target=%p TargetType=%s TargetOwner=%s WeaponIndex=%d FireError=%d Distance=%d\n",
			result,
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			static_cast<void*>(pTarget),
			pTarget ? pTarget->get_ID() : "<null>",
			pTarget && pTarget->Owner ? pTarget->Owner->PlainName : "<null>",
			weaponIndex,
			static_cast<int>(fireError),
			distance);
	}

	const char* GetDockedAutoAttackRelation(AircraftClass* pThis, TechnoClass* pTarget)
	{
		if (pTarget == pThis)
			return "Self";

		if (!pThis->Owner || !pTarget || !pTarget->Owner)
			return "Neutral";

		if (pTarget->Owner->IsNeutral())
			return "Neutral";

		return pThis->Owner->IsAlliedWith(pTarget->Owner) ? "Ally" : "Enemy";
	}

	bool HasDockedAutoAttackTargetWeightsConfigured(TechnoTypeExt::ExtData* pTypeExt)
	{
		return pTypeExt
			&& (pTypeExt->DockedAircraftAutoAttack_TargetWeight_Aircraft > 0
				|| pTypeExt->DockedAircraftAutoAttack_TargetWeight_Vehicle > 0
				|| pTypeExt->DockedAircraftAutoAttack_TargetWeight_Infantry > 0
				|| pTypeExt->DockedAircraftAutoAttack_TargetWeight_Building > 0
				|| pTypeExt->DockedAircraftAutoAttack_TargetWeight_Defense > 0
				|| pTypeExt->DockedAircraftAutoAttack_TargetWeight_Power > 0
				|| pTypeExt->DockedAircraftAutoAttack_TargetWeight_Factory > 0);
	}

	int GetDockedAutoAttackTargetCategoryWeight(TechnoTypeExt::ExtData* pTypeExt, TechnoClass* pTarget)
	{
		if (!pTypeExt || !pTarget)
			return 0;

		if (abstract_cast<AircraftClass*, true>(pTarget))
			return pTypeExt->DockedAircraftAutoAttack_TargetWeight_Aircraft.Get();

		if (abstract_cast<InfantryClass*, true>(pTarget))
			return pTypeExt->DockedAircraftAutoAttack_TargetWeight_Infantry.Get();

		if (abstract_cast<UnitClass*, true>(pTarget))
			return pTypeExt->DockedAircraftAutoAttack_TargetWeight_Vehicle.Get();

		if (const auto pBuilding = abstract_cast<BuildingClass*, true>(pTarget))
		{
			const auto pBuildingType = pBuilding->Type;
			int weight = pTypeExt->DockedAircraftAutoAttack_TargetWeight_Building.Get();

			if (pBuildingType)
			{
				if (pBuildingType->IsBaseDefense)
					weight = std::max(weight, pTypeExt->DockedAircraftAutoAttack_TargetWeight_Defense.Get());

				if (pBuildingType->PowerBonus > 0)
					weight = std::max(weight, pTypeExt->DockedAircraftAutoAttack_TargetWeight_Power.Get());

				if (pBuildingType->Factory != AbstractType::None)
					weight = std::max(weight, pTypeExt->DockedAircraftAutoAttack_TargetWeight_Factory.Get());
			}

			return weight;
		}

		return 0;
	}

	void LogDockedAutoAttackCandidate(
		AircraftClass* pThis,
		TechnoClass* pTarget,
		int weaponIndex,
		FireError fireError,
		int distance,
		bool inRange,
		bool targetIsAir,
		bool projectileAA,
		bool projectileAG,
		bool accepted,
		const char* rejectReason,
		DockedAutoAttackScanStats& stats)
	{
		if (!IsDockedAircraftAutoAttackDiagEnabled())
			return;

		if (stats.CandidateLogs >= 8)
			return;

		++stats.CandidateLogs;

		const auto targetCell = pTarget ? pTarget->GetMapCoords() : CellStruct::Empty;
		const auto pExt = pThis ? TechnoExt::ExtMap.Find(pThis) : nullptr;
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;
		const bool targetWeightConfigured = HasDockedAutoAttackTargetWeightsConfigured(pTypeExt);
		const int targetWeight = GetDockedAutoAttackTargetCategoryWeight(pTypeExt, pTarget);

		Debug::Log("[DockedAircraftAutoAttack][Diag] Candidate Frame=%d Aircraft=%p Target=%p TargetType=%s TargetOwner=%s Relation=%s TargetCell=(%d,%d) DistanceLeptons=%d InRange=%d WeaponIndex=%d TargetIsAir=%d ProjectileAA=%d ProjectileAG=%d FireErrorDiagnostic=%d TargetWeightConfigured=%d TargetWeight=%d Accepted=%d RejectReason=%s\n",
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			static_cast<void*>(pTarget),
			pTarget ? pTarget->get_ID() : "<null>",
			pTarget && pTarget->Owner ? pTarget->Owner->PlainName : "<null>",
			GetDockedAutoAttackRelation(pThis, pTarget),
			targetCell.X,
			targetCell.Y,
			distance,
			inRange,
			weaponIndex,
			targetIsAir,
			projectileAA,
			projectileAG,
			static_cast<int>(fireError),
			targetWeightConfigured,
			targetWeight,
			accepted,
			rejectReason);
	}

	void LogDockedAutoAttackCandidate(
		AircraftClass* pThis,
		TechnoClass* pTarget,
		int weaponIndex,
		FireError fireError,
		int distance,
		bool inRange,
		bool accepted,
		const char* rejectReason,
		DockedAutoAttackScanStats& stats)
	{
		LogDockedAutoAttackCandidate(pThis, pTarget, weaponIndex, fireError, distance, inRange,
			pTarget && pTarget->IsInAir(), false, false, accepted, rejectReason, stats);
	}

	void LogDockedAutoAttackScanStart(AircraftClass* pThis, const CoordStruct& center, const CoordStruct& dockCoords, bool centerFromDock, int range, int rangeLeptons)
	{
		if (!IsDockedAircraftAutoAttackDiagEnabled())
			return;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt->TypeExtData;
		const auto aircraftCell = pThis->GetMapCoords();
		const auto dockCell = centerFromDock ? CellClass::Coord2Cell(dockCoords) : CellStruct::Empty;
		const int weaponOrder0 = pTypeExt->DockedAircraftAutoAttack_WeaponOrder.size() > 0 ? pTypeExt->DockedAircraftAutoAttack_WeaponOrder[0] : -1;
		const int weaponOrder1 = pTypeExt->DockedAircraftAutoAttack_WeaponOrder.size() > 1 ? pTypeExt->DockedAircraftAutoAttack_WeaponOrder[1] : -1;

		Debug::Log("[DockedAircraftAutoAttack][Diag] ScanStart Frame=%d Aircraft=%p Type=%s Owner=%s Mission=%d MissionStatus=%d CenterSource=%s AircraftCell=(%d,%d) DockCell=(%d,%d) Center=(%d,%d,%d) RangeRaw=%d RangeLeptons=%d Ammo=%d MinAmmo=%d WeaponOrder=%d,%d\n",
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			pThis->get_ID(),
			pThis->Owner ? pThis->Owner->PlainName : "<null>",
			static_cast<int>(pThis->GetCurrentMission()),
			pThis->MissionStatus,
			centerFromDock ? "Dock" : "Aircraft",
			aircraftCell.X,
			aircraftCell.Y,
			dockCell.X,
			dockCell.Y,
			center.X,
			center.Y,
			center.Z,
			range,
			rangeLeptons,
			pThis->Ammo,
			pTypeExt->DockedAircraftAutoAttack_MinAmmo.Get(),
			weaponOrder0,
			weaponOrder1);
	}

	void LogDockedAutoAttackScanEnd(
		AircraftClass* pThis,
		const DockedAutoAttackScanStats& stats,
		const char* result,
		TechnoClass* pBestTarget = nullptr,
		int bestWeight = -1,
		int bestDistance = -1)
	{
		if (!IsDockedAircraftAutoAttackDiagEnabled())
			return;

		Debug::Log("[DockedAircraftAutoAttack][Diag] ScanEnd Frame=%d Aircraft=%p TotalTechnos=%d AliveTechnos=%d EnemyTechnos=%d InRangeTechnos=%d WeaponChecked=%d ProjectileRejected=%d WeaponUsable=%d FireErrorRejected=%d Accepted=%d BestTarget=%p BestWeight=%d BestDistance=%d Result=%s\n",
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			stats.TotalTechnos,
			stats.AliveTechnos,
			stats.EnemyTechnos,
			stats.InRangeTechnos,
			stats.WeaponChecked,
			stats.ProjectileRejected,
			stats.WeaponUsable,
			stats.FireErrorRejected,
			stats.Accepted,
			static_cast<void*>(pBestTarget),
			bestWeight,
			bestDistance,
			result);
	}

	bool IsDockedAutoAttackMissionSafe(AircraftClass* pThis)
	{
		if (pThis->Target)
			return false;

		if (pThis->ArchiveTarget)
			return false;

		if (pThis->HaveMegaMission())
			return false;

		if (pThis->Destination && pThis->Destination != pThis->DockNowHeadingTo)
			return false;

		if (pThis->IsFiring || pThis->IsLocked)
			return false;

		switch (pThis->GetCurrentMission())
		{
		case Mission::Guard:
		case Mission::Sleep:
			return pThis->MissionStatus == 0;
		default:
			return false;
		}
	}

	const char* GetDockedAutoAttackMissionUnsafeReason(AircraftClass* pThis)
	{
		if (!pThis)
			return "Other";

		if (pThis->Target)
			return "HasTarget";

		if (pThis->ArchiveTarget)
			return "HasArchiveTarget";

		if (pThis->HaveMegaMission())
			return "HasMegaMission";

		if (pThis->Destination && pThis->Destination != pThis->DockNowHeadingTo)
			return "DestinationAwayFromDock";

		if (pThis->IsFiring || pThis->IsLocked)
			return "FiringOrLocked";

		switch (pThis->GetCurrentMission())
		{
		case Mission::Guard:
		case Mission::Sleep:
			return pThis->MissionStatus == 0 ? nullptr : "MissionOrStatusUnsafe";
		default:
			return "MissionOrStatusUnsafe";
		}
	}

	CoordStruct GetDockedAutoAttackCenter(AircraftClass* pThis)
	{
		if (pThis->HasAnyLink())
		{
			if (const auto pDock = abstract_cast<BuildingClass*, true>(pThis->GetNthLink(0)))
				return pDock->GetCoords();
		}

		return pThis->GetCoords();
	}

	bool IsDockedAutoAttackProjectileCompatible(WeaponTypeClass* pWeapon, TechnoClass* pTarget, bool& targetIsAir, bool& projectileAA, bool& projectileAG)
	{
		targetIsAir = pTarget && pTarget->IsInAir();
		projectileAA = false;
		projectileAG = false;

		if (!pWeapon || !pWeapon->Projectile)
			return false;

		const auto pProjectile = pWeapon->Projectile;
		projectileAA = pProjectile->AA;
		projectileAG = pProjectile->AG;

		return targetIsAir ? projectileAA : projectileAG;
	}

	bool IsDockedAutoAttackCellVisibleTo(HouseClass* pOwner, const CellStruct& cell)
	{
		if (!pOwner || !MapClass::Instance.TryGetCellAt(cell))
			return false;

		const int cellIndex = MapClass::GetCellIndex(cell);

		if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
			return false;

		bool querySucceeded = false;
		const bool visible = HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies(pOwner, cellIndex, querySucceeded);

		return querySucceeded && visible;
	}

	bool IsDockedAutoAttackBuildingVisibleTo(HouseClass* pOwner, BuildingClass* pBuilding)
	{
		if (!pOwner || !pBuilding)
			return false;

		const auto pFoundation = pBuilding->GetFoundationData(false);

		if (!pFoundation)
			return IsDockedAutoAttackCellVisibleTo(pOwner, pBuilding->GetMapCoords());

		const auto baseCell = pBuilding->GetMapCoords();
		const CellStruct foundationEnd = { 0x7FFF, 0x7FFF };

		for (auto pCellOffset = pFoundation; *pCellOffset != foundationEnd; ++pCellOffset)
		{
			if (IsDockedAutoAttackCellVisibleTo(pOwner, baseCell + *pCellOffset))
				return true;
		}

		return false;
	}

	bool IsDockedAutoAttackTargetVisibleTo(AircraftClass* pThis, TechnoClass* pTarget)
	{
		if (!pThis || !pTarget)
			return false;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;

		if (!pTypeExt)
			return false;

		if (!pTypeExt->DockedAircraftAutoAttack_RequireVisibleTarget)
			return true;

		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled)
			return true;

		if (!pThis->Owner)
			return false;

		if (const auto pBuilding = abstract_cast<BuildingClass*, true>(pTarget))
			return IsDockedAutoAttackBuildingVisibleTo(pThis->Owner, pBuilding);

		return IsDockedAutoAttackCellVisibleTo(pThis->Owner, pTarget->GetMapCoords());
	}

	const char* GetDockedAutoAttackStateName(DockedAircraftAutoAttackState state)
	{
		switch (state)
		{
		case DockedAircraftAutoAttackState::Dispatching:
			return "Dispatching";
		case DockedAircraftAutoAttackState::LockedReloading:
			return "LockedReloading";
		default:
			return "None";
		}
	}

	enum class DockedAutoAttackUnlinkMode
	{
		None,
		SendToFirstLink,
		LinkedObjectNotify
	};

	const char* GetDockedAutoAttackUnlinkModeName(DockedAutoAttackUnlinkMode mode)
	{
		switch (mode)
		{
		case DockedAutoAttackUnlinkMode::SendToFirstLink:
			return "SendToFirstLink";
		case DockedAutoAttackUnlinkMode::LinkedObjectNotify:
			return "LinkedObjectNotify";
		default:
			return "None";
		}
	}

	const char* GetDockedAutoAttackWakeModeName(DockedAutoAttackUnlinkMode unlinkMode, bool wakeFromSleep)
	{
		if (unlinkMode != DockedAutoAttackUnlinkMode::None)
			return wakeFromSleep ? "NotifyUnlink+EnterIdleMode" : "NotifyUnlink";

		return wakeFromSleep ? "EnterIdleMode" : "None";
	}

	DockedAutoAttackUnlinkMode NotifyDockedAircraftAutoAttackUnlink(AircraftClass* pThis)
	{
		if (!pThis)
			return DockedAutoAttackUnlinkMode::None;

		const bool hasAnyLinkBefore = pThis->HasAnyLink();
		const bool hasDockLinkBefore = TechnoExt::HasRadioLinkWithDock(pThis);
		const auto pLinkBefore = hasAnyLinkBefore ? pThis->GetNthLink(0) : nullptr;

		if (ShouldLogDockedAutoAttackLifecycle(pThis, 30))
		{
			Debug::Log("[DockedAircraftAutoAttack][State] NotifyUnlink Frame=%d Aircraft=%p HasAnyLinkBefore=%d HasDockLinkBefore=%d Link0Before=%p DockNowHeadingToBefore=%p ArchiveTargetBefore=%p MissionBefore=%d IsInAirBefore=%d\n",
				Unsorted::CurrentFrame,
				static_cast<void*>(pThis),
				hasAnyLinkBefore,
				hasDockLinkBefore,
				static_cast<void*>(pLinkBefore),
				static_cast<void*>(pThis->DockNowHeadingTo),
				static_cast<void*>(pThis->ArchiveTarget),
				static_cast<int>(pThis->GetCurrentMission()),
				pThis->IsInAir());
		}

		if (!hasAnyLinkBefore)
			return DockedAutoAttackUnlinkMode::None;

		pThis->SendToFirstLink(RadioCommand::NotifyUnlink);
		return DockedAutoAttackUnlinkMode::SendToFirstLink;
	}

	bool IsDockedAutoAttackMarkerTarget(AircraftClass* pThis)
	{
		if (!pThis)
			return false;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);

		return pExt
			&& pExt->DockedAircraftAutoAttack_Target
			&& pThis->Target == pExt->DockedAircraftAutoAttack_Target;
	}

	TechnoClass* FindDockedAutoAttackMarkerTargetTechno(AircraftClass* pThis)
	{
		if (!pThis)
			return nullptr;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pMarkerTarget = pExt ? pExt->DockedAircraftAutoAttack_Target : nullptr;

		if (!pMarkerTarget)
			return nullptr;

		for (const auto pTechno : TechnoClass::Array)
		{
			if (pTechno == pMarkerTarget)
				return pTechno;
		}

		return nullptr;
	}

	void LogDockedAutoAttackStateMachine(
		AircraftClass* pThis,
		const char* eventName,
		bool targetValid,
		bool targetInRange,
		bool projectileCompatible,
		int selectedWeaponIndex,
		int interval)
	{
		if (!ShouldLogDockedAutoAttackLifecycle(pThis, interval))
			return;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;
		const auto pMarkerTarget = pExt ? pExt->DockedAircraftAutoAttack_Target : nullptr;
		const bool hasDockLink = pThis ? TechnoExt::HasRadioLinkWithDock(pThis) : false;

		Debug::Log("[DockedAircraftAutoAttack][State] %s Frame=%d Aircraft=%p State=%s Target=%p CurrentTarget=%p Mission=%d MissionStatus=%d Ammo=%d MinAmmo=%d HasDockLink=%d IsInAir=%d TargetValid=%d TargetInRange=%d ProjectileCompatible=%d WeaponIndex=%d LastDispatchFrame=%d\n",
			eventName,
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			pExt ? GetDockedAutoAttackStateName(pExt->DockedAircraftAutoAttack_State) : "<no-ext>",
			static_cast<void*>(pMarkerTarget),
			static_cast<void*>(pThis ? pThis->Target : nullptr),
			pThis ? static_cast<int>(pThis->GetCurrentMission()) : -1,
			pThis ? pThis->MissionStatus : -1,
			pThis ? pThis->Ammo : -1,
			pTypeExt ? pTypeExt->DockedAircraftAutoAttack_MinAmmo.Get() : -1,
			hasDockLink,
			pThis ? pThis->IsInAir() : false,
			targetValid,
			targetInRange,
			projectileCompatible,
			selectedWeaponIndex,
			pExt ? pExt->DockedAircraftAutoAttack_LastDispatchFrame : -1);
	}

	void LogDockedAutoAttackReDispatchAttempt(
		AircraftClass* pThis,
		TechnoClass* pResolvedTarget,
		bool targetValid,
		bool targetInRange,
		bool projectileCompatible,
		int selectedWeaponIndex)
	{
		if (!ShouldLogDockedAutoAttackLifecycle(pThis, 30))
			return;

		const auto pExt = pThis ? TechnoExt::ExtMap.Find(pThis) : nullptr;
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;
		const auto pMarkerTarget = pExt ? pExt->DockedAircraftAutoAttack_Target : nullptr;
		const bool hasDockLink = pThis ? TechnoExt::HasRadioLinkWithDock(pThis) : false;

		Debug::Log("[DockedAircraftAutoAttack][State] ReDispatchAttempt Frame=%d Aircraft=%p MarkerTarget=%p ResolvedTarget=%p TargetInArray=%d Mission=%d MissionStatus=%d Ammo=%d MinAmmo=%d HasDockLink=%d IsInAir=%d TargetValid=%d TargetInRange=%d ProjectileCompatible=%d WeaponIndex=%d\n",
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			static_cast<void*>(pMarkerTarget),
			static_cast<void*>(pResolvedTarget),
			pResolvedTarget != nullptr,
			pThis ? static_cast<int>(pThis->GetCurrentMission()) : -1,
			pThis ? pThis->MissionStatus : -1,
			pThis ? pThis->Ammo : -1,
			pTypeExt ? pTypeExt->DockedAircraftAutoAttack_MinAmmo.Get() : -1,
			hasDockLink,
			pThis ? pThis->IsInAir() : false,
			targetValid,
			targetInRange,
			projectileCompatible,
			selectedWeaponIndex);
	}

	void LogDockedAutoAttackMarkerTargetMissing(AircraftClass* pThis)
	{
		if (!ShouldLogDockedAutoAttackLifecycle(pThis, 30))
			return;

		const auto pExt = pThis ? TechnoExt::ExtMap.Find(pThis) : nullptr;
		const auto pMarkerTarget = pExt ? pExt->DockedAircraftAutoAttack_Target : nullptr;

		Debug::Log("[DockedAircraftAutoAttack][State] StateClear Reason=MarkerTargetMissing Frame=%d Aircraft=%p MarkerTarget=%p CurrentTarget=%p Mission=%d MissionStatus=%d Ammo=%d HasDockLink=%d IsInAir=%d\n",
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			static_cast<void*>(pMarkerTarget),
			static_cast<void*>(pThis ? pThis->Target : nullptr),
			pThis ? static_cast<int>(pThis->GetCurrentMission()) : -1,
			pThis ? pThis->MissionStatus : -1,
			pThis ? pThis->Ammo : -1,
			pThis ? TechnoExt::HasRadioLinkWithDock(pThis) : false,
			pThis ? pThis->IsInAir() : false);
	}

	void ClearDockedAutoAttackState(AircraftClass* pThis, bool clearTargetIfOwned, const char* eventName)
	{
		if (!pThis)
			return;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);

		if (!pExt)
			return;

		const bool ownsTarget = IsDockedAutoAttackMarkerTarget(pThis);

		LogDockedAutoAttackStateMachine(pThis, eventName, false, false, false, -1, 30);

		pExt->DockedAircraftAutoAttack_State = DockedAircraftAutoAttackState::None;
		pExt->DockedAircraftAutoAttack_Target = nullptr;
		pExt->DockedAircraftAutoAttack_LastDispatchFrame = 0;

		if (clearTargetIfOwned && ownsTarget)
			pThis->SetTarget(nullptr);
	}

	bool IsDockedAutoAttackTargetStillValid(
		AircraftClass* pThis,
		TechnoClass* pTarget,
		int& selectedWeaponIndex,
		bool& targetInRange,
		bool& projectileCompatible)
	{
		selectedWeaponIndex = -1;
		targetInRange = false;
		projectileCompatible = false;

		if (!pThis || !pTarget || pTarget == pThis)
			return false;

		if (!pTarget->IsAlive || pTarget->InLimbo || !pTarget->IsOnMap)
			return false;

		if (!pThis->Owner || !pTarget->Owner || pTarget->Owner->IsNeutral() || pThis->Owner->IsAlliedWith(pTarget->Owner))
			return false;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;

		if (!pTypeExt || pTypeExt->DockedAircraftAutoAttack_Range <= 0)
			return false;

		const int distance = static_cast<int>(GetDockedAutoAttackCenter(pThis).DistanceFrom(pTarget->GetCoords()));
		targetInRange = distance <= pTypeExt->DockedAircraftAutoAttack_Range.Get() * Unsorted::LeptonsPerCell;

		if (!targetInRange)
			return false;

		if (!IsDockedAutoAttackTargetVisibleTo(pThis, pTarget))
			return false;

		for (const int weaponIndex : pTypeExt->DockedAircraftAutoAttack_WeaponOrder)
		{
			if (weaponIndex < 0 || weaponIndex > 1)
				continue;

			const auto pWeapon = pThis->GetWeapon(weaponIndex);

			if (!pWeapon || !pWeapon->WeaponType)
				continue;

			bool targetIsAir = false;
			bool projectileAA = false;
			bool projectileAG = false;

			if (IsDockedAutoAttackProjectileCompatible(pWeapon->WeaponType, pTarget, targetIsAir, projectileAA, projectileAG))
			{
				if (!TechnoExt::IsAircraftAltitudeAllowedForWeapon(pWeapon->WeaponType, pTarget))
					continue;

				projectileCompatible = true;
				selectedWeaponIndex = weaponIndex;
				return true;
			}
		}

		return false;
	}

	bool DispatchDockedAutoAttack(AircraftClass* pThis, TechnoClass* pTarget, int selectedWeaponIndex, const char* eventName)
	{
		if (!pThis || !pTarget)
			return false;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;

		if (!pExt || !pTypeExt)
			return false;

		const int currentFrame = Unsorted::CurrentFrame;
		const int effectiveInterval = pTypeExt->DockedAircraftAutoAttack_Interval >= 1
			? pTypeExt->DockedAircraftAutoAttack_Interval.Get()
			: RulesExt::Global()->DockedAircraftAutoAttack_Interval.Get();
		const int redispatchCooldown = std::max(effectiveInterval, 30);

		if (pExt->DockedAircraftAutoAttack_LastDispatchFrame > 0
			&& currentFrame >= pExt->DockedAircraftAutoAttack_LastDispatchFrame
			&& currentFrame - pExt->DockedAircraftAutoAttack_LastDispatchFrame < redispatchCooldown)
		{
			LogDockedAutoAttackStateMachine(pThis, "ReDispatchCooldown", true, true, true, selectedWeaponIndex, 30);
			return true;
		}

		LogDockedAutoAttackReDispatchAttempt(pThis, pTarget, true, true, true, selectedWeaponIndex);

		const auto missionBeforeWake = pThis->GetCurrentMission();
		const bool wakeFromSleep = missionBeforeWake == Mission::Sleep
			&& !pThis->IsInAir()
			&& TechnoExt::HasRadioLinkWithDock(pThis);
		const auto unlinkMode = NotifyDockedAircraftAutoAttackUnlink(pThis);
		const char* wakeMode = GetDockedAutoAttackWakeModeName(unlinkMode, wakeFromSleep);

		if (wakeFromSleep)
			pThis->EnterIdleMode(false, true);

		const auto missionAfterWake = pThis->GetCurrentMission();
		const auto missionBeforeDispatch = missionAfterWake;
		const int missionStatusBeforeDispatch = pThis->MissionStatus;

		if (selectedWeaponIndex >= 0)
			pExt->CurrentAircraftWeaponIndex = selectedWeaponIndex;

		pExt->DockedAircraftAutoAttack_State = DockedAircraftAutoAttackState::Dispatching;
		pExt->DockedAircraftAutoAttack_Target = pTarget;
		pExt->DockedAircraftAutoAttack_LastDispatchFrame = currentFrame;

		LogDockedAutoAttackStateMachine(pThis, eventName, true, true, true, selectedWeaponIndex, 30);

		pThis->SetDestination(pTarget, false);
		const bool queueResult = pThis->QueueMission(Mission::Attack, true);
		pThis->SetTarget(pTarget);

		if (ShouldLogDockedAutoAttackLifecycle(pThis, 30))
		{
			const int missionStatusAfterDispatch = pThis->MissionStatus;
			const auto pLinkAfter = pThis->HasAnyLink() ? pThis->GetNthLink(0) : nullptr;

			Debug::Log("[DockedAircraftAutoAttack][State] ReDispatchResult Frame=%d Aircraft=%p WakeMode=%s UnlinkMode=%s MissionBeforeWake=%d MissionAfterWake=%d MissionBeforeDispatch=%d MissionAfterDispatch=%d MissionStatusBefore=%d MissionStatusAfter=%d QueueResult=%d TargetAfter=%p DestinationAfter=%p HasAnyLinkAfter=%d HasDockLinkAfter=%d Link0After=%p DockNowHeadingToAfter=%p ArchiveTargetAfter=%p IsInAirAfter=%d LastDispatchFrame=%d\n",
				Unsorted::CurrentFrame,
				static_cast<void*>(pThis),
				wakeMode,
				GetDockedAutoAttackUnlinkModeName(unlinkMode),
				static_cast<int>(missionBeforeWake),
				static_cast<int>(missionAfterWake),
				static_cast<int>(missionBeforeDispatch),
				static_cast<int>(pThis->GetCurrentMission()),
				missionStatusBeforeDispatch,
				missionStatusAfterDispatch,
				queueResult,
				static_cast<void*>(pThis->Target),
				static_cast<void*>(pThis->Destination),
				pThis->HasAnyLink(),
				TechnoExt::HasRadioLinkWithDock(pThis),
				static_cast<void*>(pLinkAfter),
				static_cast<void*>(pThis->DockNowHeadingTo),
				static_cast<void*>(pThis->ArchiveTarget),
				pThis->IsInAir(),
				pExt->DockedAircraftAutoAttack_LastDispatchFrame);
		}

		return true;
	}

	bool TryProcessDockedAutoAttackState(AircraftClass* pThis)
	{
		if (!pThis)
			return false;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;

		if (!pExt || !pTypeExt || !pExt->DockedAircraftAutoAttack_Target)
			return false;

		if (pThis->Target && pThis->Target != pExt->DockedAircraftAutoAttack_Target)
		{
			ClearDockedAutoAttackState(pThis, false, "ManualTargetOverride");
			return false;
		}

		if (pExt->DockedAircraftAutoAttack_State != DockedAircraftAutoAttackState::Dispatching
			&& pExt->DockedAircraftAutoAttack_State != DockedAircraftAutoAttackState::LockedReloading)
		{
			ClearDockedAutoAttackState(pThis, false, "StateClear InvalidState");
			return false;
		}

		const auto pTargetTechno = FindDockedAutoAttackMarkerTargetTechno(pThis);

		if (!pTargetTechno)
		{
			LogDockedAutoAttackReDispatchAttempt(pThis, nullptr, false, false, false, -1);
			LogDockedAutoAttackMarkerTargetMissing(pThis);
			ClearDockedAutoAttackState(pThis, true, "StateClear MarkerTargetMissing");
			return false;
		}

		int selectedWeaponIndex = -1;
		bool targetInRange = false;
		bool projectileCompatible = false;
		const bool targetValid = IsDockedAutoAttackTargetStillValid(
			pThis,
			pTargetTechno,
			selectedWeaponIndex,
			targetInRange,
			projectileCompatible);

		if (!targetValid)
		{
			ClearDockedAutoAttackState(pThis, true, "StateClear InvalidTarget");
			return false;
		}

		if (pExt->DockedAircraftAutoAttack_LastDispatchFrame == Unsorted::CurrentFrame)
		{
			LogDockedAutoAttackStateMachine(pThis, "ReDispatch SameFrameSkipped", targetValid, targetInRange, projectileCompatible, selectedWeaponIndex, 30);
			return true;
		}

		if (pThis->Ammo < pTypeExt->DockedAircraftAutoAttack_MinAmmo)
		{
			if (pExt->DockedAircraftAutoAttack_State != DockedAircraftAutoAttackState::LockedReloading)
			{
				pExt->DockedAircraftAutoAttack_State = DockedAircraftAutoAttackState::LockedReloading;
				LogDockedAutoAttackStateMachine(pThis, "StateSet LockedReloading", targetValid, targetInRange, projectileCompatible, selectedWeaponIndex, 30);
			}

			LogDockedAutoAttackStateMachine(pThis, "WaitingReload", targetValid, targetInRange, projectileCompatible, selectedWeaponIndex, 60);
			return true;
		}

		DispatchDockedAutoAttack(pThis, pTargetTechno, selectedWeaponIndex, "ReDispatch");
		return true;
	}

	void GetDockedAutoAttackTargetDiagnostics(
		AircraftClass* pThis,
		TechnoClass*& pTargetTechno,
		int& distance,
		bool& targetEnemy,
		bool& targetInRange,
		bool& projectileCompatible)
	{
		const auto pExt = pThis ? TechnoExt::ExtMap.Find(pThis) : nullptr;
		pTargetTechno = nullptr;

		if (pThis && pThis->Target)
		{
			pTargetTechno = pExt && pThis->Target == pExt->DockedAircraftAutoAttack_Target
				? FindDockedAutoAttackMarkerTargetTechno(pThis)
				: abstract_cast<TechnoClass*, true>(pThis->Target);
		}

		distance = -1;
		targetEnemy = false;
		targetInRange = false;
		projectileCompatible = false;

		if (!pThis || !pTargetTechno || !pExt)
			return;

		const auto pTypeExt = pExt->TypeExtData;
		distance = static_cast<int>(GetDockedAutoAttackCenter(pThis).DistanceFrom(pTargetTechno->GetCoords()));
		targetInRange = pTypeExt->DockedAircraftAutoAttack_Range > 0
			&& distance <= pTypeExt->DockedAircraftAutoAttack_Range.Get() * Unsorted::LeptonsPerCell;
		targetEnemy = pThis->Owner
			&& pTargetTechno->Owner
			&& !pTargetTechno->Owner->IsNeutral()
			&& !pThis->Owner->IsAlliedWith(pTargetTechno->Owner);

		for (const int weaponIndex : pTypeExt->DockedAircraftAutoAttack_WeaponOrder)
		{
			if (weaponIndex < 0 || weaponIndex > 1)
				continue;

			const auto pWeapon = pThis->GetWeapon(weaponIndex);
			bool targetIsAir = false;
			bool projectileAA = false;
			bool projectileAG = false;

			if (pWeapon && IsDockedAutoAttackProjectileCompatible(pWeapon->WeaponType, pTargetTechno, targetIsAir, projectileAA, projectileAG))
			{
				projectileCompatible = true;
				break;
			}
		}
	}

	void LogDockedAutoAttackLifecycleState(AircraftClass* pThis, const char* eventName, const char* reason, int interval)
	{
		if (!ShouldLogDockedAutoAttackLifecycle(pThis, interval))
			return;

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt->TypeExtData;
		const auto pTarget = pThis->Target;
		const auto pLink = pThis->HasAnyLink() ? pThis->GetNthLink(0) : nullptr;
		const bool hasDockLink = TechnoExt::HasRadioLinkWithDock(pThis);

		TechnoClass* pTargetTechno = nullptr;
		int targetDistance = -1;
		bool targetEnemy = false;
		bool targetInRange = false;
		bool projectileCompatible = false;
		GetDockedAutoAttackTargetDiagnostics(pThis, pTargetTechno, targetDistance, targetEnemy, targetInRange, projectileCompatible);

		Debug::Log("[DockedAircraftAutoAttack][Lifecycle] %s Frame=%d Aircraft=%p Type=%s Owner=%s Reason=%s Target=%p TargetType=%s TargetAlive=%d TargetOwner=%s TargetEnemy=%d TargetInRange=%d TargetDistance=%d ProjectileCompatible=%d Ammo=%d MinAmmo=%d Mission=%d MissionStatus=%d IsInAir=%d HasAnyLink=%d HasDockLink=%d NthLink0=%p Destination=%p DockNowHeadingTo=%p ArchiveTarget=%p Returning=%d LastScanFrame=%d\n",
			eventName,
			Unsorted::CurrentFrame,
			static_cast<void*>(pThis),
			pThis->get_ID(),
			pThis->Owner ? pThis->Owner->PlainName : "<null>",
			reason ? reason : "<null>",
			static_cast<void*>(pTarget),
			pTargetTechno ? pTargetTechno->get_ID() : "<non-techno>",
			pTargetTechno ? pTargetTechno->IsAlive : 0,
			pTargetTechno && pTargetTechno->Owner ? pTargetTechno->Owner->PlainName : "<null>",
			targetEnemy,
			targetInRange,
			targetDistance,
			projectileCompatible,
			pThis->Ammo,
			pTypeExt->DockedAircraftAutoAttack_MinAmmo.Get(),
			static_cast<int>(pThis->GetCurrentMission()),
			pThis->MissionStatus,
			pThis->IsInAir(),
			pThis->HasAnyLink(),
			hasDockLink,
			static_cast<void*>(pLink),
			static_cast<void*>(pThis->Destination),
			static_cast<void*>(pThis->DockNowHeadingTo),
			static_cast<void*>(pThis->ArchiveTarget),
			pThis->IsReturningFromAttackRun,
			pExt->DockedAircraftAutoAttack_LastScanFrame);
	}

	bool IsDockedAutoAttackStaleTargetCandidate(AircraftClass* pThis)
	{
		if (!pThis
			|| pThis->IsInAir()
			|| !TechnoExt::HasRadioLinkWithDock(pThis)
			|| !pThis->Target)
		{
			return false;
		}

		const auto pExt = TechnoExt::ExtMap.Find(pThis);
		const auto pTypeExt = pExt->TypeExtData;

		if (pThis->Ammo < pTypeExt->DockedAircraftAutoAttack_MinAmmo)
			return false;

		switch (pThis->GetCurrentMission())
		{
		case Mission::Guard:
		case Mission::Sleep:
			return true;
		default:
			return false;
		}
	}

	void LogDockedAutoAttackLifecycleCandidates(AircraftClass* pThis)
	{
		if (!pThis || pThis->IsInAir() || !TechnoExt::HasRadioLinkWithDock(pThis) || !pThis->Target)
			return;

		LogDockedAutoAttackLifecycleState(pThis, "PostAttackDocked", "DockedWithTarget", 60);

		if (IsDockedAutoAttackStaleTargetCandidate(pThis))
			LogDockedAutoAttackLifecycleState(pThis, "StaleTargetCandidate", "DockedSleepOrGuardWithTarget", 60);
	}

}

bool AircraftExt::IsDockedForAutoAttack(AircraftClass* pThis)
{
	return pThis
		&& pThis->HasAnyLink()
		&& TechnoExt::HasRadioLinkWithDock(pThis)
		&& !pThis->IsInAir();
}

AbstractClass* AircraftExt::FindDockedAutoAttackTarget(AircraftClass* pThis)
{
	if (!pThis)
		return nullptr;

	const auto pExt = TechnoExt::ExtMap.Find(pThis);
	const auto pTypeExt = pExt->TypeExtData;
	const auto range = pTypeExt->DockedAircraftAutoAttack_Range.Get();

	if (range <= 0)
	{
		LogDockedAutoAttackState(pThis, "FindRangeZero");
		return nullptr;
	}

	const auto pOwner = pThis->Owner;

	if (!pOwner)
	{
		LogDockedAutoAttackState(pThis, "FindNoOwner");
		return nullptr;
	}

	const int rangeLeptons = range * Unsorted::LeptonsPerCell;
	bool centerFromDock = false;
	auto dockCoords = CoordStruct::Empty;
	auto center = pThis->GetCoords();

	if (pThis->HasAnyLink())
	{
		if (const auto pDock = abstract_cast<BuildingClass*, true>(pThis->GetNthLink(0)))
		{
			centerFromDock = true;
			dockCoords = pDock->GetCoords();
			center = dockCoords;
		}
	}

	LogDockedAutoAttackScanStart(pThis, center, dockCoords, centerFromDock, range, rangeLeptons);

	DockedAutoAttackScanStats stats;
	const bool targetWeightConfigured = HasDockedAutoAttackTargetWeightsConfigured(pTypeExt);
	TechnoClass* pBestTarget = nullptr;
	int bestWeaponIndex = -1;
	int bestWeight = -1;
	int bestDistance = -1;

	for (const auto pTarget : TechnoClass::Array)
	{
		if (!pTarget)
			continue;

		++stats.TotalTechnos;

		if (pTarget == pThis)
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, 0, false, false, "Self", stats);
			continue;
		}

		const int distance = static_cast<int>(center.DistanceFrom(pTarget->GetCoords()));

		if (!pTarget->IsAlive)
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, false, false, "Dead", stats);
			continue;
		}

		if (pTarget->InLimbo)
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, false, false, "Limbo", stats);
			continue;
		}

		++stats.AliveTechnos;

		if (!pTarget->IsOnMap)
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, false, false, "OffMap", stats);
			continue;
		}

		if (!pTarget->Owner)
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, false, false, "NoOwner", stats);
			continue;
		}

		if (pTarget->Owner->IsNeutral())
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, false, false, "Neutral", stats);
			continue;
		}

		if (pOwner->IsAlliedWith(pTarget->Owner))
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, false, false, "Allied", stats);
			continue;
		}

		++stats.EnemyTechnos;

		if (distance > rangeLeptons)
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, false, false, "OutOfRange", stats);
			continue;
		}

		++stats.InRangeTechnos;

		if (!IsDockedAutoAttackTargetVisibleTo(pThis, pTarget))
		{
			LogDockedAutoAttackCandidate(pThis, pTarget, -1, FireError::ILLEGAL, distance, true, false, "NotVisible", stats);
			continue;
		}

		for (const int weaponIndex : pTypeExt->DockedAircraftAutoAttack_WeaponOrder)
		{
			if (weaponIndex < 0 || weaponIndex > 1)
			{
				LogDockedAutoAttackCandidate(pThis, pTarget, weaponIndex, FireError::ILLEGAL, distance, true, false, "InvalidWeaponIndex", stats);
				continue;
			}

			const auto pWeapon = pThis->GetWeapon(weaponIndex);

			if (!pWeapon || !pWeapon->WeaponType)
			{
				LogDockedAutoAttackCandidate(pThis, pTarget, weaponIndex, FireError::ILLEGAL, distance, true, false, "NoWeapon", stats);
				continue;
			}

			++stats.WeaponChecked;
			bool targetIsAir = false;
			bool projectileAA = false;
			bool projectileAG = false;

			if (!IsDockedAutoAttackProjectileCompatible(pWeapon->WeaponType, pTarget, targetIsAir, projectileAA, projectileAG))
			{
				++stats.ProjectileRejected;
				LogDockedAutoAttackCandidate(pThis, pTarget, weaponIndex, FireError::ILLEGAL, distance, true,
					targetIsAir, projectileAA, projectileAG, false,
					pWeapon->WeaponType->Projectile ? "ProjectileTargetMismatch" : "NoProjectile", stats);
				continue;
			}

			if (!TechnoExt::IsAircraftAltitudeAllowedForWeapon(pWeapon->WeaponType, pTarget))
			{
				LogDockedAutoAttackCandidate(pThis, pTarget, weaponIndex, FireError::ILLEGAL, distance, true,
					targetIsAir, projectileAA, projectileAG, false, "AircraftAltitudeMismatch", stats);
				continue;
			}

			++stats.WeaponUsable;
			const auto fireErrorDiagnostic = pThis->GetFireError(pTarget, weaponIndex, true);

			++stats.Accepted;
			LogDockedAutoAttackCandidate(pThis, pTarget, weaponIndex, fireErrorDiagnostic, distance, true,
				targetIsAir, projectileAA, projectileAG, true, "Accepted", stats);

			const int targetWeight = GetDockedAutoAttackTargetCategoryWeight(pTypeExt, pTarget);

			if (!targetWeightConfigured)
			{
				LogDockedAutoAttackScanEnd(pThis, stats, "Accepted", pTarget, targetWeight, distance);
				pExt->CurrentAircraftWeaponIndex = weaponIndex;
				return pTarget;
			}

			if (!pBestTarget
				|| targetWeight > bestWeight
				|| (targetWeight == bestWeight && distance < bestDistance))
			{
				pBestTarget = pTarget;
				bestWeaponIndex = weaponIndex;
				bestWeight = targetWeight;
				bestDistance = distance;
			}

			break;
		}
	}

	if (targetWeightConfigured && pBestTarget)
	{
		LogDockedAutoAttackScanEnd(pThis, stats, "AcceptedWeighted", pBestTarget, bestWeight, bestDistance);
		pExt->CurrentAircraftWeaponIndex = bestWeaponIndex;
		return pBestTarget;
	}

	LogDockedAutoAttackScanEnd(pThis, stats, "NoTarget");
	return nullptr;
}

bool AircraftExt::TryDockedAutoAttack(AircraftClass* pThis)
{
	LogDockedAutoAttackEntryState(pThis, "TryEntry");

	if (!pThis)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "NullThis");
		return false;
	}

	if (!pThis->IsAlive)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "Dead");
		LogDockedAutoAttackState(pThis, "Dead");
		return false;
	}

	if (pThis->InLimbo)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "InLimbo");
		LogDockedAutoAttackState(pThis, "InLimbo");
		return false;
	}

	const auto pRulesExt = RulesExt::Global();

	if (!pRulesExt->DockedAircraftAutoAttack)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "GlobalDisabled");
		LogDockedAutoAttackState(pThis, "GlobalDisabled");
		return false;
	}

	const auto pExt = TechnoExt::ExtMap.Find(pThis);
	const auto pTypeExt = pExt ? pExt->TypeExtData : nullptr;

	if (!pTypeExt)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "TypeExtMissing");
		return false;
	}

	if (!pTypeExt->DockedAircraftAutoAttack)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "TypeDisabled");
		LogDockedAutoAttackState(pThis, "TypeDisabled");
		return false;
	}

	if (pTypeExt->DockedAircraftAutoAttack_Range <= 0)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "RangeZero");
		LogDockedAutoAttackState(pThis, "RangeZero");
		return false;
	}

	if (pThis->Airstrike)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "Airstrike");
		LogDockedAutoAttackState(pThis, "Airstrike");
		return false;
	}

	if (pThis->Spawned)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "Spawned");
		LogDockedAutoAttackState(pThis, "Spawned");
		return false;
	}

	if (pThis->Team)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "Team");
		LogDockedAutoAttackState(pThis, "Team");
		return false;
	}

	if (pExt->DockedAircraftAutoAttack_DisabledByDeploy)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "DisabledByDeploy");
		LogDockedAutoAttackState(pThis, "DisabledByDeploy");
		return false;
	}

	LogDockedAutoAttackLifecycleCandidates(pThis);

	if (!AircraftExt::IsDockedForAutoAttack(pThis))
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "NotDocked");
		LogDockedAutoAttackState(pThis, "NotDocked");
		return false;
	}

	if (TryProcessDockedAutoAttackState(pThis))
		return true;

	if (pThis->Ammo < pTypeExt->DockedAircraftAutoAttack_MinAmmo)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "AmmoBelowMin");
		LogDockedAutoAttackState(pThis, "AmmoBelowMin");
		return false;
	}

	if (!IsDockedAutoAttackMissionSafe(pThis))
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "MissionUnsafe");
		LogDockedAutoAttackLifecycleState(pThis, "MissionUnsafeDetail", GetDockedAutoAttackMissionUnsafeReason(pThis), 30);
		LogDockedAutoAttackState(pThis, "MissionUnsafe");
		return false;
	}

	const int effectiveInterval = pTypeExt->DockedAircraftAutoAttack_Interval >= 1
		? pTypeExt->DockedAircraftAutoAttack_Interval.Get()
		: pRulesExt->DockedAircraftAutoAttack_Interval.Get();
	const int currentFrame = Unsorted::CurrentFrame;

	if (currentFrame >= pExt->DockedAircraftAutoAttack_LastScanFrame
		&& currentFrame - pExt->DockedAircraftAutoAttack_LastScanFrame < effectiveInterval)
	{
		LogDockedAutoAttackEntryState(pThis, "EarlyReject", "IntervalWaiting", effectiveInterval);
		return false;
	}

	pExt->DockedAircraftAutoAttack_LastScanFrame = currentFrame;

	if (const auto pTarget = AircraftExt::FindDockedAutoAttackTarget(pThis))
	{
		const auto missionBefore = pThis->GetCurrentMission();
		const int missionStatusBefore = pThis->MissionStatus;
		const auto pTargetBefore = pThis->Target;
		const auto pDestinationBefore = pThis->Destination;
		const int distance = static_cast<int>(GetDockedAutoAttackCenter(pThis).DistanceFrom(pTarget->GetCoords()));
		const auto pTargetTechno = abstract_cast<TechnoClass*, true>(pTarget);

		pExt->DockedAircraftAutoAttack_State = DockedAircraftAutoAttackState::Dispatching;
		pExt->DockedAircraftAutoAttack_Target = pTarget;
		pExt->DockedAircraftAutoAttack_LastDispatchFrame = currentFrame;
		LogDockedAutoAttackStateMachine(pThis, "StateSet Dispatching", true, true, true, pExt->CurrentAircraftWeaponIndex, 30);

		const auto unlinkMode = NotifyDockedAircraftAutoAttackUnlink(pThis);
		pThis->SetDestination(pTarget, false);
		const bool queueResult = pThis->QueueMission(Mission::Attack, true);
		pThis->SetTarget(pTarget);

		if (IsDockedAircraftAutoAttackDiagEnabled())
		{
			const auto pDockLinkAfter = pThis->HasAnyLink() ? pThis->GetNthLink(0) : nullptr;

			Debug::Log("[DockedAircraftAutoAttack][Diag] AttackQueued DispatchMode=ScriptAircraftHack UnlinkMode=%s Frame=%d Aircraft=%p Target=%p WeaponIndex=%d TargetType=%s DistanceLeptons=%d MissionBefore=%d MissionAfter=%d MissionStatusBefore=%d MissionStatusAfter=%d QueueResult=%d TargetBefore=%p TargetAfter=%p DestinationBefore=%p DestinationAfter=%p DockLinkAfter=%p HasAnyLinkAfter=%d HasDockLinkAfter=%d DockNowHeadingToAfter=%p ArchiveTargetAfter=%p IsInAirAfter=%d\n",
				GetDockedAutoAttackUnlinkModeName(unlinkMode),
				Unsorted::CurrentFrame,
				static_cast<void*>(pThis),
				static_cast<void*>(pTarget),
				pExt->CurrentAircraftWeaponIndex,
				pTargetTechno ? pTargetTechno->get_ID() : "<non-techno>",
				distance,
				static_cast<int>(missionBefore),
				static_cast<int>(pThis->GetCurrentMission()),
				missionStatusBefore,
				pThis->MissionStatus,
				queueResult,
				static_cast<void*>(pTargetBefore),
				static_cast<void*>(pThis->Target),
				static_cast<void*>(pDestinationBefore),
				static_cast<void*>(pThis->Destination),
				static_cast<void*>(pDockLinkAfter),
				pThis->HasAnyLink(),
				TechnoExt::HasRadioLinkWithDock(pThis),
				static_cast<void*>(pThis->DockNowHeadingTo),
				static_cast<void*>(pThis->ArchiveTarget),
				pThis->IsInAir());
		}

		return queueResult;
	}

	LogDockedAutoAttackEntryState(pThis, "EarlyReject", "NoTarget", effectiveInterval);
	LogDockedAutoAttackState(pThis, "NoTarget", effectiveInterval);
	return false;
}

void AircraftExt::FireWeapon(AircraftClass* pThis, AbstractClass* pTarget)
{
	auto const pExt = TechnoExt::ExtMap.Find(pThis);
	const int weaponIndex = pExt->CurrentAircraftWeaponIndex;
	auto const pWeapon = pThis->GetWeapon(weaponIndex)->WeaponType;
	auto const pWeaponExt = WeaponTypeExt::ExtMap.Find(pWeapon);
	const int burstCount = pWeapon->Burst;
	const bool isStrafe = pThis->Is_Strafe();

	if (burstCount > 0)
	{
		int& bombDropCount = pExt->Strafe_BombsDroppedThisRound;
		int& currentBurstIndex = pThis->CurrentBurstIndex;
		const bool simulateBurst = pWeaponExt->Strafing_SimulateBurst;

		for (int i = 0; i < burstCount; i++)
		{
			if (isStrafe && burstCount < 2 && simulateBurst)
				currentBurstIndex = bombDropCount % 2 == 0;

			pThis->Fire(pTarget, weaponIndex);
		}

		if (isStrafe)
		{
			bombDropCount++;

			if (pWeaponExt->Strafing_UseAmmoPerShot)
			{
				pThis->Ammo--;
				pThis->ShouldLoseAmmo = false;

				if (!pThis->Ammo)
				{
					pThis->SetTarget(nullptr);
					pThis->SetDestination(nullptr, true);
				}
			}
		}
	}
}

// Spy plane, airstrike etc.
bool AircraftExt::PlaceReinforcementAircraft(AircraftClass* pThis, CellStruct edgeCell)
{
	auto const pType = pThis->Type;
	auto const pTypeExt = TechnoTypeExt::ExtMap.Find(pType);
	auto coords = CellClass::Cell2Coord(edgeCell);
	coords.Z = 0;
	AbstractClass* pTarget = nullptr;

	if (pTypeExt->SpawnDistanceFromTarget.isset())
	{
		pTarget = pThis->Target ? pThis->Target : pThis->Destination;

		if (pTarget)
			coords = GeneralUtils::CalculateCoordsFromDistance(CellClass::Cell2Coord(edgeCell), pTarget->GetCoords(), pTypeExt->SpawnDistanceFromTarget.Get());
	}

	++Unsorted::ScenarioInit;
	const bool result = pThis->Unlimbo(coords, DirType::North);
	--Unsorted::ScenarioInit;

	pThis->SetHeight(pTypeExt->SpawnHeight.isset() ? pTypeExt->SpawnHeight.Get() : pType->GetFlightLevel());

	if (pTarget)
		pThis->PrimaryFacing.SetDesired(pThis->GetTargetDirection(pTarget));

	return result;
}

DirType AircraftExt::GetLandingDir(AircraftClass* pThis, BuildingClass* pDock)
{
	auto const poseDir = static_cast<DirType>(RulesClass::Instance->PoseDir);

	if (!pThis)
		return poseDir;

	// If this is a spawnee, use the spawner's facing.
	if (auto const pOwner = pThis->SpawnOwner)
		return pOwner->PrimaryFacing.Current().GetDir();

	auto const pType = pThis->Type;

	if (pDock || pThis->HasAnyLink())
	{
		auto const pLink = pThis->GetNthLink(0);

		if (auto const pBuilding = pDock ? pDock : abstract_cast<BuildingClass*, true>(pLink))
		{
			auto const pBuildingType = pBuilding->Type;
			auto const pBuildingTypeExt = BuildingTypeExt::ExtMap.Find(pBuildingType);
			const int docks = pBuildingType->NumberOfDocks;
			const int linkIndex = pBuilding->FindLinkIndex(pThis);

			if (docks > 0 && linkIndex >= 0 && linkIndex < docks)
			{
				if (pBuildingTypeExt->AircraftDockingDirs[linkIndex].has_value())
					return *pBuildingTypeExt->AircraftDockingDirs[linkIndex];
			}
			else if (docks > 0 && pBuildingTypeExt->AircraftDockingDirs[0].has_value())
				return *pBuildingTypeExt->AircraftDockingDirs[0];
		}
		else if (!pType->AirportBound)
			return pLink->PrimaryFacing.Current().GetDir();
	}

	const int landingDir = TechnoTypeExt::ExtMap.Find(pType)->LandingDir.Get((int)poseDir);

	if (!pType->AirportBound && landingDir < 0)
		return pThis->PrimaryFacing.Current().GetDir();

	return static_cast<DirType>(std::clamp(landingDir, 0, 255));
}
