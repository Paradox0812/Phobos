#pragma once
#include <GeneralStructures.h>
#include <HouseClass.h>

#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>

#include <climits>

class HouseExt
{
public:
	using base_type = HouseClass;

	static constexpr DWORD Canary = 0x11111111;
	static constexpr size_t ExtPointerOffset = 0x16098;
	static constexpr bool ShouldConsiderInvalidatePointer = true;

	enum class PhobosFogCellState : unsigned char
	{
		Unknown = 0,
		Explored = 1,
		Visible = 2
	};

	enum class PhobosFogStateTouchReason : unsigned char
	{
		Unknown = 0,
		Reset,
		EnsureResize,
		DegradeVisibleToExplored,
		MarkExplored,
		MarkVisible,
		MarkVisibleUntil,
		MarkAreaVisible,
		MarkCellSpreadVisible,
		MarkAllExplored,
		MarkAllVisible,
		FullMapVisibleUntil,
		SpySatPersistentVisibleEdge,
		Other,
		Count
	};

	static constexpr size_t PhobosFogStateTouchReasonCount = static_cast<size_t>(PhobosFogStateTouchReason::Count);

	struct PhobosFogRevealAreaStats
	{
		size_t AffectedCells = 0;
		size_t InvalidCells = 0;
		size_t UnknownPromoted = 0;
		size_t ExploredPromoted = 0;
		size_t VisibleExtended = 0;
		size_t AlreadyVisibleEnoughSkipped = 0;
		size_t OutOfBoundsSkipped = 0;
		short MinCellX = SHRT_MAX;
		short MaxCellX = SHRT_MIN;
		short MinCellY = SHRT_MAX;
		short MaxCellY = SHRT_MIN;
	};

	class ExtData final : public Extension<HouseClass>
	{
	public:
		std::vector<PhobosFogCellState> PhobosFog_CellStates;
		std::vector<int> PhobosFog_LastVisibleFrames;
		unsigned int PhobosFog_StateVersion;
		unsigned long long PhobosFog_StateVersionTouchCount;
		unsigned long long PhobosFog_StateVersionTouchReasons[PhobosFogStateTouchReasonCount];
		unsigned int PhobosFog_OverlayEffectiveVersion;
		unsigned long long PhobosFog_OverlayEffectiveTouchCount;
		bool PhobosFog_OverlayEffectiveBatchActive;
		std::vector<int> PhobosFog_OverlayEffectiveBatchTouchedCells;
		std::vector<PhobosFogCellState> PhobosFog_OverlayEffectiveBatchOriginalStates;
		std::vector<unsigned int> PhobosFog_OverlayEffectiveBatchCellStamps;
		unsigned int PhobosFog_OverlayEffectiveBatchStamp;
		size_t PhobosFog_LastOverlayEffectiveBatchTouchedCells;
		size_t PhobosFog_LastOverlayEffectiveBatchChangedCells;
		size_t PhobosFog_LastRefreshProviders;
		size_t PhobosFog_LastRefreshVisibleCells;
		int PhobosFog_FullMapVisibleUntilFrame;
		bool PhobosFog_LastSpySatActive;
		bool PhobosFog_LastFullMapHardVisible;
		bool PhobosFog_LastRefreshResized;
		std::vector<BuildingClass*> PowerPlantEnhancers;
		std::vector<BuildingClass*> OwnedLimboDeliveredBuildings;
		std::vector<TechnoClass*> OwnedCountedHarvesters;
		bool ForceOnlyTargetHouseEnemy;
		int ForceOnlyTargetHouseEnemyMode;

		CounterClass LimboAircraft;  // Currently owned aircraft in limbo
		CounterClass LimboBuildings; // Currently owned buildings in limbo
		CounterClass LimboInfantry;  // Currently owned infantry in limbo
		CounterClass LimboVehicles;  // Currently owned vehicles in limbo

		BuildingClass* Factory_BuildingType;
		BuildingClass* Factory_InfantryType;
		BuildingClass* Factory_VehicleType;
		BuildingClass* Factory_NavyType;
		BuildingClass* Factory_AircraftType;

		CDTimerClass CombatAlertTimer;
		CDTimerClass AISuperWeaponDelayTimer;
		CDTimerClass AIFireSaleDelayTimer;

		//Read from INI
		Nullable<bool> RepairBaseNodes[3];

		// FactoryPlants with Allow/DisallowTypes set.
		std::vector<BuildingClass*> RestrictedFactoryPlants;

		int LastBuiltNavalVehicleType;
		int ProducingNavalUnitTypeIndex;

		// Factories that exist but don't count towards multiple factory bonus.
		int NumAirpads_NonMFB;
		int NumBarracks_NonMFB;
		int NumWarFactories_NonMFB;
		int NumConYards_NonMFB;
		int NumShipyards_NonMFB;

		std::map<int, std::vector<int>> SuspendedEMPulseSWs;

		// standalone? no need and not a good idea
		struct SWExt
		{
			int ShotCount;
		};
		std::vector<SWExt> SuperExts;

		int ForceEnemyIndex;
		int TeamDelay;
		bool FreeRadar;
		bool ForceRadar;

		bool PlayerAutoRepair;

		ExtData(HouseClass* OwnerObject) : Extension<HouseClass>(OwnerObject)
			, PhobosFog_CellStates {}
			, PhobosFog_LastVisibleFrames {}
			, PhobosFog_StateVersion { 1 }
			, PhobosFog_StateVersionTouchCount { 0 }
			, PhobosFog_StateVersionTouchReasons {}
			, PhobosFog_OverlayEffectiveVersion { 1 }
			, PhobosFog_OverlayEffectiveTouchCount { 0 }
			, PhobosFog_OverlayEffectiveBatchActive { false }
			, PhobosFog_OverlayEffectiveBatchTouchedCells {}
			, PhobosFog_OverlayEffectiveBatchOriginalStates {}
			, PhobosFog_OverlayEffectiveBatchCellStamps {}
			, PhobosFog_OverlayEffectiveBatchStamp { 1 }
			, PhobosFog_LastOverlayEffectiveBatchTouchedCells { 0 }
			, PhobosFog_LastOverlayEffectiveBatchChangedCells { 0 }
			, PhobosFog_LastRefreshProviders { 0 }
			, PhobosFog_LastRefreshVisibleCells { 0 }
			, PhobosFog_FullMapVisibleUntilFrame { 0 }
			, PhobosFog_LastSpySatActive { false }
			, PhobosFog_LastFullMapHardVisible { false }
			, PhobosFog_LastRefreshResized { false }
			, PowerPlantEnhancers {}
			, OwnedLimboDeliveredBuildings {}
			, OwnedCountedHarvesters {}
			, LimboAircraft {}
			, LimboBuildings {}
			, LimboInfantry {}
			, LimboVehicles {}
			, Factory_BuildingType { nullptr }
			, Factory_InfantryType { nullptr }
			, Factory_VehicleType { nullptr }
			, Factory_NavyType { nullptr }
			, Factory_AircraftType { nullptr }
			, AISuperWeaponDelayTimer {}
			, RepairBaseNodes { }
			, RestrictedFactoryPlants {}
			, LastBuiltNavalVehicleType { -1 }
			, ProducingNavalUnitTypeIndex { -1 }
			, CombatAlertTimer {}
			, NumAirpads_NonMFB { 0 }
			, NumBarracks_NonMFB { 0 }
			, NumWarFactories_NonMFB { 0 }
			, NumConYards_NonMFB { 0 }
			, NumShipyards_NonMFB { 0 }
			, AIFireSaleDelayTimer {}
			, SuspendedEMPulseSWs {}
			, SuperExts(SuperWeaponTypeClass::Array.Count)
			, ForceEnemyIndex(-1)
			, ForceOnlyTargetHouseEnemy { false }
			, ForceOnlyTargetHouseEnemyMode { -1 }
			, TeamDelay(-1)
			, FreeRadar(false)
			, ForceRadar(false)
			, PlayerAutoRepair(true)
		{ }

		bool OwnsLimboDeliveredBuilding(BuildingClass* pBuilding) const;
		void AddToLimboTracking(TechnoTypeClass* pTechnoType);
		void RemoveFromLimboTracking(TechnoTypeClass* pTechnoType);
		int CountOwnedPresentAndLimboed(TechnoTypeClass* pTechnoType) const;
		void UpdateNonMFBFactoryCounts(AbstractType rtti, bool remove, bool isNaval);
		int GetFactoryCountWithoutNonMFB(AbstractType rtti, bool isNaval) const;
		float GetRestrictedFactoryPlantMult(TechnoTypeClass* pTechnoType) const;

		int GetForceEnemyIndex();
		void SetForceEnemyIndex(int EnemyIndex);

		void ResetPhobosFogState();
		void ResetPhobosFogRuntimeStateOnly(bool initializeSpySatEdgeState);
		void TouchPhobosFogStateVersion(PhobosFogStateTouchReason reason);
		void TouchPhobosFogOverlayEffectiveVersion();
		void BeginPhobosFogOverlayEffectiveBatch();
		void TrackPhobosFogOverlayOriginalCell(int cellIndex);
		void EndPhobosFogOverlayEffectiveBatch();
		void AbortPhobosFogOverlayEffectiveBatch();
		bool EnsurePhobosFogStateSize();
		void DegradePhobosFogVisibility(int currentFrame);
		bool MarkPhobosFogCellExplored(CellStruct cell);
		size_t MarkPhobosFogAreaExplored(CellStruct center, double radius);
		size_t MarkPhobosFogCellSpreadExplored(CellStruct center, size_t spread);
		size_t MarkAllPhobosFogCellsExplored();
		bool MarkPhobosFogCellVisible(CellStruct cell, int currentFrame);
		bool MarkPhobosFogCellVisibleUntil(CellStruct cell, int visibleUntilFrame);
		size_t MarkPhobosFogAreaVisibleUntil(CellStruct center, double radius, int visibleUntilFrame, PhobosFogRevealAreaStats* pStats = nullptr);
		size_t MarkPhobosFogCellSpreadVisibleUntil(CellStruct center, size_t spread, int visibleUntilFrame, PhobosFogRevealAreaStats* pStats = nullptr);
		size_t MarkAllPhobosFogCellsVisibleUntil(int visibleUntilFrame);
		void ExtendPhobosFogFullMapVisibleUntil(int visibleUntilFrame);
		bool IsPhobosFogFullMapHardVisible() const;
		PhobosFogCellState GetEffectivePhobosFogCellState(int cellIndex) const;
		bool IsPhobosFogCellHardVisible(int cellIndex) const;
		static bool TryGetEffectivePhobosFogCellStateForHouse(HouseClass* pHouse, int cellIndex, PhobosFogCellState& state);
		static bool TryGetEffectivePhobosFogCellStateForViewerOrAllies(HouseClass* pViewerHouse, int cellIndex, PhobosFogCellState& state);
		static bool IsPhobosFogCellHardVisibleToViewerOrAllies(HouseClass* pViewerHouse, int cellIndex, bool& querySucceeded);
		void ResetPhobosFogDebugRefreshStats(bool resized);
		void AddPhobosFogDebugVisibilityProvider();
		void AddPhobosFogDebugVisibleCell();
		void LogPhobosFogDebugSummary() const;

		virtual ~ExtData() = default;

		virtual void LoadFromINIFile(CCINIClass* pINI) override;
		//virtual void Initialize() override;
		virtual void InvalidatePointer(void* ptr, bool bRemoved) override;

		void UpdateVehicleProduction();

		virtual void LoadFromStream(PhobosStreamReader& Stm) override;
		virtual void SaveToStream(PhobosStreamWriter& Stm) override;

	private:
		template <typename T>
		void Serialize(T& Stm);
		bool LoadPhobosFogExploredPayload(PhobosStreamReader& Stm, std::vector<PhobosFogCellState>& cellStates, bool& payloadFound) const;
		void SavePhobosFogExploredPayload(PhobosStreamWriter& Stm) const;
		bool UpdateHarvesterProduction();
		bool MarkPhobosFogCellVisibleUntil(CellStruct cell, int visibleUntilFrame, PhobosFogStateTouchReason reason);
	};

	class ExtContainer final : public Container<HouseExt>
	{
	public:
		ExtContainer();
		~ExtContainer();

		virtual bool InvalidateExtDataIgnorable(void* const ptr) const override
		{
			auto const abs = static_cast<AbstractClass*>(ptr)->WhatAmI();

			switch (abs)
			{
			case AbstractType::Building:
				return false;
			}

			return true;
		}
	};

	static ExtContainer ExtMap;

	static bool LoadGlobals(PhobosStreamReader& Stm);
	static bool SaveGlobals(PhobosStreamWriter& Stm);
	static void RequestPhobosFogForceRefresh();
	static bool ConsumePhobosFogForceRefresh();

	static int ActiveHarvesterCount(HouseClass* pThis);
	static int TotalHarvesterCount(HouseClass* pThis);
	static HouseClass* GetHouseKind(OwnerHouseKind kind, bool allowRandom, HouseClass* pDefault, HouseClass* pInvoker = nullptr, HouseClass* pVictim = nullptr);
	static CellClass* GetEnemyBaseGatherCell(HouseClass* pTargetHouse, HouseClass* pCurrentHouse, CoordStruct defaultCurrentCoords, SpeedType speedTypeZone, int extraDistance = 0);
	static void GetAIChronoshiftSupers(HouseClass* pThis, SuperClass*& pSuperCSphere, SuperClass*& pSuperCWarp);

	static void ForceOnlyTargetHouseEnemy(HouseClass* pThis, int mode = -1);
	static void SetSkirmishHouseName(HouseClass* pHouse);

	static bool IsDisabledFromShell(
	HouseClass const* pHouse, BuildingTypeClass const* pItem);

	static size_t FindOwnedIndex(
	HouseClass const* pHouse, int idxParentCountry,
	Iterator<TechnoTypeClass const*> items, size_t start = 0);

	static size_t FindBuildableIndex(
		HouseClass const* pHouse, int idxParentCountry,
		Iterator<TechnoTypeClass const*> items, size_t start = 0);

	template <typename T>
	static T* FindOwned(
		HouseClass const* const pHouse, int const idxParent,
		Iterator<T*> const items, size_t const start = 0)
	{
		auto const index = FindOwnedIndex(pHouse, idxParent, items, start);
		return index < items.size() ? items[index] : nullptr;
	}

	template <typename T>
	static T* FindBuildable(
		HouseClass const* const pHouse, int const idxParent,
		Iterator<T*> const items, size_t const start = 0)
	{
		auto const index = FindBuildableIndex(pHouse, idxParent, items, start);
		return index < items.size() ? items[index] : nullptr;
	}

	static std::vector<int> AIProduction_CreationFrames;
	static std::vector<int> AIProduction_Values;
	static std::vector<int> AIProduction_BestChoices;
	static std::vector<int> AIProduction_BestChoicesNaval;
	static bool PhobosFog_ForceNextRefresh;

	static CanBuildResult BuildLimitGroupCheck(const HouseClass* pThis, const TechnoTypeClass* pItem, bool buildLimitOnly, bool includeQueued);
	static bool ReachedBuildLimit(const HouseClass* pHouse, const TechnoTypeClass* pType, bool ignoreQueued);

	static void CalculatePowerSurplus(HouseClass* pThis);
};
