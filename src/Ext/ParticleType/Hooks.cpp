#include "Body.h"

#include <Ext/Anim/Body.h>
#include <Ext/House/Body.h>
#include <Ext/Rules/Body.h>
#include <BuildingClass.h>
#include <FootClass.h>
#include <Fundamentals.h>
#include <ParticleSystemClass.h>
#include <TechnoClass.h>
#include <Utilities/Debug.h>

#include <cstring>
#include <vector>

namespace PhobosFogParticle
{
	enum class ParticleSystemOwnerSource
	{
		None,
		SystemOwnerTechno,
		SystemOwnerAnim,
		DamageParticleSystem,
		TargetTechno,
		FireParticleSystem,
		SparkParticleSystem,
		NaturalParticleSystem,
		RailgunParticleSystem,
		Unknown1ParticleSystem,
		Unknown2ParticleSystem,
		FiringParticleSystem
	};

	struct ParticleProbeInfo
	{
		bool Hidden = false;
		const char* Reason = "NotEvaluated";
		TechnoClass* OwnerTechno = nullptr;
		ParticleSystemOwnerSource OwnerSource = ParticleSystemOwnerSource::None;
		TechnoClass* AnyFieldOwnerTechno = nullptr;
		ParticleSystemOwnerSource AnyFieldOwnerSource = ParticleSystemOwnerSource::None;
		TechnoClass* EnemyCandidateTechno = nullptr;
		ParticleSystemOwnerSource EnemyCandidateSource = ParticleSystemOwnerSource::None;
		bool EnemyCandidateFootQuerySucceeded = false;
		bool EnemyCandidateFootHidden = false;
		int OwnerCandidateCount = 0;
		bool OwnerFootQuerySucceeded = false;
		bool OwnerFootHidden = false;
		bool HasCell = false;
		int CellIndex = -1;
		bool CellQuerySucceeded = false;
		bool CellVisible = false;
		bool HiddenHoldActive = false;
		int HiddenHoldUntilFrame = -1;
		int VisibleStableFrames = 0;
		bool HasSourceCell = false;
		int SourceCellIndex = -1;
		bool SourceCellQuerySucceeded = false;
		bool SourceCellVisible = false;
	};

	struct ParticleSystemOwnerCandidates
	{
		static constexpr int Capacity = 16;

		TechnoClass* Owners[Capacity] {};
		ParticleSystemOwnerSource Sources[Capacity] {};
		int Count = 0;

		void Add(TechnoClass* const pOwner, const ParticleSystemOwnerSource source)
		{
			if (!pOwner || source == ParticleSystemOwnerSource::None)
				return;

			for (int i = 0; i < Count; ++i)
			{
				if (Owners[i] == pOwner && Sources[i] == source)
					return;
			}

			if (Count >= Capacity)
				return;

			Owners[Count] = pOwner;
			Sources[Count] = source;
			++Count;
		}
	};

	struct ParticleSystemVisibilityCacheEntry
	{
		ParticleSystemClass* System = nullptr;
		TechnoClass* Owner = nullptr;
		int LastSeenFrame = -1;
		int HiddenUntilFrame = -1;
		int VisibleSinceFrame = -1;
	};

	static constexpr int ParticleSystemHiddenHoldFrames = 90;
	static constexpr int ParticleSystemVisibleReleaseFrames = 45;
	static constexpr int ParticleSystemCachePruneFrames = 900;

	static const char* BoolText(const bool value)
	{
		return value ? "true" : "false";
	}

	static const char* SafeText(const char* const pText)
	{
		return pText ? pText : "<null>";
	}

	static const char* GetParticleSystemOwnerSourceName(const ParticleSystemOwnerSource source)
	{
		switch (source)
		{
		case ParticleSystemOwnerSource::SystemOwnerTechno:
			return "SystemOwnerTechno";
		case ParticleSystemOwnerSource::SystemOwnerAnim:
			return "SystemOwnerAnim";
		case ParticleSystemOwnerSource::DamageParticleSystem:
			return "DamageParticleSystem";
		case ParticleSystemOwnerSource::TargetTechno:
			return "TargetTechno";
		case ParticleSystemOwnerSource::FireParticleSystem:
			return "FireParticleSystem";
		case ParticleSystemOwnerSource::SparkParticleSystem:
			return "SparkParticleSystem";
		case ParticleSystemOwnerSource::NaturalParticleSystem:
			return "NaturalParticleSystem";
		case ParticleSystemOwnerSource::RailgunParticleSystem:
			return "RailgunParticleSystem";
		case ParticleSystemOwnerSource::Unknown1ParticleSystem:
			return "Unknown1ParticleSystem";
		case ParticleSystemOwnerSource::Unknown2ParticleSystem:
			return "Unknown2ParticleSystem";
		case ParticleSystemOwnerSource::FiringParticleSystem:
			return "FiringParticleSystem";
		default:
			return "None";
		}
	}

	static const char* GetParticleTypeID(ParticleClass* const pParticle)
	{
		return pParticle && pParticle->Type ? pParticle->Type->get_ID() : "<null>";
	}

	static const char* GetParticleSystemTypeID(ParticleSystemClass* const pParticleSystem)
	{
		return pParticleSystem && pParticleSystem->Type ? pParticleSystem->Type->get_ID() : "<null>";
	}

	static bool IsSmallGreySmokeParticleSystem(ParticleSystemClass* const pParticleSystem)
	{
		const auto pID = pParticleSystem && pParticleSystem->Type ? pParticleSystem->Type->get_ID() : nullptr;

		return pID && !_strcmpi(pID, "SmallGreySSys");
	}

	static const char* GetTechnoID(TechnoClass* const pTechno)
	{
		return SafeText(pTechno ? pTechno->get_ID() : nullptr);
	}

	static const char* GetHouseID(HouseClass* const pHouse)
	{
		return SafeText(pHouse ? pHouse->get_ID() : nullptr);
	}

	static bool ShouldLogProbe()
	{
		return false;
	}

	static bool ShouldEmitParticleLog(const bool hidden, const bool ownerFound)
	{
		static int logCount = 0;
		static int lastLogFrame = -900;

		if (logCount >= 480)
			return false;

		const int frameDelay = hidden ? 5 : (ownerFound ? 15 : 45);

		if (Unsorted::CurrentFrame < lastLogFrame + frameDelay)
			return false;

		++logCount;
		lastLogFrame = Unsorted::CurrentFrame;
		return true;
	}

	static bool ShouldEmitParticleSystemLog(const bool ownerFound)
	{
		static int logCount = 0;
		static int lastLogFrame = -900;

		if (logCount >= 480)
			return false;

		const int frameDelay = ownerFound ? 15 : 45;

		if (Unsorted::CurrentFrame < lastLogFrame + frameDelay)
			return false;

		++logCount;
		lastLogFrame = Unsorted::CurrentFrame;
		return true;
	}

	static bool ShouldEmitParticleDrawHitProbe()
	{
		static int logCount = 0;

		if (logCount >= 200)
			return false;

		++logCount;
		return true;
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

	static bool TryGetParticleCellIndex(ParticleClass* const pParticle, int& cellIndex)
	{
		return pParticle && TryGetCellIndex(pParticle->GetMapCoords(), cellIndex);
	}

	static bool TryGetParticleSystemCellIndex(ParticleSystemClass* const pParticleSystem, int& cellIndex)
	{
		return pParticleSystem && TryGetCellIndex(pParticleSystem->GetMapCoords(), cellIndex);
	}

	static bool TryGetParticleSystemSourceCellIndex(ParticleSystemClass* const pParticleSystem, int& cellIndex)
	{
		if (!pParticleSystem)
			return false;

		if (pParticleSystem->TargetCoords != CoordStruct::Empty)
		{
			if (const auto pTargetCell = MapClass::Instance.TryGetCellAt(pParticleSystem->TargetCoords))
			{
				cellIndex = MapClass::GetCellIndex(pTargetCell->MapCoords);
				return cellIndex >= 0 && cellIndex < MapClass::MaxCells;
			}
		}

		return TryGetParticleSystemCellIndex(pParticleSystem, cellIndex);
	}

	static bool TryGetTechnoCellIndex(TechnoClass* const pTechno, int& cellIndex)
	{
		return pTechno && TryGetCellIndex(pTechno->GetMapCoords(), cellIndex);
	}

	static bool TryResolveOwnerFootHidden(TechnoClass* const pTechno, HouseClass* const pViewerHouse, bool& hidden)
	{
		hidden = false;

		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_HideEnemyFoot || !pTechno || !generic_cast<FootClass*, true>(pTechno)
			|| !pTechno->Owner || !pViewerHouse)
		{
			return false;
		}

		if (pViewerHouse->IsAlliedWith(pTechno->Owner))
			return true;

		int cellIndex = -1;

		if (!TryGetTechnoCellIndex(pTechno, cellIndex))
			return false;

		bool querySucceeded = false;
		const bool visible = IsCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);

		if (!querySucceeded)
			return false;

		hidden = !visible;
		return true;
	}

	static bool IsEnemyFootOwner(TechnoClass* const pTechno, HouseClass* const pViewerHouse)
	{
		const auto pRulesExt = RulesExt::Global();

		return pRulesExt && pRulesExt->PhobosFog_HideEnemyFoot && pTechno && generic_cast<FootClass*, true>(pTechno)
			&& pTechno->Owner && pViewerHouse && !pViewerHouse->IsAlliedWith(pTechno->Owner);
	}

	static std::vector<ParticleSystemVisibilityCacheEntry>& GetParticleSystemVisibilityCache()
	{
		static std::vector<ParticleSystemVisibilityCacheEntry> cache;
		return cache;
	}

	static void PruneParticleSystemVisibilityCache()
	{
		auto& cache = GetParticleSystemVisibilityCache();
		const int currentFrame = Unsorted::CurrentFrame;

		for (auto it = cache.begin(); it != cache.end();)
		{
			if (!it->System || currentFrame > it->LastSeenFrame + ParticleSystemCachePruneFrames)
				it = cache.erase(it);
			else
				++it;
		}
	}

	static ParticleSystemVisibilityCacheEntry* FindParticleSystemVisibilityCacheEntry(ParticleSystemClass* const pParticleSystem)
	{
		auto& cache = GetParticleSystemVisibilityCache();

		for (auto& entry : cache)
		{
			if (entry.System == pParticleSystem)
				return &entry;
		}

		return nullptr;
	}

	static ParticleSystemVisibilityCacheEntry& GetOrCreateParticleSystemVisibilityCacheEntry(ParticleSystemClass* const pParticleSystem, TechnoClass* const pOwner)
	{
		auto* const pExistingEntry = FindParticleSystemVisibilityCacheEntry(pParticleSystem);

		if (pExistingEntry)
		{
			if (pExistingEntry->Owner != pOwner)
			{
				pExistingEntry->Owner = pOwner;
				pExistingEntry->HiddenUntilFrame = -1;
				pExistingEntry->VisibleSinceFrame = -1;
			}

			return *pExistingEntry;
		}

		auto& cache = GetParticleSystemVisibilityCache();

		cache.push_back({ pParticleSystem, pOwner, Unsorted::CurrentFrame, -1, -1 });

		return cache.back();
	}

	static void ClearParticleSystemVisibilityCacheEntry(ParticleSystemClass* const pParticleSystem)
	{
		auto& cache = GetParticleSystemVisibilityCache();

		for (auto it = cache.begin(); it != cache.end(); ++it)
		{
			if (it->System == pParticleSystem)
			{
				cache.erase(it);
				return;
			}
		}
	}

	static bool ApplyParticleSystemHiddenHold(
		ParticleSystemClass* const pParticleSystem,
		TechnoClass* const pOwner,
		HouseClass* const pViewerHouse,
		const bool ownerHidden,
		ParticleProbeInfo& info)
	{
		if (!pParticleSystem || !IsEnemyFootOwner(pOwner, pViewerHouse))
		{
			if (pParticleSystem)
				ClearParticleSystemVisibilityCacheEntry(pParticleSystem);

			return false;
		}

		PruneParticleSystemVisibilityCache();

		auto& entry = GetOrCreateParticleSystemVisibilityCacheEntry(pParticleSystem, pOwner);
		const int currentFrame = Unsorted::CurrentFrame;

		entry.LastSeenFrame = currentFrame;

		if (ownerHidden)
		{
			entry.HiddenUntilFrame = currentFrame + ParticleSystemHiddenHoldFrames;
			entry.VisibleSinceFrame = -1;
			info.HiddenHoldUntilFrame = entry.HiddenUntilFrame;
			info.VisibleStableFrames = 0;
			return false;
		}

		if (entry.HiddenUntilFrame < currentFrame)
		{
			entry.HiddenUntilFrame = -1;
			entry.VisibleSinceFrame = -1;
			return false;
		}

		if (entry.VisibleSinceFrame < 0)
			entry.VisibleSinceFrame = currentFrame;

		info.VisibleStableFrames = currentFrame - entry.VisibleSinceFrame;
		info.HiddenHoldUntilFrame = entry.HiddenUntilFrame;

		if (info.VisibleStableFrames >= ParticleSystemVisibleReleaseFrames)
		{
			entry.HiddenUntilFrame = -1;
			entry.VisibleSinceFrame = -1;
			return false;
		}

		info.HiddenHoldActive = true;
		info.Hidden = true;
		info.Reason = "ParticleSystemHiddenHold";
		return true;
	}

	static bool TryResolveParticleSystemSourceCellHidden(
		ParticleSystemClass* const pParticleSystem,
		TechnoClass* const pOwner,
		HouseClass* const pViewerHouse,
		ParticleProbeInfo& info)
	{
		if (!pParticleSystem || !IsEnemyFootOwner(pOwner, pViewerHouse))
			return false;

		int cellIndex = -1;

		if (!TryGetParticleSystemSourceCellIndex(pParticleSystem, cellIndex))
			return false;

		info.HasSourceCell = true;
		info.SourceCellIndex = cellIndex;

		bool querySucceeded = false;
		const bool visible = IsCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);

		info.SourceCellQuerySucceeded = querySucceeded;
		info.SourceCellVisible = visible;

		if (!querySucceeded || visible)
			return false;

		info.Hidden = true;
		info.Reason = "ParticleSystemSourceCellHidden";
		return true;
	}

	static bool TryResolveEnemyDamageParticleConservativeHidden(
		ParticleSystemClass* const pParticleSystem,
		TechnoClass* const pOwner,
		HouseClass* const pViewerHouse,
		ParticleProbeInfo& info)
	{
		if (!pParticleSystem || !IsEnemyFootOwner(pOwner, pViewerHouse))
			return false;

		if (info.OwnerSource == ParticleSystemOwnerSource::DamageParticleSystem
			|| info.AnyFieldOwnerSource == ParticleSystemOwnerSource::DamageParticleSystem)
		{
			info.Hidden = true;
			info.Reason = "EnemyDamageParticleConservativeHidden";
			return true;
		}

		if (info.OwnerSource == ParticleSystemOwnerSource::TargetTechno
			&& IsSmallGreySmokeParticleSystem(pParticleSystem))
		{
			info.Hidden = true;
			info.Reason = info.AnyFieldOwnerSource == ParticleSystemOwnerSource::None
				? "TargetCoordsNotTrusted"
				: "EnemyTargetPreferredOverFieldOwner";
			return true;
		}

		return false;
	}

	static TechnoClass* TryGetObjectTechno(ObjectClass* const pObject)
	{
		if (!pObject)
			return nullptr;

		for (auto const pTechno : TechnoClass::Array)
		{
			if (pTechno && static_cast<ObjectClass*>(pTechno) == pObject)
				return pTechno;
		}

		return nullptr;
	}

	static AnimClass* TryGetObjectAnim(ObjectClass* const pObject)
	{
		if (!pObject)
			return nullptr;

		for (auto const pAnim : AnimClass::Array)
		{
			if (pAnim && static_cast<ObjectClass*>(pAnim) == pObject)
				return pAnim;
		}

		return nullptr;
	}

	static TechnoClass* TryGetAnimOwnerTechno(AnimClass* const pAnim)
	{
		if (!pAnim)
			return nullptr;

		if (auto const pOwnerTechno = TryGetObjectTechno(pAnim->OwnerObject))
			return pOwnerTechno;

		const auto pAnimExt = AnimExt::ExtMap.TryFind(pAnim);

		if (!pAnimExt)
			return nullptr;

		if (pAnimExt->Invoker)
			return pAnimExt->Invoker;

		return pAnimExt->ParentBuilding;
	}

	static void CollectParticleSystemFieldOwnerCandidates(
		ParticleSystemClass* const pParticleSystem,
		ParticleSystemOwnerCandidates& candidates,
		const bool diagnosticsOnly)
	{
		if (!pParticleSystem)
			return;

		for (auto const pTechno : TechnoClass::Array)
		{
			if (!pTechno)
				continue;

			if (pTechno->DamageParticleSystem == pParticleSystem)
			{
				candidates.Add(pTechno, ParticleSystemOwnerSource::DamageParticleSystem);

				if (!diagnosticsOnly)
					continue;
			}

			if (!diagnosticsOnly)
				continue;

			if (pTechno->FireParticleSystem == pParticleSystem)
				candidates.Add(pTechno, ParticleSystemOwnerSource::FireParticleSystem);

			if (pTechno->SparkParticleSystem == pParticleSystem)
				candidates.Add(pTechno, ParticleSystemOwnerSource::SparkParticleSystem);

			if (pTechno->NaturalParticleSystem == pParticleSystem)
				candidates.Add(pTechno, ParticleSystemOwnerSource::NaturalParticleSystem);

			if (pTechno->RailgunParticleSystem == pParticleSystem)
				candidates.Add(pTechno, ParticleSystemOwnerSource::RailgunParticleSystem);

			if (pTechno->unk1ParticleSystem == pParticleSystem)
				candidates.Add(pTechno, ParticleSystemOwnerSource::Unknown1ParticleSystem);

			if (pTechno->unk2ParticleSystem == pParticleSystem)
				candidates.Add(pTechno, ParticleSystemOwnerSource::Unknown2ParticleSystem);

			if (pTechno->FiringParticleSystem == pParticleSystem)
				candidates.Add(pTechno, ParticleSystemOwnerSource::FiringParticleSystem);
		}
	}

	static TechnoClass* TryFindParticleSystemOwnerByField(ParticleSystemClass* const pParticleSystem, ParticleSystemOwnerSource& source, const bool diagnosticsOnly)
	{
		source = ParticleSystemOwnerSource::None;

		ParticleSystemOwnerCandidates candidates;
		CollectParticleSystemFieldOwnerCandidates(pParticleSystem, candidates, diagnosticsOnly);

		if (candidates.Count > 0)
		{
			source = candidates.Sources[0];
			return candidates.Owners[0];
		}

		return nullptr;
	}

	static void CollectParticleSystemOwnerCandidates(
		ParticleSystemClass* const pParticleSystem,
		ParticleSystemOwnerCandidates& candidates,
		const bool diagnosticsOnly)
	{
		if (!pParticleSystem)
			return;

		CollectParticleSystemFieldOwnerCandidates(pParticleSystem, candidates, diagnosticsOnly);

		if (auto const pOwnerTechno = TryGetObjectTechno(pParticleSystem->Owner))
			candidates.Add(pOwnerTechno, ParticleSystemOwnerSource::SystemOwnerTechno);

		if (auto const pOwnerAnim = TryGetObjectAnim(pParticleSystem->Owner))
		{
			if (auto const pAnimOwnerTechno = TryGetAnimOwnerTechno(pOwnerAnim))
				candidates.Add(pAnimOwnerTechno, ParticleSystemOwnerSource::SystemOwnerAnim);
		}

		if (auto const pTargetTechno = TryGetObjectTechno(static_cast<ObjectClass*>(pParticleSystem->Target)))
			candidates.Add(pTargetTechno, ParticleSystemOwnerSource::TargetTechno);
	}

	static bool TrySelectParticleSystemOwnerCandidate(
		ParticleSystemClass* const pParticleSystem,
		HouseClass* const pViewerHouse,
		ParticleProbeInfo& info)
	{
		ParticleSystemOwnerCandidates candidates;
		CollectParticleSystemOwnerCandidates(pParticleSystem, candidates, true);
		info.OwnerCandidateCount = candidates.Count;

		int firstQueryableCandidate = -1;
		int firstEnemyCandidate = -1;
		int firstHiddenEnemyCandidate = -1;
		bool firstQueryableHidden = false;
		bool firstEnemyHidden = false;
		bool firstHiddenEnemyHidden = false;

		for (int i = 0; i < candidates.Count; ++i)
		{
			auto const pCandidate = candidates.Owners[i];
			bool candidateHidden = false;

			if (!TryResolveOwnerFootHidden(pCandidate, pViewerHouse, candidateHidden))
				continue;

			if (firstQueryableCandidate < 0)
			{
				firstQueryableCandidate = i;
				firstQueryableHidden = candidateHidden;
			}

			if (!IsEnemyFootOwner(pCandidate, pViewerHouse))
				continue;

			if (!info.EnemyCandidateTechno)
			{
				info.EnemyCandidateTechno = pCandidate;
				info.EnemyCandidateSource = candidates.Sources[i];
				info.EnemyCandidateFootQuerySucceeded = true;
				info.EnemyCandidateFootHidden = candidateHidden;
			}

			if (firstEnemyCandidate < 0)
			{
				firstEnemyCandidate = i;
				firstEnemyHidden = candidateHidden;
			}

			if (candidateHidden)
			{
				firstHiddenEnemyCandidate = i;
				firstHiddenEnemyHidden = true;
				break;
			}
		}

		int selectedCandidate = firstHiddenEnemyCandidate;
		bool selectedHidden = firstHiddenEnemyHidden;

		if (selectedCandidate < 0)
		{
			selectedCandidate = firstEnemyCandidate;
			selectedHidden = firstEnemyHidden;
		}

		if (selectedCandidate < 0)
		{
			selectedCandidate = firstQueryableCandidate;
			selectedHidden = firstQueryableHidden;
		}

		if (selectedCandidate < 0)
			return false;

		info.OwnerTechno = candidates.Owners[selectedCandidate];
		info.OwnerSource = candidates.Sources[selectedCandidate];
		info.OwnerFootQuerySucceeded = true;
		info.OwnerFootHidden = selectedHidden;

		return true;
	}

	static TechnoClass* TryGetParticleSystemOwnerTechno(ParticleSystemClass* const pParticleSystem, ParticleSystemOwnerSource& source)
	{
		source = ParticleSystemOwnerSource::None;

		if (!pParticleSystem)
			return nullptr;

		if (auto const pParticleFieldOwner = TryFindParticleSystemOwnerByField(pParticleSystem, source, false))
			return pParticleFieldOwner;

		if (auto const pOwnerTechno = TryGetObjectTechno(pParticleSystem->Owner))
		{
			source = ParticleSystemOwnerSource::SystemOwnerTechno;
			return pOwnerTechno;
		}

		if (auto const pOwnerAnim = TryGetObjectAnim(pParticleSystem->Owner))
		{
			if (auto const pAnimOwnerTechno = TryGetAnimOwnerTechno(pOwnerAnim))
			{
				source = ParticleSystemOwnerSource::SystemOwnerAnim;
				return pAnimOwnerTechno;
			}
		}

		if (auto const pTargetTechno = TryGetObjectTechno(static_cast<ObjectClass*>(pParticleSystem->Target)))
		{
			source = ParticleSystemOwnerSource::TargetTechno;
			return pTargetTechno;
		}

		return nullptr;
	}

	static TechnoClass* TryGetOwnerTechno(ParticleClass* const pParticle, ParticleSystemOwnerSource& source)
	{
		return pParticle ? TryGetParticleSystemOwnerTechno(pParticle->ParticleSystem, source) : nullptr;
	}

	static bool IsWorldParticle(ParticleClass* const pParticle)
	{
		return pParticle && pParticle->Type && pParticle->ParticleSystem && pParticle->ParticleSystem->Type;
	}

	static bool AnalyzeParticle(ParticleClass* const pParticle, ParticleProbeInfo& info)
	{
		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled || !pRulesExt->PhobosFog_HideWorldParticles
			|| !IsWorldParticle(pParticle))
		{
			info.Reason = "DisabledOrInvalid";
			return false;
		}

		const auto pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver())
		{
			info.Reason = "NoViewer";
			return false;
		}

		int cellIndex = -1;

		info.OwnerTechno = TryGetOwnerTechno(pParticle, info.OwnerSource);

		if (pParticle && pParticle->ParticleSystem)
		{
			ParticleSystemOwnerSource diagnosticsSource = ParticleSystemOwnerSource::None;
			info.AnyFieldOwnerTechno = TryFindParticleSystemOwnerByField(pParticle->ParticleSystem, diagnosticsSource, true);
			info.AnyFieldOwnerSource = diagnosticsSource;
		}

		if (info.OwnerTechno)
		{
			bool ownerHidden = false;

			if (TryResolveOwnerFootHidden(info.OwnerTechno, pViewerHouse, ownerHidden))
			{
				info.OwnerFootQuerySucceeded = true;
				info.OwnerFootHidden = ownerHidden;

				if (TryResolveParticleSystemSourceCellHidden(pParticle ? pParticle->ParticleSystem : nullptr, info.OwnerTechno, pViewerHouse, info))
					return true;

				if (TryResolveEnemyDamageParticleConservativeHidden(pParticle ? pParticle->ParticleSystem : nullptr, info.OwnerTechno, pViewerHouse, info))
					return true;

				if (ApplyParticleSystemHiddenHold(pParticle ? pParticle->ParticleSystem : nullptr, info.OwnerTechno, pViewerHouse, ownerHidden, info))
					return true;

				info.Hidden = ownerHidden;
				info.Reason = ownerHidden ? "OwnerFootHidden" : "OwnerFootVisibleOrAllied";
				return ownerHidden;
			}
		}

		if (!TryGetParticleCellIndex(pParticle, cellIndex))
		{
			info.Reason = "NoParticleCell";
			return false;
		}

		info.HasCell = true;
		info.CellIndex = cellIndex;

		bool querySucceeded = false;
		const bool visible = IsCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);

		info.CellQuerySucceeded = querySucceeded;
		info.CellVisible = visible;
		info.Hidden = querySucceeded && !visible;
		info.Reason = querySucceeded ? (visible ? "ParticleCellVisible" : "ParticleCellHidden") : "ParticleCellQueryFailed";

		return info.Hidden;
	}

	static void LogParticleProbe(ParticleClass* const pParticle, const ParticleProbeInfo& info)
	{
		if (!ShouldLogProbe() || !ShouldEmitParticleLog(info.Hidden, info.OwnerTechno || info.AnyFieldOwnerTechno))
			return;

		const auto pParticleSystem = pParticle ? pParticle->ParticleSystem : nullptr;
		const auto pOwnerHouse = info.OwnerTechno ? info.OwnerTechno->Owner : nullptr;
		const auto pAnyOwnerHouse = info.AnyFieldOwnerTechno ? info.AnyFieldOwnerTechno->Owner : nullptr;

		Debug::Log("[PhobosFogParticle] Path=ParticleClass::Draw Frame=%d Particle=%p ParticleType=%s System=%p SystemType=%s Hidden=%s Reason=%s OwnerSource=%s Owner=%p OwnerID=%s OwnerHouse=%s AnyFieldSource=%s AnyFieldOwner=%p AnyFieldOwnerID=%s AnyFieldOwnerHouse=%s OwnerFootQuery=%s OwnerFootHidden=%s HiddenHold=%s HiddenHoldUntil=%d VisibleStable=%d SourceCell=%d SourceCellQuery=%s SourceCellVisible=%s HasCell=%s Cell=%d CellQuery=%s CellVisible=%s\n",
			Unsorted::CurrentFrame,
			pParticle,
			GetParticleTypeID(pParticle),
			pParticleSystem,
			GetParticleSystemTypeID(pParticleSystem),
			BoolText(info.Hidden),
			info.Reason,
			GetParticleSystemOwnerSourceName(info.OwnerSource),
			info.OwnerTechno,
			GetTechnoID(info.OwnerTechno),
			GetHouseID(pOwnerHouse),
			GetParticleSystemOwnerSourceName(info.AnyFieldOwnerSource),
			info.AnyFieldOwnerTechno,
			GetTechnoID(info.AnyFieldOwnerTechno),
			GetHouseID(pAnyOwnerHouse),
			BoolText(info.OwnerFootQuerySucceeded),
			BoolText(info.OwnerFootHidden),
			BoolText(info.HiddenHoldActive),
			info.HiddenHoldUntilFrame,
			info.VisibleStableFrames,
			info.SourceCellIndex,
			BoolText(info.SourceCellQuerySucceeded),
			BoolText(info.SourceCellVisible),
			BoolText(info.HasCell),
			info.CellIndex,
			BoolText(info.CellQuerySucceeded),
			BoolText(info.CellVisible));
	}

	static void LogParticleDrawHitProbe(ParticleClass* const pParticle, const char* const pStage, const ParticleProbeInfo* const pInfo, const bool hasHiddenDecision, const bool hiddenDecision)
	{
		if (!ShouldLogProbe() || !ShouldEmitParticleDrawHitProbe())
			return;

		const auto pParticleSystem = pParticle ? pParticle->ParticleSystem : nullptr;
		const auto pOwnerTechno = pInfo ? pInfo->OwnerTechno : nullptr;
		const auto pOwnerHouse = pOwnerTechno ? pOwnerTechno->Owner : nullptr;
		const auto pAnyOwnerTechno = pInfo ? pInfo->AnyFieldOwnerTechno : nullptr;
		const auto pAnyOwnerHouse = pAnyOwnerTechno ? pAnyOwnerTechno->Owner : nullptr;

		int cellIndex = -1;
		const bool hasCell = TryGetParticleCellIndex(pParticle, cellIndex);
		bool cellQuerySucceeded = false;
		bool cellVisible = false;

		if (hasCell && HouseClass::CurrentPlayer)
			cellVisible = IsCellVisibleToViewerOrAllies(HouseClass::CurrentPlayer, cellIndex, cellQuerySucceeded);

		bool ownerCellQuerySucceeded = false;
		bool ownerCellVisible = false;
		int ownerCellIndex = -1;
		const bool hasOwnerCell = TryGetTechnoCellIndex(pOwnerTechno, ownerCellIndex);

		if (hasOwnerCell && HouseClass::CurrentPlayer)
			ownerCellVisible = IsCellVisibleToViewerOrAllies(HouseClass::CurrentPlayer, ownerCellIndex, ownerCellQuerySucceeded);

		Debug::Log("[PhobosFog][ParticleDrawHit] Stage=%s Frame=%d Particle=%p ParticleTypePtr=%p ParticleType=%s ParticleCoords=%d,%d,%d HasCell=%s Cell=%d CellQuery=%s CellVisible=%s System=%p SystemTypePtr=%p SystemType=%s SystemParticleCount=%d OwnerSource=%s Owner=%p OwnerID=%s OwnerHouse=%s AnyFieldSource=%s AnyFieldOwner=%p AnyFieldOwnerID=%s AnyFieldOwnerHouse=%s HasOwnerCell=%s OwnerCell=%d OwnerCellQuery=%s OwnerCellVisible=%s HasDecision=%s HiddenDecision=%s Reason=%s OwnerFootQuery=%s OwnerFootHidden=%s\n",
			pStage,
			Unsorted::CurrentFrame,
			pParticle,
			pParticle ? pParticle->Type : nullptr,
			GetParticleTypeID(pParticle),
			pParticle ? pParticle->Location.X : 0,
			pParticle ? pParticle->Location.Y : 0,
			pParticle ? pParticle->Location.Z : 0,
			BoolText(hasCell),
			cellIndex,
			BoolText(cellQuerySucceeded),
			BoolText(cellVisible),
			pParticleSystem,
			pParticleSystem ? pParticleSystem->Type : nullptr,
			GetParticleSystemTypeID(pParticleSystem),
			pParticleSystem ? pParticleSystem->Particles.Count : -1,
			pInfo ? GetParticleSystemOwnerSourceName(pInfo->OwnerSource) : "NotResolved",
			pOwnerTechno,
			GetTechnoID(pOwnerTechno),
			GetHouseID(pOwnerHouse),
			pInfo ? GetParticleSystemOwnerSourceName(pInfo->AnyFieldOwnerSource) : "NotResolved",
			pAnyOwnerTechno,
			GetTechnoID(pAnyOwnerTechno),
			GetHouseID(pAnyOwnerHouse),
			BoolText(hasOwnerCell),
			ownerCellIndex,
			BoolText(ownerCellQuerySucceeded),
			BoolText(ownerCellVisible),
			BoolText(hasHiddenDecision),
			BoolText(hiddenDecision),
			pInfo ? pInfo->Reason : "BeforeAnalyze",
			pInfo ? BoolText(pInfo->OwnerFootQuerySucceeded) : "false",
			pInfo ? BoolText(pInfo->OwnerFootHidden) : "false");
	}

	static bool AnalyzeParticleSystem(ParticleSystemClass* const pParticleSystem, ParticleProbeInfo& info)
	{
		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Enabled || !pRulesExt->PhobosFog_HideWorldParticles
			|| !pParticleSystem || !pParticleSystem->Type)
		{
			info.Reason = "DisabledOrInvalid";
			return false;
		}

		const auto pViewerHouse = HouseClass::CurrentPlayer;

		if (!pViewerHouse || pViewerHouse->IsObserver())
		{
			info.Reason = "NoViewer";
			return false;
		}

		info.AnyFieldOwnerTechno = TryFindParticleSystemOwnerByField(pParticleSystem, info.AnyFieldOwnerSource, true);
		TrySelectParticleSystemOwnerCandidate(pParticleSystem, pViewerHouse, info);

		if (info.OwnerTechno)
		{
			if (TryResolveParticleSystemSourceCellHidden(pParticleSystem, info.OwnerTechno, pViewerHouse, info))
				return true;

			if (TryResolveEnemyDamageParticleConservativeHidden(pParticleSystem, info.OwnerTechno, pViewerHouse, info))
				return true;

			if (ApplyParticleSystemHiddenHold(pParticleSystem, info.OwnerTechno, pViewerHouse, info.OwnerFootHidden, info))
				return true;

			info.Hidden = info.OwnerFootHidden;
			info.Reason = info.OwnerFootHidden ? "OwnerFootHidden" : "OwnerFootVisibleOrAllied";
			return info.OwnerFootHidden;
		}

		int cellIndex = -1;

		if (!TryGetParticleSystemCellIndex(pParticleSystem, cellIndex))
		{
			info.Reason = "NoParticleSystemCell";
			return false;
		}

		info.HasCell = true;
		info.CellIndex = cellIndex;

		bool querySucceeded = false;
		const bool visible = IsCellVisibleToViewerOrAllies(pViewerHouse, cellIndex, querySucceeded);

		info.CellQuerySucceeded = querySucceeded;
		info.CellVisible = visible;
		info.Hidden = querySucceeded && !visible;
		info.Reason = querySucceeded ? (visible ? "ParticleSystemCellVisible" : "ParticleSystemCellHidden") : "ParticleSystemCellQueryFailed";

		return info.Hidden;
	}

	static void LogParticleSystemProbe(ParticleSystemClass* const pParticleSystem, const ParticleProbeInfo& info)
	{
		if (!ShouldLogProbe() || !pParticleSystem)
			return;

		if (!ShouldEmitParticleSystemLog(info.OwnerTechno || info.AnyFieldOwnerTechno || info.EnemyCandidateTechno))
			return;

		const auto pOwnerHouse = info.OwnerTechno ? info.OwnerTechno->Owner : nullptr;
		const auto pAnyOwnerHouse = info.AnyFieldOwnerTechno ? info.AnyFieldOwnerTechno->Owner : nullptr;
		const auto pEnemyCandidateHouse = info.EnemyCandidateTechno ? info.EnemyCandidateTechno->Owner : nullptr;

		Debug::Log("[PhobosFogParticle] Path=ParticleSystemClass::Draw Frame=%d System=%p SystemType=%s Hidden=%s WillSkip=%s Reason=%s CandidateCount=%d OwnerSource=%s Owner=%p OwnerID=%s OwnerHouse=%s EnemyCandidateSource=%s EnemyCandidate=%p EnemyCandidateID=%s EnemyCandidateHouse=%s EnemyCandidateQuery=%s EnemyCandidateHidden=%s AnyFieldSource=%s AnyFieldOwner=%p AnyFieldOwnerID=%s AnyFieldOwnerHouse=%s SystemOwner=%p Target=%p OwnerHouseField=%s TargetCoords=%d,%d,%d SystemCoords=%d,%d,%d OwnerFootQuery=%s OwnerFootHidden=%s HiddenHold=%s HiddenHoldUntil=%d VisibleStable=%d SourceCell=%d SourceCellQuery=%s SourceCellVisible=%s HasCell=%s Cell=%d CellQuery=%s CellVisible=%s ParticleCount=%d\n",
			Unsorted::CurrentFrame,
			pParticleSystem,
			GetParticleSystemTypeID(pParticleSystem),
			BoolText(info.Hidden),
			BoolText(info.Hidden),
			info.Reason,
			info.OwnerCandidateCount,
			GetParticleSystemOwnerSourceName(info.OwnerSource),
			info.OwnerTechno,
			GetTechnoID(info.OwnerTechno),
			GetHouseID(pOwnerHouse),
			GetParticleSystemOwnerSourceName(info.EnemyCandidateSource),
			info.EnemyCandidateTechno,
			GetTechnoID(info.EnemyCandidateTechno),
			GetHouseID(pEnemyCandidateHouse),
			BoolText(info.EnemyCandidateFootQuerySucceeded),
			BoolText(info.EnemyCandidateFootHidden),
			GetParticleSystemOwnerSourceName(info.AnyFieldOwnerSource),
			info.AnyFieldOwnerTechno,
			GetTechnoID(info.AnyFieldOwnerTechno),
			GetHouseID(pAnyOwnerHouse),
			pParticleSystem->Owner,
			pParticleSystem->Target,
			GetHouseID(pParticleSystem->OwnerHouse),
			pParticleSystem->TargetCoords.X,
			pParticleSystem->TargetCoords.Y,
			pParticleSystem->TargetCoords.Z,
			pParticleSystem->Location.X,
			pParticleSystem->Location.Y,
			pParticleSystem->Location.Z,
			BoolText(info.OwnerFootQuerySucceeded),
			BoolText(info.OwnerFootHidden),
			BoolText(info.HiddenHoldActive),
			info.HiddenHoldUntilFrame,
			info.VisibleStableFrames,
			info.SourceCellIndex,
			BoolText(info.SourceCellQuerySucceeded),
			BoolText(info.SourceCellVisible),
			BoolText(info.HasCell),
			info.CellIndex,
			BoolText(info.CellQuerySucceeded),
			BoolText(info.CellVisible),
			pParticleSystem->Particles.Count);
	}
}

DEFINE_HOOK(0x62BE30, ParticleClass_Gas_AI_DriftSpeed, 0x5)
{
	enum { ContinueAI = 0x62BE60 };

	GET(ParticleClass*, pParticle, EBP);

	const auto pExt = ParticleTypeExt::ExtMap.Find(pParticle->Type);
	const int maxDriftSpeed = pExt->Gas_MaxDriftSpeed;
	const int minDriftSpeed = -maxDriftSpeed;

	if (pParticle->Velocity.X > maxDriftSpeed)
		pParticle->Velocity.X = maxDriftSpeed;
	else if (pParticle->Velocity.X < minDriftSpeed)
		pParticle->Velocity.X = minDriftSpeed;

	if (pParticle->Velocity.Y > maxDriftSpeed)
		pParticle->Velocity.Y = maxDriftSpeed;
	else if (pParticle->Velocity.Y < minDriftSpeed)
		pParticle->Velocity.Y = minDriftSpeed;

	return ContinueAI;
}

DEFINE_HOOK(0x62CECE, ParticleClass_Draw_PhobosFogVisibility, 0x6)
{
	enum { SkipDrawing = 0x62D28B };

	GET(ParticleClass*, pThis, EDI);

	PhobosFogParticle::LogParticleDrawHitProbe(pThis, "Entry", nullptr, false, false);

	PhobosFogParticle::ParticleProbeInfo probeInfo;
	const bool shouldHide = PhobosFogParticle::AnalyzeParticle(pThis, probeInfo);
	PhobosFogParticle::LogParticleDrawHitProbe(pThis, "AfterAnalyze", &probeInfo, true, shouldHide);
	PhobosFogParticle::LogParticleProbe(pThis, probeInfo);

	if (shouldHide)
		return SkipDrawing;

	return 0;
}

DEFINE_HOOK(0x62E287, ParticleSystemClass_Draw_PhobosFogProbe, 0x6)
{
	enum { SkipDrawing = 0x62E376 };

	GET(ParticleSystemClass*, pThis, EDI);

	PhobosFogParticle::ParticleProbeInfo probeInfo;
	const bool shouldHide = PhobosFogParticle::AnalyzeParticleSystem(pThis, probeInfo);
	PhobosFogParticle::LogParticleSystemProbe(pThis, probeInfo);

	if (shouldHide)
		return SkipDrawing;

	return 0;
}
