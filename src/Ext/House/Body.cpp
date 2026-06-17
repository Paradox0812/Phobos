#include "Body.h"

#include <MapClass.h>

#include <Ext/Rules/Body.h>
#include <Ext/SWType/Body.h>
#include <Ext/Techno/Body.h>

#include <Helpers/Iterators.h>
#include <Utilities/Debug.h>
#include <Unsorted.h>

#include <algorithm>
#include <utility>

//Static init

HouseExt::ExtContainer HouseExt::ExtMap;

std::vector<int> HouseExt::AIProduction_CreationFrames;
std::vector<int> HouseExt::AIProduction_Values;
std::vector<int> HouseExt::AIProduction_BestChoices;
std::vector<int> HouseExt::AIProduction_BestChoicesNaval;
bool HouseExt::PhobosFog_ForceNextRefresh = false;

namespace
{
	constexpr unsigned int PhobosFogPayloadMagic = 0x474F4650u; // PFOG
	constexpr unsigned int PhobosFogPayloadVersion = 1u;

	unsigned int GetPhobosFogExploredPayloadByteCount(unsigned int cellCount)
	{
		return (cellCount + 7u) / 8u;
	}

	bool SkipPhobosFogPayloadBytes(PhobosStreamReader& Stm, unsigned int byteCount)
	{
		unsigned char buffer[256] {};
		auto remaining = byteCount;

		while (remaining > 0u)
		{
			const auto chunk = std::min<unsigned int>(remaining, sizeof(buffer));

			if (!Stm.Read(buffer, chunk))
				return false;

			remaining -= chunk;
		}

		return true;
	}
}

// Based on Ares' rewrite of 0x4FEA60 for 100 unit bugfix.
void HouseExt::ExtData::UpdateVehicleProduction()
{
	const auto pThis = this->OwnerObject();
	const bool skipGround = pThis->ProducingUnitTypeIndex != -1;
	const bool skipNaval = this->ProducingNavalUnitTypeIndex != -1;

	if (skipGround && skipNaval)
		return;

	if (!skipGround && this->UpdateHarvesterProduction())
		return;

	const int AIDifficulty = pThis->GetAIDifficultyIndex();
	auto& creationFrames = HouseExt::AIProduction_CreationFrames;
	auto& values = HouseExt::AIProduction_Values;
	auto& bestChoices = HouseExt::AIProduction_BestChoices;
	auto& bestChoicesNaval = HouseExt::AIProduction_BestChoicesNaval;

	auto const count = static_cast<unsigned int>(UnitTypeClass::Array.Count);
	creationFrames.reserve(count);
	creationFrames.assign(count, 0x7FFFFFFF);
	values.reserve(count);
	values.assign(count, 0);

	for (auto const currentTeam : TeamClass::Array)
	{
		if (!currentTeam || currentTeam->Owner != pThis)
			continue;

		const int teamCreationFrame = currentTeam->CreationFrame;

		if ((!currentTeam->Type->Reinforce || currentTeam->IsFullStrength)
			&& (currentTeam->IsForcedActive || currentTeam->IsHasBeen))
		{
			continue;
		}

		DynamicVectorClass<TechnoTypeClass*> taskForceMembers;
		currentTeam->GetTaskForceMissingMemberTypes(taskForceMembers);

		for (auto const currentMember : taskForceMembers)
		{
			if (currentMember->WhatAmI() != UnitTypeClass::AbsID
				|| (skipGround && !currentMember->Naval)
				|| (skipNaval && currentMember->Naval))
			{
				continue;
			}

			auto const index = static_cast<unsigned int>(currentMember->GetArrayIndex());
			++values[index];

			if (teamCreationFrame < creationFrames[index])
				creationFrames[index] = teamCreationFrame;
		}
	}

	for (auto const unit : UnitClass::Array)
	{
		auto const index = static_cast<unsigned int>(unit->Type->GetArrayIndex());

		if (values[index] > 0 && unit->CanBeRecruited(pThis))
			--values[index];
	}

	bestChoices.clear();
	bestChoicesNaval.clear();

	int bestValue = -1;
	int bestValueNaval = -1;
	int earliestTypenameIndex = -1;
	int earliestTypenameIndexNaval = -1;
	int earliestFrame = 0x7FFFFFFF;
	int earliestFrameNaval = 0x7FFFFFFF;
	const long money = pThis->Available_Money();

	for (auto i = 0u; i < count; ++i)
	{
		auto const type = UnitTypeClass::Array[static_cast<int>(i)];
		const int currentValue = values[i];

		if (currentValue <= 0
			|| pThis->CanBuild(type, false, false) == CanBuildResult::Unbuildable
			|| type->GetActualCost(pThis) > money)
		{
			continue;
		}

		const bool isNaval = type->Naval;
		int* cBestValue = !isNaval ? &bestValue : &bestValueNaval;
		std::vector<int>* cBestChoices = !isNaval ? &bestChoices : &bestChoicesNaval;

		if (*cBestValue < currentValue || *cBestValue == -1)
		{
			*cBestValue = currentValue;
			cBestChoices->clear();
		}

		cBestChoices->push_back(static_cast<int>(i));

		int* cEarliestTypeNameIndex = !isNaval ? &earliestTypenameIndex : &earliestTypenameIndexNaval;
		int* cEarliestFrame = !isNaval ? &earliestFrame : &earliestFrameNaval;

		if (*cEarliestFrame > creationFrames[i] || *cEarliestTypeNameIndex == -1)
		{
			*cEarliestTypeNameIndex = static_cast<int>(i);
			*cEarliestFrame = creationFrames[i];
		}
	}

	const int earliestOdds = RulesClass::Instance->FillEarliestTeamProbability[AIDifficulty];

	if (!skipGround)
	{
		if (ScenarioClass::Instance->Random.RandomRanged(0, 99) < earliestOdds)
		{
			pThis->ProducingUnitTypeIndex = earliestTypenameIndex;
		}
		else if (auto const size = static_cast<int>(bestChoices.size()))
		{
			const int randomChoice = ScenarioClass::Instance->Random.RandomRanged(0, size - 1);
			pThis->ProducingUnitTypeIndex = bestChoices[static_cast<unsigned int>(randomChoice)];
		}
	}

	if (!skipNaval)
	{
		if (ScenarioClass::Instance->Random.RandomRanged(0, 99) < earliestOdds)
		{
			this->ProducingNavalUnitTypeIndex = earliestTypenameIndexNaval;
		}
		else if (auto const size = static_cast<int>(bestChoicesNaval.size()))
		{
			const int randomChoice = ScenarioClass::Instance->Random.RandomRanged(0, size - 1);
			this->ProducingNavalUnitTypeIndex = bestChoicesNaval[static_cast<unsigned int>(randomChoice)];
		}
	}
}

bool HouseExt::ExtData::UpdateHarvesterProduction()
{
	auto const pThis = this->OwnerObject();
	const int AIDifficulty = pThis->GetAIDifficultyIndex();
	const int idxParentCountry = pThis->Type->FindParentCountryIndex();
	auto const pHarvesterUnit = HouseExt::FindOwned(pThis, idxParentCountry, make_iterator(RulesClass::Instance->HarvesterUnit));

	if (pHarvesterUnit)
	{
		auto const harvesters = pThis->CountResourceGatherers;
		auto const maxHarvesters = HouseExt::FindBuildable(
			pThis, idxParentCountry, make_iterator(RulesClass::Instance->BuildRefinery))
			? RulesClass::Instance->HarvestersPerRefinery[AIDifficulty] * pThis->CountResourceDestinations
			: RulesClass::Instance->AISlaveMinerNumber[AIDifficulty];

		if (pThis->IQLevel2 >= RulesClass::Instance->Harvester && !pThis->IsTiberiumShort
			&& !pThis->IsControlledByHuman() && harvesters < maxHarvesters
			&& pThis->TechLevel >= pHarvesterUnit->TechLevel)
		{
			pThis->ProducingUnitTypeIndex = pHarvesterUnit->ArrayIndex;
			return true;
		}
	}
	else
	{
		auto const maxHarvesters = RulesClass::Instance->AISlaveMinerNumber[AIDifficulty];

		if (pThis->CountResourceGatherers < maxHarvesters)
		{
			auto const pRefinery = HouseExt::FindBuildable(
				pThis, idxParentCountry, make_iterator(RulesClass::Instance->BuildRefinery));

			if (pRefinery)
			{
				if (auto const pSlaveMiner = pRefinery->UndeploysInto)
				{
					if (pSlaveMiner->ResourceDestination)
					{
						pThis->ProducingUnitTypeIndex = pSlaveMiner->ArrayIndex;
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool HouseExt::ExtData::OwnsLimboDeliveredBuilding(BuildingClass* pBuilding) const
{
	if (!pBuilding)
		return false;

	auto& vec = this->OwnedLimboDeliveredBuildings;
	return std::find(vec.begin(), vec.end(), pBuilding) != vec.end();
}

size_t HouseExt::FindOwnedIndex(
	HouseClass const* const, int const idxParentCountry,
	Iterator<TechnoTypeClass const*> const items, size_t const start)
{
	auto const bitOwner = 1u << idxParentCountry;

	for (size_t i = start; i < items.size(); ++i)
	{
		auto const pItem = items[i];

		if (pItem->InOwners(bitOwner))
			return i;
	}

	return items.size();
}

bool HouseExt::IsDisabledFromShell(
	HouseClass const* const pHouse, BuildingTypeClass const* const pItem)
{
	// SWAllowed does not apply to campaigns any more
	if (SessionClass::IsCampaign()
		|| GameModeOptionsClass::Instance.SWAllowed)
	{
		return false;
	}

	if (pItem->SuperWeapon != -1)
	{
		// allow SWs only if not disableable from shell
		auto const pItem2 = const_cast<BuildingTypeClass*>(pItem);
		auto const& BuildTech = RulesClass::Instance->BuildTech;

		if (BuildTech.FindItemIndex(pItem2) == -1)
		{
			auto const pSuper = pHouse->Supers[pItem->SuperWeapon];
			if (pSuper->Type->DisableableFromShell)
				return true;
		}
	}

	return false;
}

size_t HouseExt::FindBuildableIndex(
	HouseClass const* const pHouse, int const idxParentCountry,
	Iterator<TechnoTypeClass const*> const items, size_t const start)
{
	for (auto i = start; i < items.size(); ++i)
	{
		auto const pItem = items[i];

		if (pHouse->CanExpectToBuild(pItem, idxParentCountry))
		{
			auto const pBld = abstract_cast<const BuildingTypeClass*, true>(pItem);
			if (pBld && HouseExt::IsDisabledFromShell(pHouse, pBld))
				continue;

			return i;
		}
	}

	return items.size();
}

int HouseExt::ActiveHarvesterCount(HouseClass* pThis)
{
	int result = 0;
	auto const pExt = HouseExt::ExtMap.Find(pThis);

	for (auto const pTechno : pExt->OwnedCountedHarvesters)
	{
		result += TechnoExt::IsHarvesting(pTechno);
	}

	return result;
}

int HouseExt::TotalHarvesterCount(HouseClass* pThis)
{
	int result = 0;
	auto const pHouseExt = HouseExt::ExtMap.Find(pThis);

	for (auto const pTechno : pHouseExt->OwnedCountedHarvesters)
	{
		auto const pExt = TechnoExt::ExtMap.Find(pTechno);
		result += pExt->HasBeenPlacedOnMap;
	}

	return result;
}

// This basically gets same cell that AI script action 53 Gather at Enemy Base uses, and code for that (0x6EF700) was used as reference here.
CellClass* HouseExt::GetEnemyBaseGatherCell(HouseClass* pTargetHouse, HouseClass* pCurrentHouse, CoordStruct defaultCurrentCoords, SpeedType speedTypeZone, int extraDistance)
{
	if (!pTargetHouse || !pCurrentHouse)
		return nullptr;

	const auto targetCoords = CellClass::Cell2Coord(pTargetHouse->GetBaseCenter());

	if (targetCoords == CoordStruct::Empty)
		return nullptr;

	auto currentCoords = CellClass::Cell2Coord(pCurrentHouse->GetBaseCenter());

	if (currentCoords == CoordStruct::Empty)
		currentCoords = defaultCurrentCoords;

	const int distance = (RulesClass::Instance->AISafeDistance + extraDistance) * Unsorted::LeptonsPerCell;
	const auto newCoords = GeneralUtils::CalculateCoordsFromDistance(currentCoords, targetCoords, distance);

	auto cellStruct = CellClass::Coord2Cell(newCoords);
	cellStruct = MapClass::Instance.NearByLocation(cellStruct, speedTypeZone, -1, MovementZone::Normal, false, 3, 3, false, false, false, true, cellStruct, false, false);

	if (cellStruct == CellStruct::Empty)
		return nullptr;

	return MapClass::Instance.TryGetCellAt(cellStruct);
}

// Gets the superweapons used by AI for Chronoshift script actions.
void HouseExt::GetAIChronoshiftSupers(HouseClass* pThis, SuperClass*& pSuperCSphere, SuperClass*& pSuperCWarp)
{
	const int idxCS = RulesExt::Global()->AIChronoSphereSW;
	const int idxCW = RulesExt::Global()->AIChronoWarpSW;

	if (idxCS >= 0)
	{
		pSuperCSphere = pThis->Supers[idxCS];

		if (idxCW < 0)
		{
			auto const pSWTypeExt = SWTypeExt::ExtMap.Find(pSuperCSphere->Type);

			if (pSWTypeExt->SW_PostDependent >= 0)
				pSuperCWarp = pThis->Supers[pSWTypeExt->SW_PostDependent];
		}
	}

	if (idxCW >= 0)
		pSuperCWarp = pThis->Supers[idxCW];

	if (pSuperCSphere && pSuperCWarp)
		return;

	for (auto const pSuper : pThis->Supers)
	{
		auto const pType = pSuper->Type->Type;

		if (pType == SuperWeaponType::ChronoSphere)
			pSuperCSphere = pSuper;

		if (pType == SuperWeaponType::ChronoWarp)
			pSuperCWarp = pSuper;
	}
}

// Ares
HouseClass* HouseExt::GetHouseKind(OwnerHouseKind const kind, bool const allowRandom, HouseClass* const pDefault, HouseClass* const pInvoker, HouseClass* const pVictim)
{
	switch (kind)
	{
	case OwnerHouseKind::Invoker:
	case OwnerHouseKind::Killer:
		return pInvoker ? pInvoker : pDefault;
	case OwnerHouseKind::Victim:
		return pVictim ? pVictim : pDefault;
	case OwnerHouseKind::Civilian:
		return HouseClass::FindCivilianSide();
	case OwnerHouseKind::Special:
		return HouseClass::FindSpecial();
	case OwnerHouseKind::Neutral:
		return HouseClass::FindNeutral();
	case OwnerHouseKind::Random:
		if (allowRandom)
			return HouseClass::Array.GetItem(ScenarioClass::Instance->Random.RandomRanged(0, HouseClass::Array.Count - 1));
		else
			return pDefault;
	case OwnerHouseKind::Default:
	default:
		return pDefault;
	}
}

void HouseExt::ExtData::AddToLimboTracking(TechnoTypeClass* pTechnoType)
{
	if (pTechnoType)
	{
		const int arrayIndex = pTechnoType->GetArrayIndex();

		switch (pTechnoType->WhatAmI())
		{
		case AbstractType::AircraftType:
			this->LimboAircraft.Increment(arrayIndex);
			break;
		case AbstractType::BuildingType:
			this->LimboBuildings.Increment(arrayIndex);
			break;
		case AbstractType::InfantryType:
			this->LimboInfantry.Increment(arrayIndex);
			break;
		case AbstractType::UnitType:
			this->LimboVehicles.Increment(arrayIndex);
			break;
		default:
			break;
		}
	}
}

void HouseExt::ExtData::RemoveFromLimboTracking(TechnoTypeClass* pTechnoType)
{
	if (pTechnoType)
	{
		const int arrayIndex = pTechnoType->GetArrayIndex();

		switch (pTechnoType->WhatAmI())
		{
		case AbstractType::AircraftType:
			this->LimboAircraft.Decrement(arrayIndex);
			break;
		case AbstractType::BuildingType:
			this->LimboBuildings.Decrement(arrayIndex);
			break;
		case AbstractType::InfantryType:
			this->LimboInfantry.Decrement(arrayIndex);
			break;
		case AbstractType::UnitType:
			this->LimboVehicles.Decrement(arrayIndex);
			break;
		default:
			break;
		}
	}
}

int HouseExt::ExtData::CountOwnedPresentAndLimboed(TechnoTypeClass* pTechnoType) const
{
	int count = this->OwnerObject()->CountOwnedAndPresent(pTechnoType);
	const int arrayIndex = pTechnoType->GetArrayIndex();

	switch (pTechnoType->WhatAmI())
	{
	case AbstractType::AircraftType:
		count += this->LimboAircraft.GetItemCount(arrayIndex);
		break;
	case AbstractType::BuildingType:
		count += this->LimboBuildings.GetItemCount(arrayIndex);
		break;
	case AbstractType::InfantryType:
		count += this->LimboInfantry.GetItemCount(arrayIndex);
		break;
	case AbstractType::UnitType:
		count += this->LimboVehicles.GetItemCount(arrayIndex);
		break;
	default:
		break;
	}

	return count;
}

void HouseExt::ExtData::UpdateNonMFBFactoryCounts(AbstractType rtti, bool remove, bool isNaval)
{
	int* count = nullptr;

	switch (rtti)
	{
	case AbstractType::Aircraft:
	case AbstractType::AircraftType:
		count = &this->NumAirpads_NonMFB;
		break;
	case AbstractType::Building:
	case AbstractType::BuildingType:
		count = &this->NumConYards_NonMFB;
		break;
	case AbstractType::Infantry:
	case AbstractType::InfantryType:
		count = &this->NumBarracks_NonMFB;
		break;
	case AbstractType::Unit:
	case AbstractType::UnitType:
		count = isNaval ? &this->NumShipyards_NonMFB : &this->NumWarFactories_NonMFB;
		break;
	default:
		break;
	}

	if (count)
		*count += remove ? -1 : 1;
}

int HouseExt::ExtData::GetFactoryCountWithoutNonMFB(AbstractType rtti, bool isNaval) const
{
	auto const pThis = this->OwnerObject();
	int count = 0;

	switch (rtti)
	{
	case AbstractType::Aircraft:
	case AbstractType::AircraftType:
		count = pThis->NumAirpads - this->NumAirpads_NonMFB;
		break;
	case AbstractType::Building:
	case AbstractType::BuildingType:
		count = pThis->NumConYards - this->NumConYards_NonMFB;
		break;
	case AbstractType::Infantry:
	case AbstractType::InfantryType:
		count = pThis->NumBarracks - this->NumBarracks_NonMFB;
		break;
	case AbstractType::Unit:
	case AbstractType::UnitType:
		if (isNaval)
			count = pThis->NumShipyards - this->NumShipyards_NonMFB;
		else
			count = pThis->NumWarFactories - this->NumWarFactories_NonMFB;
		break;
	default:
		break;
	}

	return Math::max(count, 0);
}

float HouseExt::ExtData::GetRestrictedFactoryPlantMult(TechnoTypeClass* pTechnoType) const
{
	float mult = 1.0f;
	auto const pTechnoTypeExt = TechnoTypeExt::ExtMap.Find(pTechnoType);
	std::unordered_map<int, int> counts;

	for (auto const pBuilding : this->RestrictedFactoryPlants)
	{
		auto const pType = pBuilding->Type;
		auto const pTypeExt = BuildingTypeExt::ExtMap.Find(pType);
		const int max = pTypeExt->FactoryPlant_MaxCount;

		if (max > -1 && counts[pType->ArrayIndex] >= max)
			continue;

		if (pTypeExt->FactoryPlant_AllowTypes.size() > 0 && !pTypeExt->FactoryPlant_AllowTypes.Contains(pTechnoType))
			continue;

		if (pTypeExt->FactoryPlant_DisallowTypes.size() > 0 && pTypeExt->FactoryPlant_DisallowTypes.Contains(pTechnoType))
			continue;

		counts[pType->ArrayIndex]++;
		float currentMult = 1.0f;

		switch (pTechnoType->WhatAmI())
		{
		case AbstractType::BuildingType:
			if (((BuildingTypeClass*)pTechnoType)->BuildCat == BuildCat::Combat)
				currentMult = pType->DefensesCostBonus;
			else
				currentMult = pType->BuildingsCostBonus;
			break;
		case AbstractType::AircraftType:
			currentMult = pType->AircraftCostBonus;
			break;
		case AbstractType::InfantryType:
			currentMult = pType->InfantryCostBonus;
			break;
		case AbstractType::UnitType:
			currentMult = pType->UnitsCostBonus;
			break;
		default:
			break;
		}

		mult *= currentMult;
	}

	return 1.0f - ((1.0f - mult) * pTechnoTypeExt->FactoryPlant_Multiplier);
}

void HouseExt::ForceOnlyTargetHouseEnemy(HouseClass* pThis, int mode)
{
	const auto pHouseExt = HouseExt::ExtMap.Find(pThis);

	if (mode < 0 || mode > 2)
		mode = -1;

	enum { ForceFalse = 0, ForceTrue = 1, ForceRandom = 2};
	pHouseExt->ForceOnlyTargetHouseEnemyMode = mode;

	switch (mode)
	{
		case ForceFalse:
			pHouseExt->ForceOnlyTargetHouseEnemy = false;
			break;

		case ForceTrue:
			pHouseExt->ForceOnlyTargetHouseEnemy = true;
			break;

		case ForceRandom:
			pHouseExt->ForceOnlyTargetHouseEnemy = (bool)ScenarioClass::Instance->Random.RandomRanged(0, 1);;
			break;

		default:
			pHouseExt->ForceOnlyTargetHouseEnemy = false;
			break;
	}
}

void HouseExt::ExtData::LoadFromINIFile(CCINIClass* const pINI)
{
	const char* pSection = this->OwnerObject()->PlainName;

	INI_EX exINI(pINI);

	ValueableVector<bool> readBaseNodeRepairInfo;
	readBaseNodeRepairInfo.Read(exINI, pSection, "RepairBaseNodes");
	size_t nWritten = readBaseNodeRepairInfo.size();

	if (nWritten <= 3)
	{
		for (size_t i = 0; i < nWritten; i++)
		{
			this->RepairBaseNodes[i] = readBaseNodeRepairInfo[i];
		}
	}
}

int HouseExt::ExtData::GetForceEnemyIndex()
{
	auto const pHouse = this->OwnerObject();
	if (!pHouse)
		return -1;

	return this->ForceEnemyIndex;
}

void HouseExt::ExtData::SetForceEnemyIndex(int EnemyIndex)
{
	if (EnemyIndex < 0 && EnemyIndex != -2)
		this->ForceEnemyIndex = -1;
	else
		this->ForceEnemyIndex = EnemyIndex;
}

void HouseExt::CalculatePowerSurplus(HouseClass* pThis)
{
	auto const pRulesExt = RulesExt::Global();

	if (!pRulesExt->EnablePowerSurplus)
		return;

	int scaleToDrainAmount = pRulesExt->PowerSurplus_ScaleToDrainAmount;

	if (scaleToDrainAmount <= 0)
	{
		pThis->PowerSurplus = RulesClass::Instance->PowerSurplus;
	}
	else
	{
		double factor = pThis->PowerDrain / static_cast<double>(scaleToDrainAmount);
		pThis->PowerSurplus = static_cast<int>(RulesClass::Instance->PowerSurplus * factor);
	}
}

// =============================
// load / save

template <typename T>
void HouseExt::ExtData::Serialize(T& Stm)
{
	Stm
		.Process(this->PowerPlantEnhancers)
		.Process(this->OwnedLimboDeliveredBuildings)
		.Process(this->OwnedCountedHarvesters)
		.Process(this->LimboAircraft)
		.Process(this->LimboBuildings)
		.Process(this->LimboInfantry)
		.Process(this->LimboVehicles)
		.Process(this->Factory_BuildingType)
		.Process(this->Factory_InfantryType)
		.Process(this->Factory_VehicleType)
		.Process(this->Factory_NavyType)
		.Process(this->Factory_AircraftType)
		.Process(this->AISuperWeaponDelayTimer)
		.Process(this->RepairBaseNodes)
		.Process(this->RestrictedFactoryPlants)
		.Process(this->LastBuiltNavalVehicleType)
		.Process(this->ProducingNavalUnitTypeIndex)
		.Process(this->CombatAlertTimer)
		.Process(this->NumAirpads_NonMFB)
		.Process(this->NumBarracks_NonMFB)
		.Process(this->NumWarFactories_NonMFB)
		.Process(this->NumConYards_NonMFB)
		.Process(this->NumShipyards_NonMFB)
		.Process(this->AIFireSaleDelayTimer)
		.Process(this->SuspendedEMPulseSWs)
		.Process(this->SuperExts)
		.Process(this->ForceEnemyIndex)
		.Process(this->ForceOnlyTargetHouseEnemy)
		.Process(this->ForceOnlyTargetHouseEnemyMode)
		.Process(this->TeamDelay)
		.Process(this->FreeRadar)
		.Process(this->ForceRadar)
		.Process(this->PlayerAutoRepair)
		;
}

void HouseExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<HouseClass>::LoadFromStream(Stm);
	this->Serialize(Stm);

	std::vector<PhobosFogCellState> loadedPhobosFogCellStates;
	bool phobosFogPayloadFound = false;

	if (!this->LoadPhobosFogExploredPayload(Stm, loadedPhobosFogCellStates, phobosFogPayloadFound))
		return;

	this->PhobosFog_CellStates.clear();
	this->ResetPhobosFogRuntimeStateOnly(true);

	if (phobosFogPayloadFound && loadedPhobosFogCellStates.size() == static_cast<size_t>(MapClass::MaxCells))
	{
		this->PhobosFog_CellStates = std::move(loadedPhobosFogCellStates);
	}
	else
	{
		this->PhobosFog_CellStates.assign(static_cast<size_t>(MapClass::MaxCells), PhobosFogCellState::Unknown);
	}

	this->PhobosFog_LastVisibleFrames.assign(static_cast<size_t>(MapClass::MaxCells), 0);
	this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::Reset);
	this->TouchPhobosFogOverlayEffectiveVersion();
	HouseExt::RequestPhobosFogForceRefresh();
}

void HouseExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Extension<HouseClass>::SaveToStream(Stm);
	this->Serialize(Stm);
	this->SavePhobosFogExploredPayload(Stm);
}

bool HouseExt::LoadGlobals(PhobosStreamReader& Stm)
{
	return Stm
		.Success();
}

bool HouseExt::SaveGlobals(PhobosStreamWriter& Stm)
{
	return Stm
		.Success();
}

void HouseExt::RequestPhobosFogForceRefresh()
{
	HouseExt::PhobosFog_ForceNextRefresh = true;
}

bool HouseExt::ConsumePhobosFogForceRefresh()
{
	const bool result = HouseExt::PhobosFog_ForceNextRefresh;
	HouseExt::PhobosFog_ForceNextRefresh = false;

	return result;
}

void HouseExt::ExtData::ResetPhobosFogState()
{
	this->PhobosFog_CellStates.clear();
	this->ResetPhobosFogRuntimeStateOnly(false);
	this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::Reset);
	this->TouchPhobosFogOverlayEffectiveVersion();
}

void HouseExt::ExtData::ResetPhobosFogRuntimeStateOnly(bool initializeSpySatEdgeState)
{
	this->PhobosFog_LastVisibleFrames.clear();
	this->PhobosFog_FullMapVisibleUntilFrame = 0;
	this->PhobosFog_LastSpySatActive = initializeSpySatEdgeState && this->OwnerObject()
		? this->OwnerObject()->SpySatActive
		: false;
	this->PhobosFog_LastFullMapHardVisible = initializeSpySatEdgeState
		? this->IsPhobosFogFullMapHardVisible()
		: false;
	this->AbortPhobosFogOverlayEffectiveBatch();
	this->PhobosFog_OverlayEffectiveBatchCellStamps.clear();
	this->PhobosFog_OverlayEffectiveBatchStamp = 1;
	this->PhobosFog_StateVersionTouchCount = 0;
	for (auto& reasonCount : this->PhobosFog_StateVersionTouchReasons)
		reasonCount = 0ULL;
	this->PhobosFog_OverlayEffectiveTouchCount = 0;
	this->ResetPhobosFogDebugRefreshStats(false);
}

bool HouseExt::ExtData::LoadPhobosFogExploredPayload(
	PhobosStreamReader& Stm,
	std::vector<PhobosFogCellState>& cellStates,
	bool& payloadFound) const
{
	payloadFound = false;
	cellStates.clear();

	if (!Stm.HasRemainingBytes(sizeof(unsigned int)))
		return true;

	unsigned int magic = 0u;

	if (!Stm.TryPeekUInt32(magic) || magic != PhobosFogPayloadMagic)
		return true;

	payloadFound = true;

	unsigned int version = 0u;
	unsigned int cellCount = 0u;
	unsigned int byteCount = 0u;

	if (!Stm.Load(magic)
		|| !Stm.Load(version)
		|| !Stm.Load(cellCount)
		|| !Stm.Load(byteCount))
	{
		return false;
	}

	if (!Stm.HasRemainingBytes(byteCount))
		return false;

	const auto expectedByteCount = GetPhobosFogExploredPayloadByteCount(cellCount);
	const bool canRestore = version == PhobosFogPayloadVersion
		&& cellCount == static_cast<unsigned int>(MapClass::MaxCells)
		&& byteCount == expectedByteCount;

	if (!canRestore)
		return SkipPhobosFogPayloadBytes(Stm, byteCount);

	std::vector<unsigned char> bitset(byteCount);

	if (byteCount > 0u && !Stm.Read(bitset.data(), byteCount))
		return false;

	cellStates.assign(static_cast<size_t>(cellCount), PhobosFogCellState::Unknown);

	for (unsigned int cellIndex = 0; cellIndex < cellCount; ++cellIndex)
	{
		const auto byte = bitset[cellIndex / 8u];
		const auto mask = static_cast<unsigned char>(1u << (cellIndex % 8u));

		if ((byte & mask) != 0)
			cellStates[cellIndex] = PhobosFogCellState::Explored;
	}

	return true;
}

void HouseExt::ExtData::SavePhobosFogExploredPayload(PhobosStreamWriter& Stm) const
{
	const auto cellCount = static_cast<unsigned int>(MapClass::MaxCells);
	const auto byteCount = GetPhobosFogExploredPayloadByteCount(cellCount);
	std::vector<unsigned char> bitset(byteCount, 0u);
	const bool hasValidCellStates = this->PhobosFog_CellStates.size() == static_cast<size_t>(cellCount);

	if (hasValidCellStates)
	{
		for (unsigned int cellIndex = 0; cellIndex < cellCount; ++cellIndex)
		{
			const auto state = this->PhobosFog_CellStates[cellIndex];

			if (state == PhobosFogCellState::Explored || state == PhobosFogCellState::Visible)
				bitset[cellIndex / 8u] |= static_cast<unsigned char>(1u << (cellIndex % 8u));
		}
	}

	const auto magic = PhobosFogPayloadMagic;
	const auto version = PhobosFogPayloadVersion;

	Stm.Save(magic);
	Stm.Save(version);
	Stm.Save(cellCount);
	Stm.Save(byteCount);

	if (byteCount > 0u)
		Stm.Write(bitset.data(), byteCount);
}

void HouseExt::ExtData::TouchPhobosFogStateVersion(HouseExt::PhobosFogStateTouchReason reason)
{
	++this->PhobosFog_StateVersion;

	if (this->PhobosFog_StateVersion == 0)
		this->PhobosFog_StateVersion = 1;

	const auto pRulesExt = RulesExt::Global();

	if (!pRulesExt || !pRulesExt->PhobosFog_Perf_Enabled)
		return;

	++this->PhobosFog_StateVersionTouchCount;

	auto reasonIndex = static_cast<size_t>(reason);

	if (reasonIndex >= PhobosFogStateTouchReasonCount)
		reasonIndex = static_cast<size_t>(PhobosFogStateTouchReason::Other);

	++this->PhobosFog_StateVersionTouchReasons[reasonIndex];
}

void HouseExt::ExtData::TouchPhobosFogOverlayEffectiveVersion()
{
	++this->PhobosFog_OverlayEffectiveVersion;

	if (this->PhobosFog_OverlayEffectiveVersion == 0)
		this->PhobosFog_OverlayEffectiveVersion = 1;

	++this->PhobosFog_OverlayEffectiveTouchCount;
}

void HouseExt::ExtData::BeginPhobosFogOverlayEffectiveBatch()
{
	this->PhobosFog_OverlayEffectiveBatchActive = true;
	this->PhobosFog_OverlayEffectiveBatchTouchedCells.clear();
	this->PhobosFog_OverlayEffectiveBatchOriginalStates.clear();
	this->PhobosFog_LastOverlayEffectiveBatchTouchedCells = 0;
	this->PhobosFog_LastOverlayEffectiveBatchChangedCells = 0;

	++this->PhobosFog_OverlayEffectiveBatchStamp;

	if (this->PhobosFog_OverlayEffectiveBatchStamp == 0)
	{
		std::fill(
			this->PhobosFog_OverlayEffectiveBatchCellStamps.begin(),
			this->PhobosFog_OverlayEffectiveBatchCellStamps.end(),
			0U);
		this->PhobosFog_OverlayEffectiveBatchStamp = 1;
	}
}

void HouseExt::ExtData::TrackPhobosFogOverlayOriginalCell(int cellIndex)
{
	if (!this->PhobosFog_OverlayEffectiveBatchActive
		|| cellIndex < 0
		|| cellIndex >= MapClass::MaxCells)
	{
		return;
	}

	if (this->PhobosFog_OverlayEffectiveBatchCellStamps.size() != MapClass::MaxCells)
		this->PhobosFog_OverlayEffectiveBatchCellStamps.resize(MapClass::MaxCells, 0U);

	const auto index = static_cast<size_t>(cellIndex);

	if (this->PhobosFog_OverlayEffectiveBatchCellStamps[index] == this->PhobosFog_OverlayEffectiveBatchStamp)
		return;

	this->PhobosFog_OverlayEffectiveBatchCellStamps[index] = this->PhobosFog_OverlayEffectiveBatchStamp;
	this->PhobosFog_OverlayEffectiveBatchTouchedCells.push_back(cellIndex);
	this->PhobosFog_OverlayEffectiveBatchOriginalStates.push_back(this->GetEffectivePhobosFogCellState(cellIndex));
}

void HouseExt::ExtData::EndPhobosFogOverlayEffectiveBatch()
{
	if (!this->PhobosFog_OverlayEffectiveBatchActive)
		return;

	size_t changedCells = 0;
	const auto count = std::min(
		this->PhobosFog_OverlayEffectiveBatchTouchedCells.size(),
		this->PhobosFog_OverlayEffectiveBatchOriginalStates.size());

	for (size_t i = 0; i < count; ++i)
	{
		const int cellIndex = this->PhobosFog_OverlayEffectiveBatchTouchedCells[i];
		const auto originalState = this->PhobosFog_OverlayEffectiveBatchOriginalStates[i];

		if (cellIndex >= 0
			&& cellIndex < MapClass::MaxCells
			&& this->GetEffectivePhobosFogCellState(cellIndex) != originalState)
		{
			++changedCells;
		}
	}

	this->PhobosFog_LastOverlayEffectiveBatchTouchedCells = count;
	this->PhobosFog_LastOverlayEffectiveBatchChangedCells = changedCells;
	this->PhobosFog_OverlayEffectiveBatchActive = false;
	this->PhobosFog_OverlayEffectiveBatchTouchedCells.clear();
	this->PhobosFog_OverlayEffectiveBatchOriginalStates.clear();

	if (changedCells > 0)
		this->TouchPhobosFogOverlayEffectiveVersion();
}

void HouseExt::ExtData::AbortPhobosFogOverlayEffectiveBatch()
{
	this->PhobosFog_OverlayEffectiveBatchActive = false;
	this->PhobosFog_OverlayEffectiveBatchTouchedCells.clear();
	this->PhobosFog_OverlayEffectiveBatchOriginalStates.clear();
	this->PhobosFog_LastOverlayEffectiveBatchTouchedCells = 0;
	this->PhobosFog_LastOverlayEffectiveBatchChangedCells = 0;
}

bool HouseExt::ExtData::EnsurePhobosFogStateSize()
{
	bool resized = false;

	if (this->PhobosFog_CellStates.size() != MapClass::MaxCells)
	{
		this->PhobosFog_CellStates.resize(MapClass::MaxCells, PhobosFogCellState::Unknown);
		resized = true;
	}

	if (this->PhobosFog_LastVisibleFrames.size() != MapClass::MaxCells)
	{
		this->PhobosFog_LastVisibleFrames.resize(MapClass::MaxCells, -1);
		resized = true;
	}

	if (resized)
	{
		this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::EnsureResize);
		this->TouchPhobosFogOverlayEffectiveVersion();
	}

	return resized;
}

void HouseExt::ExtData::DegradePhobosFogVisibility(int currentFrame)
{
	bool changed = false;

	for (size_t i = 0; i < this->PhobosFog_CellStates.size(); ++i)
	{
		if (this->PhobosFog_CellStates[i] == PhobosFogCellState::Visible
			&& (i >= this->PhobosFog_LastVisibleFrames.size() || this->PhobosFog_LastVisibleFrames[i] < currentFrame))
		{
			this->TrackPhobosFogOverlayOriginalCell(static_cast<int>(i));
			this->PhobosFog_CellStates[i] = PhobosFogCellState::Explored;
			changed = true;
		}
	}

	if (changed)
	{
		this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::DegradeVisibleToExplored);

		if (!this->PhobosFog_OverlayEffectiveBatchActive)
			this->TouchPhobosFogOverlayEffectiveVersion();
	}
}

bool HouseExt::ExtData::MarkPhobosFogCellExplored(CellStruct cell)
{
	this->EnsurePhobosFogStateSize();

	if (this->PhobosFog_CellStates.size() != MapClass::MaxCells
		|| !MapClass::Instance.TryGetCellAt(cell))
	{
		return false;
	}

	const int cellIndex = MapClass::GetCellIndex(cell);

	if (cellIndex >= 0 && cellIndex < MapClass::MaxCells)
	{
		const auto index = static_cast<size_t>(cellIndex);

		if (this->PhobosFog_CellStates[index] == PhobosFogCellState::Unknown)
		{
			this->TrackPhobosFogOverlayOriginalCell(cellIndex);
			this->PhobosFog_CellStates[index] = PhobosFogCellState::Explored;
			this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::MarkExplored);

			if (!this->PhobosFog_OverlayEffectiveBatchActive)
				this->TouchPhobosFogOverlayEffectiveVersion();

			return true;
		}
	}

	return false;
}

size_t HouseExt::ExtData::MarkPhobosFogAreaExplored(CellStruct center, double radius)
{
	if (!MapClass::Instance.TryGetCellAt(center) || radius <= 0.0)
		return 0;

	size_t markedCells = 0;

	CellRangeIterator<CellClass>{}(center, radius, [this, &markedCells](CellClass* const pCell)
	{
		if (this->MarkPhobosFogCellExplored(pCell->MapCoords))
			++markedCells;

		return true;
	});

	return markedCells;
}

size_t HouseExt::ExtData::MarkPhobosFogCellSpreadExplored(CellStruct center, size_t spread)
{
	if (!MapClass::Instance.TryGetCellAt(center))
		return 0;

	size_t markedCells = 0;

	CellSpreadIterator<CellClass>{}(center, spread, [this, &markedCells](CellClass* const pCell)
	{
		if (pCell && this->MarkPhobosFogCellExplored(pCell->MapCoords))
			++markedCells;

		return true;
	});

	return markedCells;
}

size_t HouseExt::ExtData::MarkAllPhobosFogCellsExplored()
{
	this->EnsurePhobosFogStateSize();

	if (this->PhobosFog_CellStates.size() != MapClass::MaxCells)
		return 0;

	size_t markedCells = 0;
	auto& map = MapClass::Instance;
	map.CellIteratorReset();

	for (auto pCell = map.CellIteratorNext(); pCell; pCell = map.CellIteratorNext())
	{
		const int cellIndex = MapClass::GetCellIndex(pCell->MapCoords);

		if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
			continue;

		const auto index = static_cast<size_t>(cellIndex);

		if (this->PhobosFog_CellStates[index] == PhobosFogCellState::Unknown)
		{
			this->TrackPhobosFogOverlayOriginalCell(cellIndex);
			this->PhobosFog_CellStates[index] = PhobosFogCellState::Explored;
			++markedCells;
		}
	}

	if (markedCells > 0)
	{
		this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::MarkAllExplored);

		if (!this->PhobosFog_OverlayEffectiveBatchActive)
			this->TouchPhobosFogOverlayEffectiveVersion();
	}

	return markedCells;
}

bool HouseExt::ExtData::MarkPhobosFogCellVisible(CellStruct cell, int currentFrame)
{
	return this->MarkPhobosFogCellVisibleUntil(cell, currentFrame, PhobosFogStateTouchReason::MarkVisible);
}

bool HouseExt::ExtData::MarkPhobosFogCellVisibleUntil(CellStruct cell, int visibleUntilFrame)
{
	return this->MarkPhobosFogCellVisibleUntil(cell, visibleUntilFrame, PhobosFogStateTouchReason::MarkVisibleUntil);
}

bool HouseExt::ExtData::MarkPhobosFogCellVisibleUntil(CellStruct cell, int visibleUntilFrame, HouseExt::PhobosFogStateTouchReason reason)
{
	if (this->PhobosFog_CellStates.size() != MapClass::MaxCells
		|| this->PhobosFog_LastVisibleFrames.size() != MapClass::MaxCells
		|| !MapClass::Instance.TryGetCellAt(cell))
	{
		return false;
	}

	const int cellIndex = MapClass::GetCellIndex(cell);

	if (cellIndex >= 0 && cellIndex < MapClass::MaxCells)
	{
		const auto index = static_cast<size_t>(cellIndex);

		if (this->PhobosFog_CellStates[index] == PhobosFogCellState::Visible
			&& this->PhobosFog_LastVisibleFrames[index] >= visibleUntilFrame)
		{
			return true;
		}

		this->TrackPhobosFogOverlayOriginalCell(cellIndex);
		this->PhobosFog_CellStates[index] = PhobosFogCellState::Visible;
		if (this->PhobosFog_LastVisibleFrames[index] < visibleUntilFrame)
			this->PhobosFog_LastVisibleFrames[index] = visibleUntilFrame;

		this->TouchPhobosFogStateVersion(reason);

		if (!this->PhobosFog_OverlayEffectiveBatchActive)
			this->TouchPhobosFogOverlayEffectiveVersion();

		return true;
	}

	return false;
}

size_t HouseExt::ExtData::MarkPhobosFogAreaVisibleUntil(CellStruct center, double radius, int visibleUntilFrame, PhobosFogRevealAreaStats* pStats)
{
	if (!MapClass::Instance.TryGetCellAt(center) || radius <= 0.0)
		return 0;

	this->EnsurePhobosFogStateSize();

	size_t markedCells = 0;
	bool changed = false;

	CellRangeIterator<CellClass>{}(center, radius, [this, &markedCells, &changed, visibleUntilFrame, pStats](CellClass* const pCell)
	{
		if (!pCell)
		{
			if (pStats)
				++pStats->InvalidCells;

			return true;
		}

		const int cellIndex = MapClass::GetCellIndex(pCell->MapCoords);

		if (cellIndex >= 0 && cellIndex < MapClass::MaxCells)
		{
			const auto index = static_cast<size_t>(cellIndex);
			const auto previousState = this->PhobosFog_CellStates[index];
			const int previousVisibleUntil = this->PhobosFog_LastVisibleFrames[index];

			if (pStats)
			{
				++pStats->AffectedCells;
				pStats->MinCellX = std::min(pStats->MinCellX, pCell->MapCoords.X);
				pStats->MaxCellX = std::max(pStats->MaxCellX, pCell->MapCoords.X);
				pStats->MinCellY = std::min(pStats->MinCellY, pCell->MapCoords.Y);
				pStats->MaxCellY = std::max(pStats->MaxCellY, pCell->MapCoords.Y);
			}

			if (previousState != PhobosFogCellState::Visible
				|| previousVisibleUntil < visibleUntilFrame)
			{
				this->TrackPhobosFogOverlayOriginalCell(cellIndex);
				this->PhobosFog_CellStates[index] = PhobosFogCellState::Visible;
				this->PhobosFog_LastVisibleFrames[index] = visibleUntilFrame;
				changed = true;

				if (pStats)
				{
					switch (previousState)
					{
					case PhobosFogCellState::Unknown:
						++pStats->UnknownPromoted;
						break;
					case PhobosFogCellState::Explored:
						++pStats->ExploredPromoted;
						break;
					case PhobosFogCellState::Visible:
						++pStats->VisibleExtended;
						break;
					default:
						break;
					}
				}
			}
			else if (pStats)
			{
				++pStats->AlreadyVisibleEnoughSkipped;
			}

			++markedCells;
		}
		else if (pStats)
		{
			++pStats->OutOfBoundsSkipped;
		}

		return true;
	});

	if (changed)
	{
		this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::MarkAreaVisible);

		if (!this->PhobosFog_OverlayEffectiveBatchActive)
			this->TouchPhobosFogOverlayEffectiveVersion();
	}

	return markedCells;
}

size_t HouseExt::ExtData::MarkPhobosFogCellSpreadVisibleUntil(CellStruct center, size_t spread, int visibleUntilFrame, PhobosFogRevealAreaStats* pStats)
{
	if (!MapClass::Instance.TryGetCellAt(center))
		return 0;

	this->EnsurePhobosFogStateSize();

	size_t markedCells = 0;
	bool changed = false;

	CellSpreadIterator<CellClass>{}(center, spread, [this, &markedCells, &changed, visibleUntilFrame, pStats](CellClass* const pCell)
	{
		if (!pCell)
		{
			if (pStats)
				++pStats->InvalidCells;

			return true;
		}

		const int cellIndex = MapClass::GetCellIndex(pCell->MapCoords);

		if (cellIndex >= 0 && cellIndex < MapClass::MaxCells)
		{
			const auto index = static_cast<size_t>(cellIndex);
			const auto previousState = this->PhobosFog_CellStates[index];
			const int previousVisibleUntil = this->PhobosFog_LastVisibleFrames[index];

			if (pStats)
			{
				++pStats->AffectedCells;
				pStats->MinCellX = std::min(pStats->MinCellX, pCell->MapCoords.X);
				pStats->MaxCellX = std::max(pStats->MaxCellX, pCell->MapCoords.X);
				pStats->MinCellY = std::min(pStats->MinCellY, pCell->MapCoords.Y);
				pStats->MaxCellY = std::max(pStats->MaxCellY, pCell->MapCoords.Y);
			}

			if (previousState != PhobosFogCellState::Visible
				|| previousVisibleUntil < visibleUntilFrame)
			{
				this->TrackPhobosFogOverlayOriginalCell(cellIndex);
				this->PhobosFog_CellStates[index] = PhobosFogCellState::Visible;
				this->PhobosFog_LastVisibleFrames[index] = visibleUntilFrame;
				changed = true;

				if (pStats)
				{
					switch (previousState)
					{
					case PhobosFogCellState::Unknown:
						++pStats->UnknownPromoted;
						break;
					case PhobosFogCellState::Explored:
						++pStats->ExploredPromoted;
						break;
					case PhobosFogCellState::Visible:
						++pStats->VisibleExtended;
						break;
					default:
						break;
					}
				}
			}
			else if (pStats)
			{
				++pStats->AlreadyVisibleEnoughSkipped;
			}

			++markedCells;
		}
		else if (pStats)
		{
			++pStats->OutOfBoundsSkipped;
		}

		return true;
	});

	if (changed)
	{
		this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::MarkCellSpreadVisible);

		if (!this->PhobosFog_OverlayEffectiveBatchActive)
			this->TouchPhobosFogOverlayEffectiveVersion();
	}

	return markedCells;
}

size_t HouseExt::ExtData::MarkAllPhobosFogCellsVisibleUntil(int visibleUntilFrame)
{
	this->EnsurePhobosFogStateSize();

	if (this->PhobosFog_CellStates.size() != MapClass::MaxCells
		|| this->PhobosFog_LastVisibleFrames.size() != MapClass::MaxCells)
	{
		return 0;
	}

	size_t markedCells = 0;
	bool changed = false;
	auto& map = MapClass::Instance;
	map.CellIteratorReset();

	for (auto pCell = map.CellIteratorNext(); pCell; pCell = map.CellIteratorNext())
	{
		const int cellIndex = MapClass::GetCellIndex(pCell->MapCoords);

		if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
			continue;

		const auto index = static_cast<size_t>(cellIndex);
		const auto previousState = this->PhobosFog_CellStates[index];
		const int previousVisibleUntil = this->PhobosFog_LastVisibleFrames[index];

		if (previousState != PhobosFogCellState::Visible
			|| previousVisibleUntil < visibleUntilFrame)
		{
			this->TrackPhobosFogOverlayOriginalCell(cellIndex);
			changed = true;
		}

		this->PhobosFog_CellStates[index] = PhobosFogCellState::Visible;
		if (this->PhobosFog_LastVisibleFrames[index] < visibleUntilFrame)
			this->PhobosFog_LastVisibleFrames[index] = visibleUntilFrame;

		++markedCells;
	}

	if (changed)
	{
		this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::MarkAllVisible);

		if (!this->PhobosFog_OverlayEffectiveBatchActive)
			this->TouchPhobosFogOverlayEffectiveVersion();
	}

	return markedCells;
}

void HouseExt::ExtData::ExtendPhobosFogFullMapVisibleUntil(int visibleUntilFrame)
{
	if (this->PhobosFog_FullMapVisibleUntilFrame < visibleUntilFrame)
	{
		this->PhobosFog_FullMapVisibleUntilFrame = visibleUntilFrame;
		this->TouchPhobosFogStateVersion(PhobosFogStateTouchReason::FullMapVisibleUntil);
		this->TouchPhobosFogOverlayEffectiveVersion();
	}
}

bool HouseExt::ExtData::IsPhobosFogFullMapHardVisible() const
{
	const auto pRulesExt = RulesExt::Global();

	if (!pRulesExt || !pRulesExt->PhobosFog_Enabled)
		return false;

	auto const pHouse = this->OwnerObject();

	if (pRulesExt->PhobosFog_SyncSpySatellite
		&& pRulesExt->PhobosFog_SpySatellite_PersistentVisible
		&& pHouse && pHouse->SpySatActive)
	{
		return true;
	}

	return (pRulesExt->PhobosFog_SyncFullMapReveal || pRulesExt->PhobosFog_SyncSpySatellite)
		&& this->PhobosFog_FullMapVisibleUntilFrame > 0
		&& this->PhobosFog_FullMapVisibleUntilFrame >= Unsorted::CurrentFrame;
}

HouseExt::PhobosFogCellState HouseExt::ExtData::GetEffectivePhobosFogCellState(int cellIndex) const
{
	if (this->IsPhobosFogFullMapHardVisible())
		return PhobosFogCellState::Visible;

	if (cellIndex < 0 || cellIndex >= MapClass::MaxCells
		|| this->PhobosFog_CellStates.size() != MapClass::MaxCells)
	{
		return PhobosFogCellState::Unknown;
	}

	return this->PhobosFog_CellStates[static_cast<size_t>(cellIndex)];
}

bool HouseExt::ExtData::IsPhobosFogCellHardVisible(int cellIndex) const
{
	return this->GetEffectivePhobosFogCellState(cellIndex) == PhobosFogCellState::Visible;
}

static bool IsEligiblePhobosFogAlly(HouseClass* const pViewerHouse, HouseClass* const pHouse)
{
	return pViewerHouse && pHouse && pHouse != pViewerHouse && !pHouse->Defeated && !pHouse->IsObserver()
		&& pHouse->Type && !pHouse->Type->MultiplayPassive && pViewerHouse->IsAlliedWith(pHouse);
}

bool HouseExt::ExtData::TryGetEffectivePhobosFogCellStateForHouse(HouseClass* pHouse, int cellIndex, PhobosFogCellState& state)
{
	if (cellIndex < 0 || cellIndex >= MapClass::MaxCells)
		return false;

	const auto pHouseExt = HouseExt::ExtMap.TryFind(pHouse);

	if (!pHouseExt || pHouseExt->PhobosFog_CellStates.size() != MapClass::MaxCells)
		return false;

	state = pHouseExt->GetEffectivePhobosFogCellState(cellIndex);
	return true;
}

bool HouseExt::ExtData::TryGetEffectivePhobosFogCellStateForViewerOrAllies(HouseClass* pViewerHouse, int cellIndex, PhobosFogCellState& state)
{
	if (!pViewerHouse || pViewerHouse->IsObserver())
		return false;

	if (!TryGetEffectivePhobosFogCellStateForHouse(pViewerHouse, cellIndex, state))
		return false;

	if (state == PhobosFogCellState::Visible)
		return true;

	bool hasExplored = state == PhobosFogCellState::Explored;
	PhobosFogCellState allyState = PhobosFogCellState::Unknown;

	for (auto const pHouse : HouseClass::Array)
	{
		if (!IsEligiblePhobosFogAlly(pViewerHouse, pHouse)
			|| !TryGetEffectivePhobosFogCellStateForHouse(pHouse, cellIndex, allyState))
		{
			continue;
		}

		if (allyState == PhobosFogCellState::Visible)
		{
			state = PhobosFogCellState::Visible;
			return true;
		}

		hasExplored = hasExplored || allyState == PhobosFogCellState::Explored;
	}

	state = hasExplored ? PhobosFogCellState::Explored : PhobosFogCellState::Unknown;
	return true;
}

bool HouseExt::ExtData::IsPhobosFogCellHardVisibleToViewerOrAllies(HouseClass* pViewerHouse, int cellIndex, bool& querySucceeded)
{
	querySucceeded = false;

	PhobosFogCellState state = PhobosFogCellState::Unknown;

	if (!TryGetEffectivePhobosFogCellStateForViewerOrAllies(pViewerHouse, cellIndex, state))
		return false;

	querySucceeded = true;
	return state == PhobosFogCellState::Visible;
}

void HouseExt::ExtData::ResetPhobosFogDebugRefreshStats(bool resized)
{
	this->PhobosFog_LastRefreshProviders = 0;
	this->PhobosFog_LastRefreshVisibleCells = 0;
	this->PhobosFog_LastRefreshResized = resized;
}

void HouseExt::ExtData::AddPhobosFogDebugVisibilityProvider()
{
	++this->PhobosFog_LastRefreshProviders;
}

void HouseExt::ExtData::AddPhobosFogDebugVisibleCell()
{
	++this->PhobosFog_LastRefreshVisibleCells;
}

void HouseExt::ExtData::LogPhobosFogDebugSummary() const
{
	size_t unknownCount = 0;
	size_t exploredCount = 0;
	size_t visibleCount = 0;

	for (const auto cellState : this->PhobosFog_CellStates)
	{
		switch (cellState)
		{
		case PhobosFogCellState::Explored:
			++exploredCount;
			break;
		case PhobosFogCellState::Visible:
			++visibleCount;
			break;
		case PhobosFogCellState::Unknown:
		default:
			++unknownCount;
			break;
		}
	}

	auto const pHouse = this->OwnerObject();
	Debug::Log("[PhobosFog] House=%s VectorSize=%u Unknown=%u Explored=%u Visible=%u Providers=%u VisibleMarks=%u Resized=%s\n",
		pHouse ? pHouse->PlainName : "<null>",
		static_cast<unsigned int>(this->PhobosFog_CellStates.size()),
		static_cast<unsigned int>(unknownCount),
		static_cast<unsigned int>(exploredCount),
		static_cast<unsigned int>(visibleCount),
		static_cast<unsigned int>(this->PhobosFog_LastRefreshProviders),
		static_cast<unsigned int>(this->PhobosFog_LastRefreshVisibleCells),
		this->PhobosFog_LastRefreshResized ? "true" : "false");
}

void HouseExt::ExtData::InvalidatePointer(void* ptr, bool bRemoved)
{
	if (bRemoved)
	{
		AnnounceInvalidPointer(this->Factory_BuildingType, ptr);
		AnnounceInvalidPointer(this->Factory_InfantryType, ptr);
		AnnounceInvalidPointer(this->Factory_VehicleType, ptr);
		AnnounceInvalidPointer(this->Factory_NavyType, ptr);
		AnnounceInvalidPointer(this->Factory_AircraftType, ptr);

		if (ptr != nullptr)
		{
			if (!this->PowerPlantEnhancers.empty())
			{
				auto& vec = this->PowerPlantEnhancers;
				vec.erase(std::remove(vec.begin(), vec.end(), reinterpret_cast<BuildingClass*>(ptr)), vec.end());
			}

			if (!this->OwnedLimboDeliveredBuildings.empty())
			{
				auto& vec = this->OwnedLimboDeliveredBuildings;
				vec.erase(std::remove(vec.begin(), vec.end(), reinterpret_cast<BuildingClass*>(ptr)), vec.end());
			}

			if (!this->RestrictedFactoryPlants.empty())
			{
				auto& vec = this->RestrictedFactoryPlants;
				vec.erase(std::remove(vec.begin(), vec.end(), reinterpret_cast<BuildingClass*>(ptr)), vec.end());
			}
		}
	}
}

// =============================
// container

HouseExt::ExtContainer::ExtContainer() : Container("HouseClass")
{ }

HouseExt::ExtContainer::~ExtContainer() = default;

// =============================
// container hooks

DEFINE_HOOK(0x4F6532, HouseClass_CTOR, 0x5)
{
	GET(HouseClass*, pItem, EAX);

	HouseExt::ExtMap.TryAllocate(pItem);
	HouseExt::CalculatePowerSurplus(pItem);

	return 0;
}

DEFINE_HOOK(0x4F7371, HouseClass_DTOR, 0x6)
{
	GET(HouseClass*, pItem, ESI);

	HouseExt::ExtMap.Remove(pItem);

	return 0;
}

DEFINE_HOOK_AGAIN(0x504080, HouseClass_SaveLoad_Prefix, 0x5)
DEFINE_HOOK(0x503040, HouseClass_SaveLoad_Prefix, 0x5)
{
	GET_STACK(HouseClass*, pItem, 0x4);
	GET_STACK(IStream*, pStm, 0x8);

	HouseExt::ExtMap.PrepareStream(pItem, pStm);

	return 0;
}

DEFINE_HOOK(0x504069, HouseClass_Load_Suffix, 0x7)
{
	HouseExt::ExtMap.LoadStatic();

	return 0;
}

DEFINE_HOOK(0x5046DE, HouseClass_Save_Suffix, 0x7)
{
	HouseExt::ExtMap.SaveStatic();

	return 0;
}

DEFINE_HOOK(0x50114D, HouseClass_InitFromINI, 0x5)
{
	GET(HouseClass* const, pThis, EBX);
	GET(CCINIClass* const, pINI, ESI);

	HouseExt::ExtMap.LoadFromINI(pThis, pINI);

	return 0;
}
#pragma region BuildLimitGroup
static int CountOwnedIncludeDeploy(const HouseClass* pThis, const TechnoTypeClass* pItem)
{
	int count = pThis->CountOwnedNow(pItem);
	count += pItem->DeploysInto ? pThis->CountOwnedNow(pItem->DeploysInto) : 0;
	count += pItem->UndeploysInto ? pThis->CountOwnedNow(pItem->UndeploysInto) : 0;
	return count;
}

CanBuildResult HouseExt::BuildLimitGroupCheck(const HouseClass* pThis, const TechnoTypeClass* pItem, bool buildLimitOnly, bool includeQueued)
{
	const auto pItemExt = TechnoTypeExt::ExtMap.Find(pItem);

	if (pItemExt->BuildLimitGroup_Types.empty())
		return CanBuildResult::Buildable;

	std::vector<int> limits = pItemExt->BuildLimitGroup_Nums;
	auto const& extraLimitTypes = pItemExt->BuildLimitGroup_ExtraLimit_Types;
	auto const& extraLimitNums = pItemExt->BuildLimitGroup_ExtraLimit_Nums;
	auto const& extraLimitMaxCount = pItemExt->BuildLimitGroup_ExtraLimit_MaxCount;
	const int maxNum = pItemExt->BuildLimitGroup_ExtraLimit_MaxNum;

	if (!extraLimitTypes.empty() && !extraLimitNums.empty())
	{
		for (size_t i = 0; i < extraLimitTypes.size(); i++)
		{
			int count = 0;
			auto const pTmpType = extraLimitTypes[i];
			auto const pBuildingType = abstract_cast<BuildingTypeClass*, true>(pTmpType);

			if (pBuildingType && (BuildingTypeExt::ExtMap.Find(pBuildingType)->PowersUp_Buildings.size() > 0 || BuildingTypeClass::Find(pBuildingType->PowersUpBuilding)))
				count = BuildingTypeExt::GetUpgradesAmount(pBuildingType, const_cast<HouseClass*>(pThis));
			else
				count = pThis->CountOwnedNow(pTmpType);

			if (i < extraLimitMaxCount.size() && extraLimitMaxCount[i] > 0)
				count = Math::min(count, extraLimitMaxCount[i]);

			for (auto& limit : limits)
			{
				if (i < extraLimitNums.size() && extraLimitNums[i] > 0)
				{
					limit += count * extraLimitNums[i];

					if (maxNum > 0)
						limit = Math::min(limit, maxNum);
				}
			}
		}
	}

	auto const& buildLimit = pItemExt->BuildLimitGroup_Types;
	const int factor = pItemExt->BuildLimitGroup_Factor;

	if (pItemExt->BuildLimitGroup_ContentIfAnyMatch.Get())
	{
		bool reachedLimit = false;

		for (size_t i = 0; i < std::min(buildLimit.size(), pItemExt->BuildLimitGroup_Nums.size()); i++)
		{
			const auto pType = buildLimit[i];
			const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pType);
			const auto pBuildingType = abstract_cast<BuildingTypeClass*, true>(pType);
			int ownedNow = 0;

			if (pBuildingType && (BuildingTypeExt::ExtMap.Find(pBuildingType)->PowersUp_Buildings.size() > 0 || BuildingTypeClass::Find(pBuildingType->PowersUpBuilding)))
				ownedNow = BuildingTypeExt::GetUpgradesAmount(pBuildingType, const_cast<HouseClass*>(pThis));
			else
				ownedNow = CountOwnedIncludeDeploy(pThis, pType);

			ownedNow *= pTypeExt->BuildLimitGroup_Factor;

			if (ownedNow >= limits[i] + 1 - factor)
				reachedLimit |= (includeQueued && FactoryClass::FindByOwnerAndProduct(pThis, pType)) ? false : true;
		}

		return reachedLimit ? CanBuildResult::TemporarilyUnbuildable : CanBuildResult::Buildable;
	}
	else
	{
		if (limits.size() == 1U)
		{
			int sum = 0;
			bool reachedLimit = false;

			for (const auto pType : buildLimit)
			{
				const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pType);
				const auto pBuildingType = abstract_cast<BuildingTypeClass*, true>(pType);
				int owned = 0;

				if (pBuildingType && (BuildingTypeExt::ExtMap.Find(pBuildingType)->PowersUp_Buildings.size() > 0 || BuildingTypeClass::Find(pBuildingType->PowersUpBuilding)))
					owned = BuildingTypeExt::GetUpgradesAmount(pBuildingType, const_cast<HouseClass*>(pThis));
				else
					owned = CountOwnedIncludeDeploy(pThis, pType);

				sum += owned * pTypeExt->BuildLimitGroup_Factor;
			}

			if (sum >= limits[0] + 1 - factor)
			{
				for (const auto pType : buildLimit)
				{
					reachedLimit |= (includeQueued && FactoryClass::FindByOwnerAndProduct(pThis, pType)) ? false : true;
				}
			}

			return reachedLimit ? CanBuildResult::TemporarilyUnbuildable : CanBuildResult::Buildable;
		}
		else
		{
			for (size_t i = 0; i < std::min(buildLimit.size(), limits.size()); i++)
			{
				const auto pType = buildLimit[i];
				const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pType);
				const auto pBuildingType = abstract_cast<BuildingTypeClass*, true>(pType);
				int ownedNow = 0;

				if (pBuildingType && (BuildingTypeExt::ExtMap.Find(pBuildingType)->PowersUp_Buildings.size() > 0 || BuildingTypeClass::Find(pBuildingType->PowersUpBuilding)))
					ownedNow = BuildingTypeExt::GetUpgradesAmount(pBuildingType, const_cast<HouseClass*>(pThis));
				else
					ownedNow = CountOwnedIncludeDeploy(pThis, pType);

				ownedNow *= pTypeExt->BuildLimitGroup_Factor;

				if ((pItem == pType && ownedNow < limits[i] + 1 - factor) || includeQueued && FactoryClass::FindByOwnerAndProduct(pThis, pType))
					return CanBuildResult::Buildable;

				if ((pItem != pType && ownedNow < limits[i]) || includeQueued && FactoryClass::FindByOwnerAndProduct(pThis, pType))
					return CanBuildResult::Buildable;
			}

			return CanBuildResult::TemporarilyUnbuildable;
		}
	}
}

static int QueuedNum(const HouseClass* pHouse, const TechnoTypeClass* pType)
{
	const AbstractType absType = pType->WhatAmI();
	const BuildCat buildCat = (absType == AbstractType::BuildingType ? static_cast<const BuildingTypeClass*>(pType)->BuildCat : BuildCat::DontCare);
	const FactoryClass* pFactory = pHouse->GetPrimaryFactory(absType, pType->Naval, buildCat);
	int queued = 0;

	if (pFactory)
	{
		queued = pFactory->CountTotal(pType);

		if (const auto pObject = pFactory->Object)
		{
			if (pObject->GetType() == pType)
				--queued;
		}
	}

	return queued;
}

static void RemoveProduction(const HouseClass* pHouse, const TechnoTypeClass* pType, int num)
{
	const auto absType = pType->WhatAmI();
	const auto buildCat = (absType == AbstractType::BuildingType ? static_cast<const BuildingTypeClass*>(pType)->BuildCat : BuildCat::DontCare);
	const auto pFactory = pHouse->GetPrimaryFactory(absType, pType->Naval, buildCat);

	if (pFactory)
	{
		int queued = pFactory->CountTotal(pType);

		if (num >= 0)
			queued = Math::min(num, queued);

		for (int i = 0; i < queued; i++)
		{
			pFactory->RemoveOneFromQueue(pType);
		}
	}
}

bool HouseExt::ReachedBuildLimit(const HouseClass* pHouse, const TechnoTypeClass* pType, bool ignoreQueued)
{
	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pType);

	if (pTypeExt->BuildLimitGroup_Types.empty() || pTypeExt->BuildLimitGroup_Nums.empty())
		return false;

	std::vector<int> limits = pTypeExt->BuildLimitGroup_Nums;
	auto const& extraLimitTypes = pTypeExt->BuildLimitGroup_ExtraLimit_Types;
	auto const& extraLimitNums = pTypeExt->BuildLimitGroup_ExtraLimit_Nums;
	auto const& extraLimitMaxCount = pTypeExt->BuildLimitGroup_ExtraLimit_MaxCount;
	const int maxNum = pTypeExt->BuildLimitGroup_ExtraLimit_MaxNum;

	if (!extraLimitTypes.empty() && !extraLimitNums.empty())
	{
		for (size_t i = 0; i < extraLimitTypes.size(); i++)
		{
			const auto pTmpType = extraLimitTypes[i];
			const auto pBuildingType = abstract_cast<BuildingTypeClass*, true>(pTmpType);
			int count = 0;

			if (pBuildingType && (BuildingTypeExt::ExtMap.Find(pBuildingType)->PowersUp_Buildings.size() > 0 || BuildingTypeClass::Find(pBuildingType->PowersUpBuilding)))
				count = BuildingTypeExt::GetUpgradesAmount(pBuildingType, const_cast<HouseClass*>(pHouse));
			else
				count = pHouse->CountOwnedNow(pTmpType);

			if (i < extraLimitMaxCount.size() && extraLimitMaxCount[i] > 0)
				count = Math::min(count, extraLimitMaxCount[i]);

			for (auto& limit : limits)
			{
				if (i < extraLimitNums.size() && extraLimitNums[i] > 0)
				{
					limit += count * extraLimitNums[i];

					if (maxNum > 0)
						limit = Math::min(limit, maxNum);
				}
			}
		}
	}

	const int factor = pTypeExt->BuildLimitGroup_Factor;
	const bool notBuildable = pTypeExt->BuildLimitGroup_NotBuildableIfQueueMatch;

	if (limits.size() == 1)
	{
		int count = 0;
		int queued = 0;
		bool inside = false;

		for (const auto pTmpType : pTypeExt->BuildLimitGroup_Types)
		{
			const auto pTmpTypeExt = TechnoTypeExt::ExtMap.Find(pTmpType);

			if (!ignoreQueued)
				queued += QueuedNum(pHouse, pTmpType) * pTmpTypeExt->BuildLimitGroup_Factor;

			int owned = 0;
			const auto pBuildingType = abstract_cast<BuildingTypeClass*>(pTmpType);

			if (pBuildingType && (BuildingTypeExt::ExtMap.Find(pBuildingType)->PowersUp_Buildings.size() > 0 || BuildingTypeClass::Find(pBuildingType->PowersUpBuilding)))
				owned = BuildingTypeExt::GetUpgradesAmount(pBuildingType, const_cast<HouseClass*>(pHouse));
			else
				owned = pHouse->CountOwnedNow(pTmpType);

			count += owned * pTmpTypeExt->BuildLimitGroup_Factor;

			if (pTmpType == pType)
				inside = true;
		}

		const int num = count - limits.back();

		if (num + queued >= 1 - factor)
		{
			if (inside)
				RemoveProduction(pHouse, pType, (num + queued + factor - 1) / factor);
			else if (num >= 1 - factor || notBuildable)
				RemoveProduction(pHouse, pType, -1);

			return true;
		}
	}
	else
	{
		const size_t size = Math::min(limits.size(), pTypeExt->BuildLimitGroup_Types.size());
		const bool contentIfAny = pTypeExt->BuildLimitGroup_ContentIfAnyMatch;
		bool reached = true;
		bool realReached = true;

		for (size_t i = 0; i < size; i++)
		{
			const auto pTmpType = pTypeExt->BuildLimitGroup_Types[i];
			const auto pTmpTypeExt = TechnoTypeExt::ExtMap.Find(pTmpType);
			const int queued = ignoreQueued ? 0 : QueuedNum(pHouse, pTmpType) * pTmpTypeExt->BuildLimitGroup_Factor;
			int num = 0;
			const auto pBuildingType = abstract_cast<BuildingTypeClass*>(pTmpType);

			if (pBuildingType && (BuildingTypeExt::ExtMap.Find(pBuildingType)->PowersUp_Buildings.size() > 0 || BuildingTypeClass::Find(pBuildingType->PowersUpBuilding)))
				num = BuildingTypeExt::GetUpgradesAmount(pBuildingType, const_cast<HouseClass*>(pHouse));
			else
				num = pHouse->CountOwnedNow(pTmpType);

			num *= pTmpTypeExt->BuildLimitGroup_Factor - limits[i];

			if (pType == pTmpType && num + queued >= 1 - factor)
			{
				if (contentIfAny)
				{
					if (num >= 1 - factor || notBuildable)
						RemoveProduction(pHouse, pType, (num + queued + factor - 1) / factor);

					return true;
				}
				else if (num < 1 - factor)
				{
					realReached = false;
				}
			}
			else if (pType != pTmpType && num + queued >= 0)
			{
				if (contentIfAny)
				{
					if (num >= 0 || notBuildable)
						RemoveProduction(pHouse, pType, -1);

					return true;
				}
				else if (num < 0)
				{
					realReached = false;
				}
			}
			else
			{
				reached = false;
			}
		}

		if (reached)
		{
			if (realReached || notBuildable)
				RemoveProduction(pHouse, pType, -1);

			return true;
		}
	}

	return false;
}
#pragma endregion
