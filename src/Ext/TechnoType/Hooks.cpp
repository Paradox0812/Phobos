#include "Body.h"
#include <Ext/House/Body.h>
#include <Ext/AnimType/Body.h>
#include <Ext/Rules/Body.h>

#include <BuildingClass.h>
#include <DisplayClass.h>
#include <MapClass.h>

namespace PhobosFogOreGath
{
	static bool IsCellVisibleToViewerOrAllies(HouseClass* const pViewerHouse, const int cellIndex, bool& querySucceeded)
	{
		return HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);
	}

	static bool TryGetUnitCellIndex(UnitClass* const pUnit, int& cellIndex)
	{
		if (!pUnit)
			return false;

		const auto pCell = pUnit->GetCell();

		if (!pCell || !MapClass::Instance.TryGetCellAt(pCell->MapCoords))
			return false;

		cellIndex = MapClass::GetCellIndex(pCell->MapCoords);
		return cellIndex >= 0 && cellIndex < MapClass::MaxCells;
	}

	static bool ShouldHide(UnitClass* const pUnit)
	{
		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled || !pRulesExt->PhobosFog_HideWorldAnim)
			return false;

		const auto pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver())
			return false;

		int cellIndex = -1;

		if (!TryGetUnitCellIndex(pUnit, cellIndex))
			return false;

		bool querySucceeded = false;
		const bool visible = IsCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);

		return querySucceeded && !visible;
	}
}

namespace PhobosFogTooltip
{
	static bool IsEnabled()
	{
		const auto pRulesExt = RulesExt::Global();
		return pRulesExt && pRulesExt->PhobosFog_Enabled && pRulesExt->PhobosFog_HideHoverTooltip;
	}

	static bool IsCursorEnabled()
	{
		const auto pRulesExt = RulesExt::Global();
		return pRulesExt && pRulesExt->PhobosFog_Enabled && pRulesExt->PhobosFog_HideHoverCursor;
	}

	static bool IsCommandGatingEnabled()
	{
		const auto pRulesExt = RulesExt::Global();
		return pRulesExt && pRulesExt->PhobosFog_Enabled && pRulesExt->PhobosFog_GateHiddenObjectCommands;
	}

	static bool IsCellVisibleToViewerOrAllies(HouseClass* const pViewerHouse, const int cellIndex, bool& querySucceeded)
	{
		return HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);
	}

	static bool TryGetCellIndex(const CellStruct& cell, int& cellIndex)
	{
		if (!MapClass::Instance.TryGetCellAt(cell))
			return false;

		cellIndex = MapClass::GetCellIndex(cell);
		return cellIndex >= 0 && cellIndex < MapClass::MaxCells;
	}

	static bool IsCellVisibleToViewerOrAllies(HouseClass* const pViewerHouse, const CellStruct& cell, bool& querySucceeded)
	{
		querySucceeded = false;

		int cellIndex = -1;

		if (!TryGetCellIndex(cell, cellIndex))
			return false;

		return IsCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);
	}

	static bool IsBuildingVisibleToViewerOrAllies(BuildingClass* const pBuilding, HouseClass* const pViewerHouse, bool& querySucceeded)
	{
		querySucceeded = false;

		if (!pBuilding || !pBuilding->Type || !pViewerHouse)
			return false;

		const auto pFoundation = pBuilding->GetFoundationData(false);

		if (!pFoundation)
			return false;

		const auto baseCell = pBuilding->GetMapCoords();
		const CellStruct foundationEnd = { 0x7FFF, 0x7FFF };

		for (auto pCellOffset = pFoundation; *pCellOffset != foundationEnd; ++pCellOffset)
		{
			const auto cell = baseCell + *pCellOffset;

			bool cellQuerySucceeded = false;
			const bool visible = IsCellVisibleToViewerOrAllies(pViewerHouse, cell, cellQuerySucceeded);

			if (!cellQuerySucceeded)
			{
				querySucceeded = false;
				return false;
			}

			querySucceeded = true;

			if (visible)
				return true;
		}

		return false;
	}

	static bool IsObjectVisibleToViewerOrAllies(ObjectClass* const pObject, bool& querySucceeded)
	{
		querySucceeded = false;

		if (!pObject)
			return false;

		const auto pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver())
			return false;

		if (const auto pBuilding = abstract_cast<BuildingClass*, true>(pObject))
			return IsBuildingVisibleToViewerOrAllies(pBuilding, pViewerHouse, querySucceeded);

		return IsCellVisibleToViewerOrAllies(pViewerHouse, pObject->GetMapCoords(), querySucceeded);
	}

	static bool ShouldSuppressObjectTooltip(ObjectClass* const pObject)
	{
		if (!IsEnabled())
			return false;

		bool querySucceeded = false;
		const bool visible = IsObjectVisibleToViewerOrAllies(pObject, querySucceeded);

		return querySucceeded && !visible;
	}

	static bool ShouldSuppressObjectCursor(ObjectClass* const pObject)
	{
		if (!IsCursorEnabled())
			return false;

		bool querySucceeded = false;
		const bool visible = IsObjectVisibleToViewerOrAllies(pObject, querySucceeded);

		return querySucceeded && !visible;
	}

	static bool ShouldGateHiddenObjectCommand(ObjectClass* const pObject)
	{
		if (!IsCommandGatingEnabled())
			return false;

		const auto pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver())
			return false;

		const auto pTechno = generic_cast<TechnoClass*>(pObject);

		if (!pTechno || !pTechno->Owner || pViewerHouse->IsAlliedWith(pTechno->Owner))
			return false;

		bool querySucceeded = false;
		const bool visible = IsObjectVisibleToViewerOrAllies(pObject, querySucceeded);

		return querySucceeded && !visible;
	}

	static Action GetCellOnlyAction(DisplayClass* const pDisplay, CellStruct const* const pCell, const DWORD dwUnk)
	{
		return pDisplay && pCell ? pDisplay->DecideAction(*pCell, nullptr, dwUnk) : Action::None;
	}
}

DEFINE_HOOK(0x73D223, UnitClass_DrawIt_OreGath, 0x6)
{
	enum { SkipOreGathDraw = 0x73D28E };

	GET(UnitClass*, pThis, ESI);

	if (PhobosFogOreGath::ShouldHide(pThis))
		return SkipOreGathDraw;

	GET(const int, nFacing, EDI);
	GET_STACK(RectangleStruct*, pBounds, STACK_OFFSET(0x50, 0x8));
	LEA_STACK(Point2D*, pLocation, STACK_OFFSET(0x50, -0x18));
	GET_STACK(const int, nBrightness, STACK_OFFSET(0x50, 0x4));

	auto const pData = TechnoTypeExt::ExtMap.Find(pThis->Type);

	ConvertClass* pDrawer = FileSystem::ANIM_PAL;
	SHPStruct* pSHP = FileSystem::OREGATH_SHP;
	int idxFrame;

	const int idxTiberium = pThis->GetCell()->GetContainedTiberiumIndex();
	const int idxArray = pData->OreGathering_Tiberiums.size() > 0 ? pData->OreGathering_Tiberiums.IndexOf(idxTiberium) : 0;
	if (idxTiberium != -1 && idxArray != -1)
	{
		auto const pAnimType = pData->OreGathering_Anims.size() > 0 ? pData->OreGathering_Anims[idxArray] : nullptr;
		auto const nFramesPerFacing = pData->OreGathering_FramesPerDir.size() > 0 ? pData->OreGathering_FramesPerDir[idxArray] : 15;
		auto const pAnimExt = AnimTypeExt::ExtMap.TryFind(pAnimType);
		if (pAnimType)
		{
			pSHP = pAnimType->GetImage();
			if (auto const pPalette = pAnimExt->Palette.GetConvert())
				pDrawer = pPalette;
		}
		idxFrame = nFramesPerFacing * nFacing + (Unsorted::CurrentFrame + pThis->WalkedFramesSoFar) % nFramesPerFacing;
	}
	else
	{
		idxFrame = 15 * nFacing + (Unsorted::CurrentFrame + pThis->WalkedFramesSoFar) % 15;
	}

	DSurface::Temp->DrawSHP(
		pDrawer, pSHP, idxFrame, pLocation, pBounds,
		BlitterFlags::Flat | BlitterFlags::Alpha | BlitterFlags::Centered,
		0, pThis->GetZAdjustment() - 2, ZGradient::Ground, nBrightness,
		0, nullptr, 0, 0, 0
	);

	R->EBP(nBrightness);
	R->EBX(pBounds);

	return 0x73D28C;
}

// Issue #503
// Author : Otamaa
DEFINE_HOOK(0x4AE670, DisplayClass_GetToolTip_EnemyUIName, 0x8)
{
	enum { SetUIName = 0x4AE678, ApplyToolTip = 0x4AE69D };

	GET(ObjectClass*, pObject, ECX);

	if (PhobosFogTooltip::ShouldSuppressObjectTooltip(pObject))
	{
		R->EAX(0);
		return ApplyToolTip;
	}

	auto pDecidedUIName = pObject->GetUIName();
	const auto pFoot = generic_cast<FootClass*, true>(pObject);
	const auto pTechnoType = pObject->GetTechnoType();

	if (pFoot && pTechnoType && !pObject->IsDisguised())
	{
		const auto pOwnerHouse = pFoot->Owner;
		const bool IsAlly = pOwnerHouse->IsAlliedWith(HouseClass::CurrentPlayer);
		const bool IsCivilian = (pOwnerHouse == HouseClass::FindCivilianSide()) || pOwnerHouse->IsNeutral();
		const bool IsObserver = HouseClass::Observer || HouseClass::IsCurrentPlayerObserver();

		if (!IsAlly && !IsCivilian && !IsObserver)
		{
			const auto pTechnoTypeExt = TechnoTypeExt::ExtMap.Find(pTechnoType);

			if (const auto pEnemyUIName = pTechnoTypeExt->EnemyUIName.Get().Text)
			{
				pDecidedUIName = pEnemyUIName;
			}
		}
	}

	R->EAX(pDecidedUIName);
	return SetUIName;
}

DEFINE_HOOK(0x4AAE90, DisplayClass_ConvertAction_PhobosFogCursor, 0x8)
{
	GET(DisplayClass*, pThis, ECX);
	GET_STACK(CellStruct*, pCell, 0x4);
	GET_STACK(ObjectClass*, pObject, 0xC);
	GET_STACK(DWORD, dwUnk, 0x14);

	if (PhobosFogTooltip::ShouldSuppressObjectCursor(pObject))
	{
		R->Stack(0xC, static_cast<ObjectClass*>(nullptr));
		R->Stack(0x10, PhobosFogTooltip::GetCellOnlyAction(pThis, pCell, dwUnk));
	}

	return 0;
}

DEFINE_HOOK(0x4AE750, DisplayClass_sub_4AE750_PhobosFogCommandGate, 0x8)
{
	GET_STACK(ObjectClass*, pObject, 0x4);

	if (PhobosFogTooltip::ShouldGateHiddenObjectCommand(pObject))
		R->Stack(0x4, static_cast<ObjectClass*>(nullptr));

	return 0;
}

DEFINE_HOOK(0x711F39, TechnoTypeClass_CostOf_FactoryPlant, 0x8)
{
	GET(TechnoTypeClass*, pThis, ESI);
	GET(HouseClass*, pHouse, EDI);
	REF_STACK(float, mult, STACK_OFFSET(0x10, -0x8));

	auto const pHouseExt = HouseExt::ExtMap.Find(pHouse);

	if (pHouseExt->RestrictedFactoryPlants.size() > 0)
		mult *= pHouseExt->GetRestrictedFactoryPlantMult(pThis);

	return 0;
}

DEFINE_HOOK(0x711FDF, TechnoTypeClass_RefundAmount_FactoryPlant, 0x8)
{
	GET(TechnoTypeClass*, pThis, ESI);
	GET(HouseClass*, pHouse, EDI);
	REF_STACK(float, mult, STACK_OFFSET(0x10, -0x4));

	auto const pHouseExt = HouseExt::ExtMap.Find(pHouse);

	if (pHouseExt->RestrictedFactoryPlants.size() > 0)
		mult *= pHouseExt->GetRestrictedFactoryPlantMult(pThis);

	return 0;
}

DEFINE_HOOK(0x71464A, TechnoTypeClass_ReadINI_Speed, 0x7)
{
	enum { SkipGameCode = 0x71469F };

	GET(TechnoTypeClass*, pThis, EBP);
	GET(CCINIClass*, pINI, ESI);
	GET(char*, pSection, EBX);
	GET(int, eliteAirstrikeRechargeTime, EAX);

	pThis->EliteAirstrikeRechargeTime = eliteAirstrikeRechargeTime; // Restore overridden instructions.
	INI_EX exINI(pINI);
	exINI.ReadSpeed(pSection, "Speed", &pThis->Speed);

	return SkipGameCode;
}

DEFINE_HOOK(0x747A2E, UnitTypeClass_ReadINI_TurretShape, 0x6)
{
	GET(UnitTypeClass*, pType, EDI);

	if (!pType->Voxel && pType->Turret)
	{
		char nameBuffer[0x19];
		char Buffer[260];
		const auto pArtSection = pType->ImageFile;

		if (Phobos::Config::ArtImageSwap &&
			CCINIClass::INI_Art.ReadString(pArtSection, "Image", 0, nameBuffer, 0x19) != 0)
		{
			_snprintf_s(Buffer, sizeof(Buffer), "%sTUR.SHP", nameBuffer);
		}
		else
		{
			_snprintf_s(Buffer, sizeof(Buffer), "%sTUR.SHP", pArtSection);
		}

		if (const auto pShape = FileSystem::LoadSHPFile(Buffer))
			TechnoTypeExt::ExtMap.Find(pType)->TurretShape = pShape;
	}

	return 0;
}
