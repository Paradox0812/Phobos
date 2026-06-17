#include "Body.h"

#include <BuildingClass.h>
#include <CellClass.h>
#include <Ext/House/Body.h>
#include <Ext/Rules/Body.h>
#include <Unsorted.h>
#include <Utilities/Debug.h>

namespace
{
	struct FoggedObjectSnapshot
	{
		char Padding0[0x30];
		AbstractType Type;
		CoordStruct Coords;
	};

	static_assert(offsetof(FoggedObjectSnapshot, Type) == 0x30);
	static_assert(offsetof(FoggedObjectSnapshot, Coords) == 0x34);

	struct BuildingVisibilityProbe
	{
		const char* Reason { "Unset" };
		CellStruct BaseCell {};
		int FoundationCells { 0 };
		int ValidCells { 0 };
		int InvalidCells { 0 };
		int QueryFailedCells { 0 };
		int VisibleCells { 0 };
		bool QuerySucceeded { false };
		bool Visible { false };
		bool ShouldHide { false };
	};

	const char* SafeText(const char* const pText)
	{
		return pText ? pText : "<null>";
	}

	const char* BoolText(const bool value)
	{
		return value ? "true" : "false";
	}

	const char* GetHouseID(HouseClass* const pHouse)
	{
		return SafeText(pHouse ? pHouse->get_ID() : nullptr);
	}

	const char* GetTechnoID(TechnoClass* const pTechno)
	{
		return SafeText(pTechno ? pTechno->get_ID() : nullptr);
	}

	bool ShouldLogPhobosFogBuildingProbe()
	{
		const auto pRulesExt = RulesExt::Global();

		return pRulesExt && pRulesExt->PhobosFog_Enabled && pRulesExt->PhobosFog_Debug && pRulesExt->PhobosFog_HideBuildings;
	}

	bool CanEmitPhobosFogBuildingProbeLog()
	{
		static int lastFrame = -1;
		static int frameLogs = 0;

		if (!ShouldLogPhobosFogBuildingProbe() || Unsorted::CurrentFrame % 30 != 0)
			return false;

		if (lastFrame != Unsorted::CurrentFrame)
		{
			lastFrame = Unsorted::CurrentFrame;
			frameLogs = 0;
		}

		if (frameLogs >= 32)
			return false;

		++frameLogs;
		return true;
	}

	void LogPhobosFogBuildingProbe(TechnoClass* const pTechno, HouseClass* const pViewerHouse, const BuildingVisibilityProbe& probe)
	{
		if (!CanEmitPhobosFogBuildingProbeLog())
			return;

		const auto pOwner = pTechno ? pTechno->Owner : nullptr;

		Debug::Log(
			"[PhobosFogBuilding] Frame=%d Origin=0x6D9134 Techno=%p ID=%s WhatAmI=%d Owner=%s Viewer=%s Reason=%s ShouldHide=%s Query=%s Visible=%s BaseCell=%d,%d Foundation=%d Valid=%d Invalid=%d QueryFailed=%d VisibleCells=%d\n",
			Unsorted::CurrentFrame,
			pTechno,
			GetTechnoID(pTechno),
			pTechno ? static_cast<int>(pTechno->WhatAmI()) : -1,
			GetHouseID(pOwner),
			GetHouseID(pViewerHouse),
			SafeText(probe.Reason),
			BoolText(probe.ShouldHide),
			BoolText(probe.QuerySucceeded),
			BoolText(probe.Visible),
			probe.BaseCell.X,
			probe.BaseCell.Y,
			probe.FoundationCells,
			probe.ValidCells,
			probe.InvalidCells,
			probe.QueryFailedCells,
			probe.VisibleCells);
	}

	bool IsPhobosFogCellVisibleToViewerOrAllies(HouseClass* const pViewerHouse, const int cellIndex)
	{
		bool querySucceeded = false;

		return HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);
	}

	bool IsPhobosFogCellVisibleToViewerOrAllies(HouseClass* const pViewerHouse, const int cellIndex, bool& querySucceeded)
	{
		return HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);
	}

	bool IsPhobosFogCellKnownToViewerOrAllies(HouseClass* const pViewerHouse, const int cellIndex, bool& querySucceeded)
	{
		querySucceeded = false;

		HouseExt::PhobosFogCellState state = HouseExt::PhobosFogCellState::Unknown;

		if (!HouseExt::ExtData::TryGetEffectivePhobosFogCellStateForViewerOrAllies(pViewerHouse, cellIndex, state))
			return false;

		querySucceeded = true;
		return state != HouseExt::PhobosFogCellState::Unknown;
	}

	bool IsBuildingFoundationVisibleToViewerOrAllies(BuildingClass* const pBuilding, HouseClass* const pViewerHouse, bool& querySucceeded, BuildingVisibilityProbe* const pProbe = nullptr)
	{
		querySucceeded = false;

		if (!pBuilding || !pBuilding->Type || !pViewerHouse)
		{
			if (pProbe)
				pProbe->Reason = "InvalidInput";

			return false;
		}

		if (pProbe)
			pProbe->BaseCell = pBuilding->GetMapCoords();

		auto const pFoundation = pBuilding->GetFoundationData(false);

		if (!pFoundation)
		{
			if (pProbe)
				pProbe->Reason = "NoFoundation";

			return false;
		}

		const auto baseCell = pBuilding->GetMapCoords();
		const CellStruct foundationEnd = { 0x7FFF, 0x7FFF };

		for (auto pCellOffset = pFoundation; *pCellOffset != foundationEnd; ++pCellOffset)
		{
			if (pProbe)
				++pProbe->FoundationCells;

			const auto cell = baseCell + *pCellOffset;

			if (!MapClass::Instance.TryGetCellAt(cell))
			{
				if (pProbe)
					++pProbe->InvalidCells;

				continue;
			}

			const int cellIndex = MapClass::GetCellIndex(cell);

			if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
			{
				if (pProbe)
					++pProbe->InvalidCells;

				continue;
			}

			if (pProbe)
				++pProbe->ValidCells;

			bool cellQuerySucceeded = false;
			const bool visible = IsPhobosFogCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, cellQuerySucceeded);

			if (!cellQuerySucceeded)
			{
				if (pProbe)
				{
					++pProbe->QueryFailedCells;
					pProbe->Reason = "CellQueryFailed";
				}

				querySucceeded = false;
				return false;
			}

			querySucceeded = true;

			if (visible)
			{
				if (pProbe)
				{
					++pProbe->VisibleCells;
					pProbe->QuerySucceeded = true;
					pProbe->Visible = true;
					pProbe->Reason = "VisibleFoundation";
					continue;
				}

				return true;
			}
		}

		if (pProbe)
		{
			pProbe->QuerySucceeded = querySucceeded;
			pProbe->Visible = pProbe->VisibleCells > 0;
			pProbe->Reason = querySucceeded ? (pProbe->Visible ? "VisibleFoundation" : "HiddenFoundation") : "NoValidFoundationCells";
		}

		if (pProbe && pProbe->Visible)
			return true;

		return false;
	}

	bool ShouldHidePhobosFogFoot(TechnoClass* const pTechno)
	{
		auto const pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled || !pRulesExt->PhobosFog_HideEnemyFoot
			|| !pTechno || !pTechno->Owner)
		{
			return false;
		}

		auto const pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver() || pViewerHouse->IsAlliedWith(pTechno->Owner))
			return false;

		const auto cell = pTechno->GetMapCoords();

		if (!MapClass::Instance.TryGetCellAt(cell))
			return false;

		const int cellIndex = MapClass::GetCellIndex(cell);

		if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
			return false;

		return !IsPhobosFogCellVisibleToViewerOrAllies(pViewerHouse, cellIndex);
	}

	bool ShouldHidePhobosFogBuilding(TechnoClass* const pTechno)
	{
		auto const pRulesExt = RulesExt::Global();
		auto const pViewerHouse = HouseClass::CurrentPlayer;
		BuildingVisibilityProbe probe {};

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled || !pRulesExt->PhobosFog_HideBuildings
			|| !pTechno || pTechno->WhatAmI() != AbstractType::Building)
		{
			probe.Reason = !pRulesExt ? "NoRules" : !pRulesExt->PhobosFog_Enabled ? "Disabled" : !pRulesExt->PhobosFog_HideBuildings ? "HideBuildingsOff" : !pTechno ? "NoTechno" : "NotBuilding";
			LogPhobosFogBuildingProbe(pTechno, pViewerHouse, probe);
			return false;
		}

		if (!pViewerHouse || pViewerHouse->IsObserver())
		{
			probe.Reason = "NoViewer";
			LogPhobosFogBuildingProbe(pTechno, pViewerHouse, probe);
			return false;
		}

		auto const pBuilding = static_cast<BuildingClass*>(pTechno);

		bool querySucceeded = false;
		const bool visible = IsBuildingFoundationVisibleToViewerOrAllies(
			pBuilding,
			pViewerHouse,
			querySucceeded,
			ShouldLogPhobosFogBuildingProbe() ? &probe : nullptr);

		const bool shouldHide = querySucceeded && !visible;

		probe.QuerySucceeded = querySucceeded;
		probe.Visible = visible;
		probe.ShouldHide = shouldHide;

		if (!querySucceeded && !ShouldLogPhobosFogBuildingProbe())
			probe.Reason = "QueryFailed";
		else if (querySucceeded && !visible && !ShouldLogPhobosFogBuildingProbe())
			probe.Reason = "Hidden";

		LogPhobosFogBuildingProbe(pTechno, pViewerHouse, probe);

		return shouldHide;
	}

	bool IsPhobosFogTechnoVisibleToViewerOrAllies(TechnoClass* const pTechno, bool& querySucceeded)
	{
		querySucceeded = false;

		if (!pTechno)
			return false;

		auto const pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver())
			return false;

		if (pTechno->WhatAmI() == AbstractType::Building)
			return IsBuildingFoundationVisibleToViewerOrAllies(static_cast<BuildingClass*>(pTechno), pViewerHouse, querySucceeded);

		const auto cell = pTechno->GetMapCoords();

		if (!MapClass::Instance.TryGetCellAt(cell))
			return false;

		const int cellIndex = MapClass::GetCellIndex(cell);

		if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
			return false;

		return IsPhobosFogCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);
	}

	bool ShouldSuppressPhobosFogHoverHealthBar(TechnoClass* const pTechno)
	{
		auto const pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled || !pRulesExt->PhobosFog_HideHoverHealthBar)
			return false;

		bool querySucceeded = false;
		const bool visible = IsPhobosFogTechnoVisibleToViewerOrAllies(pTechno, querySucceeded);

		return querySucceeded && !visible;
	}

	bool ShouldHidePhobosFogFoggedBuilding(const FoggedObjectSnapshot* const pFoggedObject)
	{
		auto const pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled || !pRulesExt->PhobosFog_HideBuildings
			|| !pFoggedObject || pFoggedObject->Type != AbstractType::Building)
		{
			return false;
		}

		auto const pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver())
			return false;

		auto const cell = CellClass::Coord2Cell(pFoggedObject->Coords);

		if (!MapClass::Instance.TryGetCellAt(cell))
			return true;

		auto const cellIndex = MapClass::GetCellIndex(cell);

		if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
			return true;

		bool querySucceeded = false;
		auto const known = IsPhobosFogCellKnownToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);

		return !querySucceeded || !known;
	}
}

DEFINE_HOOK_AGAIN(0x6D9134, TacticalClass_RenderLayers_DrawBefore, 0x5)// BuildingClass
DEFINE_HOOK(0x6D9076, TacticalClass_RenderLayers_DrawBefore, 0x5)// FootClass
{
	enum
	{
		SkipCurrentFootDraw = 0x6D9408,
		SkipCurrentBuildingDraw = 0x6D940C
	};

	GET(TechnoClass*, pTechno, ESI);

	if (R->Origin() == 0x6D9076 && ShouldHidePhobosFogFoot(pTechno))
		return SkipCurrentFootDraw;

	if (R->Origin() == 0x6D9134 && ShouldHidePhobosFogBuilding(pTechno))
		return SkipCurrentBuildingDraw;

	if (pTechno->IsSelected && Phobos::Config::EnableSelectBox)
	{
		const auto pTypeExt = TechnoExt::ExtMap.Find(pTechno)->TypeExtData;

		if (!pTypeExt->HealthBar_Hide && !pTypeExt->HideSelectBox)
		{
			GET(Point2D*, pLocation, EAX);
			TechnoExt::DrawSelectBox(pTechno, pLocation, &DSurface::ViewBounds, true);
		}
	}

	return 0;
}

DEFINE_HOOK(0x4D18F7, FoggedObjectClass_Draw_PhobosFogHideBuildings, 0x5)
{
	enum { SkipCurrentFoggedObjectDraw = 0x4D2311 };

	GET(FoggedObjectSnapshot*, pFoggedObject, EBX);

	if (ShouldHidePhobosFogFoggedBuilding(pFoggedObject))
		return SkipCurrentFoggedObjectDraw;

	return 0;
}

DEFINE_HOOK(0x6F5E37, TechnoClass_DrawExtras_DrawHealthBar, 0x6)
{
	enum { Permanent = 0x6F5E41 };

	GET(TechnoClass*, pThis, EBP);

	if (pThis && (pThis->IsMouseHovering || TechnoExt::ExtMap.Find(pThis)->TypeExtData->HealthBar_Permanent)
		&& !ShouldSuppressPhobosFogHoverHealthBar(pThis)
		&& !MapClass::Instance.IsLocationShrouded(pThis->GetCoords()))
	{
		return Permanent;
	}

	return 0;
}

DEFINE_HOOK(0x6F64A9, TechnoClass_DrawHealthBar_Hide, 0x5)
{
	enum { SkipDraw = 0x6F6AB6 };

	GET(TechnoClass*, pThis, ECX);

	const auto pTypeData = TechnoExt::ExtMap.Find(pThis)->TypeExtData;

	if (pTypeData->HealthBar_Hide)
		return SkipDraw;

	return 0;
}

DEFINE_HOOK(0x6F6637, TechnoClass_DrawHealthBar_HideBuildingsPips, 0x5)
{
	enum { SkipDrawPips = 0x6F677D };

	GET(TechnoClass*, pThis, ESI);

	const bool hidePips = TechnoExt::ExtMap.Find(pThis)->TypeExtData->HealthBar_HidePips;

	return hidePips ? SkipDrawPips : 0;
}

DEFINE_HOOK_AGAIN(0x6F6A58, TechnoClass_DrawHealthBar_PermanentPipScale, 0x6)	// DrawOther
DEFINE_HOOK(0x6F67E8, TechnoClass_DrawHealthBar_PermanentPipScale, 0xA)			// DrawBuilding
{
	enum { Permanent = 0x6F6AB6 };

	GET(TechnoClass*, pThis, ESI);

	const bool showPipScale = TechnoExt::ExtMap.Find(pThis)->TypeExtData->HealthBar_Permanent_PipScale;

	return !showPipScale && !pThis->IsMouseHovering && !pThis->IsSelected ? Permanent : 0;
}

DEFINE_HOOK(0x6F65D1, TechnoClass_DrawHealthBar_Buildings, 0x6)
{
	enum { SkipDraw = 0x6F6AB6 };

	GET(BuildingClass*, pThis, ESI);
	GET(const int, length, EBX);
	GET_STACK(RectangleStruct*, pBound, STACK_OFFSET(0x4C, 0x8));

	if (ShouldSuppressPhobosFogHoverHealthBar(pThis))
		return SkipDraw;

	const auto pExt = TechnoExt::ExtMap.Find(pThis);

	if (pThis->IsSelected && Phobos::Config::EnableSelectBox && !pExt->TypeExtData->HideSelectBox)
	{
		GET_STACK(Point2D*, pLocation, STACK_OFFSET(0x4C, 0x4));
		UNREFERENCED_PARAMETER(pLocation); // choom thought he was clever and recomputed the same shit again and again
		TechnoExt::DrawSelectBox(pThis, pLocation, pBound);
	}

	if (const auto pShieldData = pExt->Shield.get())
	{
		if (pShieldData->IsAvailable() && !pShieldData->IsBrokenAndNonRespawning())
			pShieldData->DrawShieldBar_Building(length, pBound);
	}

	TechnoExt::ProcessDigitalDisplays(pThis);

	return 0;
}

DEFINE_HOOK(0x6F683C, TechnoClass_DrawHealthBar_Units, 0x7)
{
	enum { SkipDrawPips = 0x6F6A58 };

	GET(FootClass*, pThis, ESI);
	GET_STACK(Point2D*, pLocation, STACK_OFFSET(0x4C, 0x4));
	GET_STACK(RectangleStruct*, pBound, STACK_OFFSET(0x4C, 0x8));

	const auto pExt = TechnoExt::ExtMap.Find(pThis);

	if (pThis->IsSelected && Phobos::Config::EnableSelectBox && !pExt->TypeExtData->HideSelectBox)
	{
		UNREFERENCED_PARAMETER(pLocation);
		TechnoExt::DrawSelectBox(pThis, pLocation, pBound);
	}

	if (const auto pShieldData = pExt->Shield.get())
	{
		if (pShieldData->IsAvailable() && !pShieldData->IsBrokenAndNonRespawning())
		{
			const int length = pThis->WhatAmI() == AbstractType::Infantry ? 8 : 17;
			pShieldData->DrawShieldBar_Other(length, pBound);
		}
	}

	TechnoExt::ProcessDigitalDisplays(pThis);

	if (pExt->TypeExtData->HealthBar_HidePips)
	{
		R->EDI(pLocation);
		return SkipDrawPips;
	}

	return 0;
}

DEFINE_HOOK(0x6F534E, TechnoClass_DrawExtras_Insignia, 0x5)
{
	enum { SkipGameCode = 0x6F5388 };

	GET(TechnoClass*, pThis, EBP);
	GET_STACK(Point2D*, pLocation, STACK_OFFSET(0x98, 0x4));
	GET(RectangleStruct*, pBounds, ESI);

	if (pThis->VisualCharacter(false, nullptr) != VisualType::Hidden)
	{
		if (RulesExt::Global()->DrawInsignia_OnlyOnSelected.Get() && !pThis->IsSelected && !pThis->IsMouseHovering)
			return SkipGameCode;
		else
			TechnoExt::DrawInsignia(pThis, pLocation, pBounds);
	}

	return SkipGameCode;
}

DEFINE_HOOK(0x709B2E, TechnoClass_DrawPips_Sizes, 0x5)
{
	GET(TechnoClass*, pThis, ECX);
	REF_STACK(int, pipWidth, STACK_OFFSET(0x74, -0x1C));

	Point2D size;
	const bool isBuilding = pThis->WhatAmI() == AbstractType::Building;
	auto const pType = pThis->GetTechnoType();

	if (pType->PipScale == PipScale::Ammo)
	{
		if (isBuilding)
			size = RulesExt::Global()->Pips_Ammo_Buildings_Size;
		else
			size = RulesExt::Global()->Pips_Ammo_Size;

		size = TechnoTypeExt::ExtMap.Find(pType)->AmmoPipSize.Get(size);
	}
	else
	{
		if (isBuilding)
			size = RulesExt::Global()->Pips_Generic_Buildings_Size;
		else
			size = RulesExt::Global()->Pips_Generic_Size;
	}

	pipWidth = size.X;
	R->ESI(size.Y);

	return 0;
}

DEFINE_HOOK(0x709B8B, TechnoClass_DrawPips_Spawns, 0x5)
{
	enum { SkipGameDrawing = 0x709C27 };

	GET(TechnoClass*, pThis, ECX);
	auto const pTypeExt = TechnoExt::ExtMap.Find(pThis)->TypeExtData;

	if (!pTypeExt->ShowSpawnsPips)
		return SkipGameDrawing;

	LEA_STACK(RectangleStruct*, offset, STACK_OFFSET(0x74, -0x24));
	GET_STACK(RectangleStruct*, rect, STACK_OFFSET(0x74, 0xC));
	GET_STACK(SHPStruct*, shape, STACK_OFFSET(0x74, -0x58));
	GET_STACK(const bool, isBuilding, STACK_OFFSET(0x74, -0x61));
	GET(const int, maxSpawnsCount, EBX);

	const int currentSpawnsCount = pThis->SpawnManager->CountDockedSpawns();
	auto const pipOffset = pTypeExt->SpawnsPipOffset.Get();
	Point2D position = { offset->X + pipOffset.X, offset->Y + pipOffset.Y };
	Point2D size;

	if (isBuilding)
		size = pTypeExt->SpawnsPipSize.Get(RulesExt::Global()->Pips_Generic_Buildings_Size);
	else
		size = pTypeExt->SpawnsPipSize.Get(RulesExt::Global()->Pips_Generic_Size);

	for (int i = 0; i < maxSpawnsCount; i++)
	{
		const int frame = i < currentSpawnsCount ? pTypeExt->SpawnsPipFrame : pTypeExt->EmptySpawnsPipFrame;

		DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, shape, frame,
			&position, rect, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);

		position.X += size.X;
		position.Y += size.Y;
	}

	return SkipGameDrawing;
}

DEFINE_HOOK(0x70A36E, TechnoClass_DrawPips_Ammo, 0x6)
{
	enum { SkipGameDrawing = 0x70A4EC };

	GET(TechnoClass*, pThis, ECX);
	LEA_STACK(RectangleStruct*, offset, STACK_OFFSET(0x74, -0x24));
	GET_STACK(RectangleStruct*, rect, STACK_OFFSET(0x74, 0xC));
	GET(const int, pipWrap, EBX);
	GET_STACK(const int, pipCount, STACK_OFFSET(0x74, -0x54));
	GET_STACK(const int, maxPips, STACK_OFFSET(0x74, -0x60));
	GET(const int, yOffset, ESI);

	auto const pTypeExt = TechnoExt::ExtMap.Find(pThis)->TypeExtData;
	auto const pipOffset = pTypeExt->AmmoPipOffset.Get();
	const int offsetWidth = offset->Width;
	Point2D position = { offset->X + pipOffset.X, offset->Y + pipOffset.Y };

	if (pipWrap > 0)
	{
		const int ammo = pThis->Ammo;
		const int levels = maxPips / pipWrap - 1;
		const int startFrame = pTypeExt->AmmoPipWrapStartFrame;

		for (int i = 0; i < pipWrap; i++)
		{
			int frame = startFrame;

			if (levels >= 0)
			{
				int counter = i + pipWrap * levels;
				int frameCounter = levels;
				bool calculateFrame = true;

				while (counter >= ammo)
				{
					frameCounter--;
					counter -= pipWrap;

					if (frameCounter < 0)
					{
						calculateFrame = false;
						break;
					}
				}

				if (calculateFrame)
					frame = frameCounter + frame + 1;
			}

			position.X += offsetWidth;
			position.Y += yOffset;

			DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, FileSystem::PIPS2_SHP,
				frame, &position, rect, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
		}
	}
	else
	{
		const int ammoFrame = pTypeExt->AmmoPipFrame;
		const int emptyFrame = pTypeExt->EmptyAmmoPipFrame;

		for (int i = 0; i < maxPips; i++)
		{
			if (i >= pipCount && emptyFrame < 0)
				break;

			const int frame = i >= pipCount ? emptyFrame : ammoFrame;

			DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, FileSystem::PIPS2_SHP,
				frame, &position, rect, BlitterFlags(0x600), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);

			position.X += offsetWidth;
			position.Y += yOffset;
		}
	}

	return SkipGameDrawing;
}

DEFINE_HOOK(0x70A1F6, TechnoClass_DrawPips_Tiberium, 0x6)
{
	enum { SkipGameDrawing = 0x70A4EC };

	GET(TechnoClass*, pThis, ECX);
	LEA_STACK(RectangleStruct*, offset, STACK_OFFSET(0x74, -0x24));
	GET_STACK(RectangleStruct*, rect, STACK_OFFSET(0x74, 0xC));
	GET_STACK(SHPStruct*, shape, STACK_OFFSET(0x74, -0x58));
	GET_STACK(const int, maxPips, STACK_OFFSET(0x74, -0x60));
	GET(const int, yOffset, ESI);

	Point2D position = { offset->X, offset->Y };
	const int totalStorage = pThis->GetTechnoType()->Storage;

	std::vector<int> pipsToDraw;
	pipsToDraw.reserve(maxPips);

	bool isWeeder = false;
	auto const whatAmI = pThis->WhatAmI();

	switch (whatAmI)
	{
	case AbstractType::Building:
		isWeeder = static_cast<BuildingClass*>(pThis)->Type->Weeder;
		break;
	case AbstractType::Unit:
		isWeeder = static_cast<UnitClass*>(pThis)->Type->Weeder;
		break;
	default:
		break;
	}

	if (isWeeder)
	{
		const int fullWeedFrames = whatAmI == AbstractType::Building
			? static_cast<int>(pThis->Owner->GetWeedStoragePercentage() * maxPips + 0.5)
			: static_cast<int>(pThis->Tiberium.GetTotalAmount() / totalStorage * maxPips + 0.5);

		for (int i = 0; i < maxPips; i++)
		{
			if (i < fullWeedFrames)
				pipsToDraw.push_back(RulesExt::Global()->Pips_Tiberiums_WeedFrame);
			else
				pipsToDraw.push_back(RulesExt::Global()->Pips_Tiberiums_WeedEmptyFrame);
		}
	}
	else
	{
		const int count = TiberiumClass::Array.Count;
		std::vector<int> tiberiumPipCounts(count);

		for (size_t i = 0; i < tiberiumPipCounts.size(); i++)
		{
			tiberiumPipCounts[i] = static_cast<int>(pThis->Tiberium.GetAmount(i) / totalStorage * maxPips + 0.5);
		}

		auto const rawPipOrder = RulesExt::Global()->Pips_Tiberiums_DisplayOrder.empty() ? std::vector<int>{ 0, 2, 3, 1 } : RulesExt::Global()->Pips_Tiberiums_DisplayOrder;
		auto const& pipFrames = RulesExt::Global()->Pips_Tiberiums_Frames;
		int const emptyFrame = RulesExt::Global()->Pips_Tiberiums_EmptyFrame;

		std::vector<int> pipOrder;
		pipOrder.reserve(count);

		// First make a new vector, removing all the duplicate and invalid tiberiums
		for (int index : rawPipOrder)
		{
			if (std::find(pipOrder.begin(), pipOrder.end(), index) == pipOrder.end()
				&& index >= 0 && index < count)
			{
				pipOrder.push_back(index);
			}
		}

		// Then add any tiberium types that are missing
		for (int i = 0; i < count; i++)
		{
			if (std::find(pipOrder.begin(), pipOrder.end(), i) == pipOrder.end())
			{
				pipOrder.push_back(i);
			}
		}

		for (int i = 0; i < maxPips; i++)
		{
			for (const int index : pipOrder)
			{
				if (tiberiumPipCounts[index] > 0)
				{
					tiberiumPipCounts[index]--;

					if (static_cast<size_t>(index) >= pipFrames.size())
						pipsToDraw.push_back(index == 1 ? 5 : 2);
					else
						pipsToDraw.push_back(pipFrames.at(index));

					break;
				}
			}

			if (pipsToDraw.size() <= static_cast<size_t>(i))
				pipsToDraw.push_back(emptyFrame);
		}
	}

	const int offsetWidth = offset->Width;

	for (const int pip : pipsToDraw)
	{
		DSurface::Temp->DrawSHP(FileSystem::PALETTE_PAL, shape, pip,
			&position, rect, BlitterFlags::Centered | BlitterFlags::bf_400, 0, 0,
			ZGradient::Ground, 1000, 0, nullptr, 0, 0, 0);

		position.X += offsetWidth;
		position.Y += yOffset;
	}

	return SkipGameDrawing;
}

DEFINE_HOOK(0x70A4FB, TechnoClass_DrawPips_SelfHealGain, 0x5)
{
	enum { SkipGameDrawing = 0x70A6C0 };

	GET(TechnoClass*, pThis, ECX);
	GET_STACK(Point2D*, pLocation, STACK_OFFSET(0x74, 0x4));
	GET_STACK(RectangleStruct*, pBounds, STACK_OFFSET(0x74, 0xC));

	TechnoExt::DrawSelfHealPips(pThis, pLocation, pBounds);

	return SkipGameDrawing;
}
