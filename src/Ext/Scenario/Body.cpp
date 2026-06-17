#include "Body.h"

#include <RadarClass.h>
#include <TechnoClass.h>
#include <Unsorted.h>
#include <VeinholeMonsterClass.h>

#include <Ext/House/Body.h>
#include <Ext/Rules/Body.h>
#include <Ext/Techno/Body.h>

#include <Helpers/Iterators.h>
#include <Utilities/Debug.h>

#include <algorithm>
#include <vector>

std::unique_ptr<ScenarioExt::ExtData> ScenarioExt::Data = nullptr;

bool ScenarioExt::CellParsed = false;

namespace
{
	constexpr int PhobosFogDebugReportInterval = 900;
	constexpr size_t PhobosFogRadarRefreshBudgetPerFrame = 512;
	int PhobosFogLastDebugReportFrame = -1;
	std::vector<CellStruct> PhobosFogPendingRadarRefreshCells;
	size_t PhobosFogPendingRadarRefreshIndex = 0;

	using RadarQueueCellRefreshFunction = void(__thiscall*)(RadarClass*, CellStruct*);

	void QueuePhobosFogRadarCellRefresh(const CellStruct& cell)
	{
		if (!MapClass::Instance.TryGetCellAt(cell))
			return;

		CellStruct queuedCell = cell;
		const auto queueCellRefresh = reinterpret_cast<RadarQueueCellRefreshFunction>(0x6551C0);
		queueCellRefresh(&RadarClass::Instance, &queuedCell);
	}

	bool IsEligiblePhobosFogHouse(HouseClass* const pHouse)
	{
		return pHouse && !pHouse->Defeated && !pHouse->IsObserver() && pHouse->Type && !pHouse->Type->MultiplayPassive;
	}

	bool IsRelevantPhobosFogRadarHouse(HouseClass* const pHouse)
	{
		const auto pCurrentPlayer = HouseClass::CurrentPlayer;

		return IsEligiblePhobosFogHouse(pHouse) && pCurrentPlayer && !pCurrentPlayer->IsObserver()
			&& (pHouse == pCurrentPlayer || pCurrentPlayer->IsAlliedWith(pHouse));
	}

	CellStruct GetPhobosFogCellFromIndex(const size_t cellIndex)
	{
		return { static_cast<short>(cellIndex & 0x1FF), static_cast<short>(cellIndex >> 9) };
	}

	void QueueVisiblePhobosFogRadarCells(HouseExt::ExtData* const pHouseExt)
	{
		if (!pHouseExt || pHouseExt->PhobosFog_CellStates.size() != MapClass::MaxCells)
			return;

		for (size_t i = 0; i < pHouseExt->PhobosFog_CellStates.size(); ++i)
		{
			if (pHouseExt->PhobosFog_CellStates[i] == HouseExt::PhobosFogCellState::Visible)
				QueuePhobosFogRadarCellRefresh(GetPhobosFogCellFromIndex(i));
		}
	}

	void ScheduleAllPhobosFogRadarCells()
	{
		PhobosFogPendingRadarRefreshCells.clear();
		PhobosFogPendingRadarRefreshIndex = 0;

		auto& map = MapClass::Instance;
		map.CellIteratorReset();

		for (auto pCell = map.CellIteratorNext(); pCell; pCell = map.CellIteratorNext())
			PhobosFogPendingRadarRefreshCells.push_back(pCell->MapCoords);
	}

	void ProcessScheduledPhobosFogRadarCells()
	{
		if (PhobosFogPendingRadarRefreshIndex >= PhobosFogPendingRadarRefreshCells.size())
			return;

		const size_t endIndex = std::min(
			PhobosFogPendingRadarRefreshIndex + PhobosFogRadarRefreshBudgetPerFrame,
			PhobosFogPendingRadarRefreshCells.size());

		for (; PhobosFogPendingRadarRefreshIndex < endIndex; ++PhobosFogPendingRadarRefreshIndex)
			QueuePhobosFogRadarCellRefresh(PhobosFogPendingRadarRefreshCells[PhobosFogPendingRadarRefreshIndex]);

		if (PhobosFogPendingRadarRefreshIndex >= PhobosFogPendingRadarRefreshCells.size())
		{
			PhobosFogPendingRadarRefreshCells.clear();
			PhobosFogPendingRadarRefreshIndex = 0;
		}
	}

	void UpdatePhobosFogSpySatelliteState(HouseClass* const pHouse, HouseExt::ExtData* const pHouseExt, RulesExt::ExtData* const pRulesExt)
	{
		if (!pHouse || !pHouseExt || !pRulesExt || !pRulesExt->PhobosFog_SyncSpySatellite)
			return;

		const bool spySatActive = pHouse->SpySatActive;
		const bool spySatEffectiveVisible = pRulesExt->PhobosFog_SpySatellite_PersistentVisible;

		if (spySatActive && !pHouseExt->PhobosFog_LastSpySatActive)
		{
			if (pRulesExt->PhobosFog_SpySatellite_MarkExplored)
				pHouseExt->MarkAllPhobosFogCellsExplored();

			if (spySatEffectiveVisible)
			{
				pHouseExt->TouchPhobosFogStateVersion(HouseExt::PhobosFogStateTouchReason::SpySatPersistentVisibleEdge);
				pHouseExt->TouchPhobosFogOverlayEffectiveVersion();
			}

			if (pRulesExt->PhobosFog_Debug)
				Debug::Log("[PhobosFog] House=%s SpySatActive=true MarkExplored=%s PersistentVisible=%s\n",
					pHouse->PlainName,
					pRulesExt->PhobosFog_SpySatellite_MarkExplored.Get() ? "true" : "false",
					pRulesExt->PhobosFog_SpySatellite_PersistentVisible.Get() ? "true" : "false");
		}
		else if (!spySatActive && pHouseExt->PhobosFog_LastSpySatActive)
		{
			const int holdFrames = pRulesExt->ResolvePhobosFogRevealVisibleHoldFrames(RulesExt::ExtData::PhobosFogRevealHoldSource::SpySatelliteDeactivate);

			if (holdFrames > 0)
				pHouseExt->ExtendPhobosFogFullMapVisibleUntil(Unsorted::CurrentFrame + holdFrames);
			else if (spySatEffectiveVisible)
			{
				pHouseExt->TouchPhobosFogStateVersion(HouseExt::PhobosFogStateTouchReason::SpySatPersistentVisibleEdge);
				pHouseExt->TouchPhobosFogOverlayEffectiveVersion();
			}

			if (pRulesExt->PhobosFog_Debug)
				Debug::Log("[PhobosFog] House=%s SpySatActive=false DeactivateHold=%d\n", pHouse->PlainName, holdFrames);
		}

		pHouseExt->PhobosFog_LastSpySatActive = spySatActive;
	}

	void QueuePhobosFogFullMapRadarRefreshOnEdge(HouseClass* const pHouse, HouseExt::ExtData* const pHouseExt, RulesExt::ExtData* const pRulesExt)
	{
		if (!pHouse || !pHouseExt || !pRulesExt || !pRulesExt->PhobosFog_OverrideRadarFog || !IsRelevantPhobosFogRadarHouse(pHouse))
			return;

		const bool fullMapHardVisible = pHouseExt->IsPhobosFogFullMapHardVisible();

		if (fullMapHardVisible == pHouseExt->PhobosFog_LastFullMapHardVisible)
			return;

		ScheduleAllPhobosFogRadarCells();
		pHouseExt->PhobosFog_LastFullMapHardVisible = fullMapHardVisible;

		if (pRulesExt->PhobosFog_Debug)
			Debug::Log("[PhobosFog] House=%s FullMapRadarRefresh FullMapHardVisible=%s Scheduled=%u BudgetPerFrame=%u\n",
				pHouse->PlainName,
				fullMapHardVisible ? "true" : "false",
				static_cast<unsigned int>(PhobosFogPendingRadarRefreshCells.size()),
				static_cast<unsigned int>(PhobosFogRadarRefreshBudgetPerFrame));
	}

	bool IsEligiblePhobosFogProvider(TechnoClass* const pTechno)
	{
		return pTechno && pTechno->Owner && pTechno->IsAlive && pTechno->Health > 0
			&& pTechno->IsOnMap && !pTechno->InLimbo && !pTechno->Deactivated;
	}

	void RefreshPhobosFogState()
	{
		auto const pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled)
			return;

		const int currentFrame = Unsorted::CurrentFrame;
		const int updateInterval = pRulesExt->PhobosFog_UpdateInterval.Get() > 0 ? pRulesExt->PhobosFog_UpdateInterval.Get() : 1;
		const bool forceRefresh = HouseExt::ConsumePhobosFogForceRefresh();
		const bool debugEnabled = pRulesExt->PhobosFog_Debug;
		const bool radarOverrideEnabled = pRulesExt->PhobosFog_OverrideRadarFog;
		const bool reportDebug = debugEnabled
			&& (PhobosFogLastDebugReportFrame < 0
				|| currentFrame < PhobosFogLastDebugReportFrame
				|| currentFrame - PhobosFogLastDebugReportFrame >= PhobosFogDebugReportInterval);

		if (!forceRefresh && updateInterval > 1 && currentFrame % updateInterval != 0)
			return;

		if (radarOverrideEnabled)
			ProcessScheduledPhobosFogRadarCells();

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligiblePhobosFogHouse(pHouse))
				continue;

			if (auto const pHouseExt = HouseExt::ExtMap.TryFind(pHouse))
			{
				const bool resized = pHouseExt->EnsurePhobosFogStateSize();

				if (debugEnabled)
					pHouseExt->ResetPhobosFogDebugRefreshStats(resized);

				UpdatePhobosFogSpySatelliteState(pHouse, pHouseExt, pRulesExt);
				QueuePhobosFogFullMapRadarRefreshOnEdge(pHouse, pHouseExt, pRulesExt);

				if (radarOverrideEnabled && IsRelevantPhobosFogRadarHouse(pHouse))
					QueueVisiblePhobosFogRadarCells(pHouseExt);

				pHouseExt->BeginPhobosFogOverlayEffectiveBatch();
				pHouseExt->DegradePhobosFogVisibility(currentFrame);
			}
		}

		for (auto const pTechno : TechnoClass::Array)
		{
			if (!IsEligiblePhobosFogProvider(pTechno) || !IsEligiblePhobosFogHouse(pTechno->Owner))
				continue;

			auto const pHouseExt = HouseExt::ExtMap.TryFind(pTechno->Owner);
			auto const pTechnoExt = TechnoExt::ExtMap.TryFind(pTechno);

			if (!pHouseExt || !pTechnoExt)
				continue;

			const int sight = pTechnoExt->GetSight();

			if (sight <= 0)
				continue;

			const auto technoCell = pTechno->GetMapCoords();

			if (!MapClass::Instance.TryGetCellAt(technoCell))
				continue;

			if (debugEnabled)
				pHouseExt->AddPhobosFogDebugVisibilityProvider();

			const bool queueRadarRefresh = radarOverrideEnabled && IsRelevantPhobosFogRadarHouse(pTechno->Owner);

			CellRangeIterator<CellClass>{}(technoCell, sight, [pHouseExt, debugEnabled, queueRadarRefresh, currentFrame](CellClass* const pCell)
			{
				if (queueRadarRefresh)
					QueuePhobosFogRadarCellRefresh(pCell->MapCoords);

				if (pHouseExt->MarkPhobosFogCellVisible(pCell->MapCoords, currentFrame) && debugEnabled)
					pHouseExt->AddPhobosFogDebugVisibleCell();

				return true;
			});
		}

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligiblePhobosFogHouse(pHouse))
				continue;

			if (auto const pHouseExt = HouseExt::ExtMap.TryFind(pHouse))
				pHouseExt->EndPhobosFogOverlayEffectiveBatch();
		}

		if (reportDebug)
		{
			PhobosFogLastDebugReportFrame = currentFrame;
			Debug::Log("[PhobosFog] Frame=%d DebugSummary\n", currentFrame);

			for (auto const pHouse : HouseClass::Array)
			{
				if (!IsEligiblePhobosFogHouse(pHouse))
					continue;

				if (auto const pHouseExt = HouseExt::ExtMap.TryFind(pHouse))
					pHouseExt->LogPhobosFogDebugSummary();
			}
		}
	}
}

void ScenarioExt::ExtData::SetVariableToByID(bool bIsGlobal, int nIndex, char bState)
{
	auto& dict = Global()->Variables[bIsGlobal];

	auto itr = dict.find(nIndex);

	if (itr != dict.end() && itr->second.Value != bState)
	{
		itr->second.Value = bState;
		ScenarioClass::Instance->VariablesChanged = true;
		if (!bIsGlobal)
			TagClass::NotifyLocalChanged(nIndex);
		else
			TagClass::NotifyGlobalChanged(nIndex);
	}
}

void ScenarioExt::ExtData::GetVariableStateByID(bool bIsGlobal, int nIndex, char* pOut)
{
	auto& dict = Global()->Variables[bIsGlobal];

	auto itr = dict.find(nIndex);
	if (itr != dict.end())
		*pOut = static_cast<char>(itr->second.Value);
}

void ScenarioExt::ExtData::ReadVariables(bool bIsGlobal, CCINIClass* pINI)
{
	if (!bIsGlobal) // Local variables need to be read again
		Global()->Variables[false].clear();
	else if (Global()->Variables[true].size() != 0) // Global variables had been loaded, DO NOT CHANGE THEM
		return;

	const int nCount = pINI->GetKeyCount("VariableNames");
	for (int i = 0; i < nCount; ++i)
	{
		const auto pKey = pINI->GetKeyName("VariableNames", i);
		int nIndex;
		if (sscanf_s(pKey, "%d", &nIndex) == 1)
		{
			auto& var = Global()->Variables[bIsGlobal][nIndex];
			pINI->ReadString("VariableNames", pKey, pKey, Phobos::readBuffer);
			char* buffer;
			strcpy_s(var.Name, strtok_s(Phobos::readBuffer, ",", &buffer));
			if (auto pState = strtok_s(nullptr, ",", &buffer))
				var.Value = atoi(pState);
			else
				var.Value = 0;
		}
	}
}

// you've inspired something controversial
void ScenarioExt::ExtData::SaveVariablesToFile(bool isGlobal)
{
	CCINIClass fINI {};
	CCFileClass file { isGlobal ? "globals.ini" : "locals.ini" };

	if (file.Exists())
		fINI.ReadCCFile(&file);
	else
		file.CreateFileA();

	for (const auto& [_,varext] : Global()->Variables[isGlobal])
		fINI.WriteInteger(ScenarioClass::Instance->FileName, varext.Name, varext.Value, false);

	fINI.WriteCCFile(&file);
	file.Close();
}

void ScenarioExt::Allocate(ScenarioClass* pThis)
{
	Data = std::make_unique<ScenarioExt::ExtData>(pThis);
}

void ScenarioExt::Remove(ScenarioClass* pThis)
{
	Data = nullptr;
}

void ScenarioExt::LoadFromINIFile(ScenarioClass* pThis, CCINIClass* pINI)
{
	Data->LoadFromINI(pINI);

	for (auto const pHouse : HouseClass::Array)
	{
		HouseExt::ExtMap.Find(pHouse)->FreeRadar = ScenarioClass::Instance->FreeRadar;
	}
}

void ScenarioExt::ExtData::UpdateAutoDeathObjectsInLimbo()
{
	for (auto const pExt : this->AutoDeathObjects)
	{
		auto const pTechno = pExt->OwnerObject();

		if (!pTechno->IsInLogic && pTechno->IsAlive)
			pExt->CheckDeathConditions(true);
	}
}

void ScenarioExt::ExtData::UpdateTransportReloaders()
{
	for (auto const pExt : this->TransportReloaders)
	{
		auto const pTechno = pExt->OwnerObject();

		if (pTechno->IsAlive && pTechno->Transporter && pTechno->Transporter->IsInLogic)
			pTechno->Reload();
	}
}

// =============================
// load / save

void ScenarioExt::ExtData::LoadFromINIFile(CCINIClass* const pINI)
{
	auto pThis = this->OwnerObject();

	INI_EX maINI(pINI);
	INI_EX ruINI(CCINIClass::INI_Rules);

	if (SessionClass::IsCampaign())
	{
		Nullable<bool> SP_MCVRedeploy;
		SP_MCVRedeploy.Read(maINI, GameStrings::Basic, GameStrings::MCVRedeploys);
		if (!SP_MCVRedeploy.isset())
			SP_MCVRedeploy.Read(ruINI, GameStrings::Basic, GameStrings::MCVRedeploys);
		GameModeOptionsClass::Instance.MCVRedeploy = SP_MCVRedeploy.Get(false);

		CCINIClass ini_missionmd {};
		ini_missionmd.LoadFromFile(GameStrings::MISSIONMD_INI);
		auto const scenarioName = pThis->FileName;

		// Override rankings
		pThis->ParTimeEasy = ini_missionmd.ReadTime(scenarioName, "Ranking.ParTimeEasy", pThis->ParTimeEasy);
		pThis->ParTimeMedium = ini_missionmd.ReadTime(scenarioName, "Ranking.ParTimeMedium", pThis->ParTimeMedium);
		pThis->ParTimeDifficult = ini_missionmd.ReadTime(scenarioName, "Ranking.ParTimeHard", pThis->ParTimeDifficult);
		ini_missionmd.ReadString(scenarioName, "Ranking.UnderParTitle", pThis->UnderParTitle, pThis->UnderParTitle);
		ini_missionmd.ReadString(scenarioName, "Ranking.UnderParMessage", pThis->UnderParMessage, pThis->UnderParMessage);
		ini_missionmd.ReadString(scenarioName, "Ranking.OverParTitle", pThis->OverParTitle, pThis->OverParTitle);
		ini_missionmd.ReadString(scenarioName, "Ranking.OverParMessage", pThis->OverParMessage, pThis->OverParMessage);

		this->ShowBriefing = pINI->ReadBool(GameStrings::Basic, "ShowBriefing", this->ShowBriefing);
		this->BriefingTheme = pINI->ReadTheme(GameStrings::Basic, "BriefingTheme", this->BriefingTheme);
	}
}

template <typename T>
void ScenarioExt::ExtData::Serialize(T& Stm)
{
	Stm
		.Process(this->Waypoints)
		.Process(this->Variables[0])
		.Process(this->Variables[1])
		.Process(this->ShowBriefing)
		.Process(this->BriefingTheme)
		.Process(this->AutoDeathObjects)
		.Process(this->TransportReloaders)
		.Process(this->SWSidebar_Enable)
		.Process(this->SWSidebar_Indices)
		.Process(this->RecordMessages)
		.Process(this->DefaultLS640BkgdName)
		.Process(this->DefaultLS800BkgdName)
		.Process(this->DefaultLS800BkgdPal)
		.Process(this->LimboLaunchers)
		.Process(this->UndergroundTracker)
		.Process(this->SpecialTracker)
		.Process(this->FallingDownTracker)
		.Process(this->EVAIndex)
		;
}

void ScenarioExt::ExtData::LoadFromStream(PhobosStreamReader& Stm)
{
	Extension<ScenarioClass>::LoadFromStream(Stm);
	this->Serialize(Stm);
}

void ScenarioExt::ExtData::SaveToStream(PhobosStreamWriter& Stm)
{
	Global()->EVAIndex = VoxClass::EVAIndex;

	Extension<ScenarioClass>::SaveToStream(Stm);
	this->Serialize(Stm);
}

// =============================
// container hooks

DEFINE_HOOK(0x683549, ScenarioClass_CTOR, 0x9)
{
	GET(ScenarioClass*, pItem, EAX);

	ScenarioExt::Allocate(pItem);

	ScenarioExt::Global()->Waypoints.clear();
	ScenarioExt::Global()->Variables[0].clear();
	ScenarioExt::Global()->Variables[1].clear();

	return 0;
}

DEFINE_HOOK(0x6BEB7D, ScenarioClass_DTOR, 0x6)
{
	GET(ScenarioClass*, pItem, ESI);

	ScenarioExt::Remove(pItem);
	return 0;
}

IStream* ScenarioExt::g_pStm = nullptr;

DEFINE_HOOK_AGAIN(0x689470, ScenarioClass_SaveLoad_Prefix, 0x5)
DEFINE_HOOK(0x689310, ScenarioClass_SaveLoad_Prefix, 0x5)
{
	GET_STACK(IStream*, pStm, 0x4);

	ScenarioExt::g_pStm = pStm;

	return 0;
}

DEFINE_HOOK(0x689669, ScenarioClass_Load_Suffix, 0x6)
{
	auto buffer = ScenarioExt::Global();

	PhobosByteStream Stm(0);
	if (Stm.ReadBlockFromStream(ScenarioExt::g_pStm))
	{
		PhobosStreamReader Reader(Stm);

		if (Reader.Expect(ScenarioExt::Canary) && Reader.RegisterChange(buffer))
			buffer->LoadFromStream(Reader);
	}

	return 0;
}

DEFINE_HOOK(0x68945B, ScenarioClass_Save_Suffix, 0x8)
{
	auto buffer = ScenarioExt::Global();
	PhobosByteStream saver(sizeof(*buffer));
	PhobosStreamWriter writer(saver);

	writer.Expect(ScenarioExt::Canary);
	writer.RegisterChange(buffer);

	buffer->SaveToStream(writer);
	saver.WriteBlockToStream(ScenarioExt::g_pStm);

	return 0;
}

DEFINE_HOOK(0x68AD2F, ScenarioClass_LoadFromINI, 0x5)
{
	GET(ScenarioClass*, pItem, ESI);
	GET(CCINIClass*, pINI, EDI);

	ScenarioExt::LoadFromINIFile(pItem, pINI);
	return 0;
}

DEFINE_HOOK(0x55B4E1, LogicClass_Update_BeforeAll, 0x5)
{
	VeinholeMonsterClass::UpdateAllVeinholes();

	ScenarioExt::Global()->UpdateAutoDeathObjectsInLimbo();
	ScenarioExt::Global()->UpdateTransportReloaders();
	RefreshPhobosFogState();

	return 0;
}
