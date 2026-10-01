#include "PhobosFogOverlay.h"

#include <Ext/Anim/Body.h>
#include <Ext/House/Body.h>
#include <Ext/Rules/Body.h>

#include <MapClass.h>
#include <Surface.h>
#include <TacticalClass.h>
#include <Unsorted.h>
#include <VeinholeMonsterClass.h>
#include <Utilities/Debug.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

///
/// Veinhole Monster
///

namespace PhobosFogExploredOverlay
{
	constexpr int MaxOverlayCellsPerFrame = 8192;

	enum class OverlayCellKind : unsigned char
	{
		Unknown,
		ExploredOverlay,
		Visible
	};

	enum class DiamondEdge : unsigned char
	{
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	static const char* GetOverlayCellKindName(const OverlayCellKind kind)
	{
		switch (kind)
		{
		case OverlayCellKind::Unknown:
			return "Unknown";
		case OverlayCellKind::ExploredOverlay:
			return "Explored";
		case OverlayCellKind::Visible:
			return "Visible";
		default:
			return "<invalid>";
		}
	}

	static const char* GetDiamondEdgeName(const DiamondEdge edge)
	{
		switch (edge)
		{
		case DiamondEdge::TopLeft:
			return "TopLeft";
		case DiamondEdge::TopRight:
			return "TopRight";
		case DiamondEdge::BottomLeft:
			return "BottomLeft";
		case DiamondEdge::BottomRight:
			return "BottomRight";
		default:
			return "<invalid>";
		}
	}

	struct CliffCoverEdge
	{
		Point2D HighA {};
		Point2D HighB {};
		Point2D LowA {};
		Point2D LowB {};
		OverlayCellKind NeighborKind { OverlayCellKind::Unknown };
	};

	struct FrontierInfo
	{
		bool IsFrontier { false };
		bool HasVisibleNeighbor { false };
		bool HasVisibleNeighbor4[4] {};
	};

	struct OverlaySpan
	{
		int Y { 0 };
		int X1 { 0 };
		int X2 { 0 };
		int Alpha { 0 };
	};

	struct OverlaySpanEvent
	{
		int X { 0 };
		int Alpha { 0 };
		bool Begin { false };
	};

	struct OverlayDrawRect
	{
		int X { 0 };
		int Y { 0 };
		int Width { 0 };
		int Height { 0 };
		int Alpha { 0 };
	};

	struct OverlayCellShapeTemplateRow
	{
		int LocalY { 0 };
		int X1Offset { 0 };
		int X2Offset { 0 };
	};

	struct OverlayCellShapeTemplate
	{
		bool Valid { false };
		bool Diamond { false };
		int Width { 0 };
		int Height { 0 };
		std::vector<OverlayCellShapeTemplateRow> Rows {};
	};

	struct OverlayCellShapeTemplateStats
	{
		bool Enabled { false };
		size_t RectTemplateRows { 0 };
		size_t DiamondTemplateRows { 0 };
		size_t CandidateRows { 0 };
		size_t YRejectedRows { 0 };
		size_t XRejectedRows { 0 };
		size_t VisibleRows { 0 };
		size_t InstantiatedRows { 0 };
		size_t ClippedRows { 0 };
		int Fallbacks { 0 };
	};

	struct OverlayTemplateVisibleRange
	{
		size_t BeginIndex { 0 };
		size_t EndIndex { 0 };
	};

	struct CliffProbeStats
	{
		int Candidates { 0 };
		int Accepted { 0 };
		int AcceptedCliffTile { 0 };
		int AcceptedOrdinaryForward { 0 };
		int RejectedNotHigher { 0 };
		int RejectedBackFace { 0 };
		int RejectedThreshold { 0 };
		int RejectedBelowThreshold { 0 };
		int RejectedInvalid { 0 };
		int RejectedUnknown { 0 };
		int CliffTileCandidates { 0 };
		int CliffTileAccepted { 0 };
		int CliffTileRejected { 0 };
		int NonCliffAccepted { 0 };
		int AcceptedByCurrentCliffTile { 0 };
		int AcceptedByNeighborCliffTile { 0 };
		int RejectedBackFaceByCurrentCliffTile { 0 };
		int RejectedBackFaceByNeighborCliffTile { 0 };
		int ByOffset[4] {};
		int AcceptedByOffset[4] {};
		int RejectedBackFaceByOffset[4] {};
		int ByEdge[4] {};
		int AcceptedByEdge[4] {};
		int RejectedBackFaceByEdge[4] {};
		int CollectCalls { 0 };
		int CollectLines { 0 };
		int DetailLines { 0 };
	};

	struct HeightFaceStats
	{
		int Candidates { 0 };
		int Accepted { 0 };
		int MinDrop { 0 };
		int MaxDrop { 0 };
	};

	struct OverlayPerfSnapshot
	{
		int Frame { 0 };
		int SourceCells { 0 };
		size_t RawMainSpans { 0 };
		size_t MergedMainSpans { 0 };
		size_t PreCompactMainSpans { 0 };
		size_t CompactMainSpans { 0 };
		int CompactClosedGaps { 0 };
		bool HeightFaceEnabled { false };
		int HeightFaceAccepted { 0 };
		size_t HeightFaceRawSpans { 0 };
		int HeightFaceMaxDrop { 0 };
		int FallbackDilationY { 0 };
		size_t FallbackDilationRawSpans { 0 };
		size_t FinalMergeInputSpans { 0 };
		size_t FinalMergedSpans { 0 };
		int ClosedGaps { 0 };
		size_t UnionSpans { 0 };
		int DrawRects { 0 };
		int DrawRectBatchReduction { 0 };
		size_t DrawRectScratchCapacity { 0 };
		int GeometryProbes { 0 };
		int VisibilityQueries { 0 };
		size_t RowBucketInputSpans { 0 };
		int RowBucketNonEmptyRows { 0 };
		size_t RowBucketMainUnionSpans { 0 };
		bool TemplateEnabled { false };
		size_t RectTemplateRows { 0 };
		size_t DiamondTemplateRows { 0 };
		size_t TemplateCandidateRows { 0 };
		size_t TemplateYRejectedRows { 0 };
		size_t TemplateXRejectedRows { 0 };
		size_t TemplateVisibleRows { 0 };
		size_t TemplateInstantiatedRows { 0 };
		size_t TemplateClippedRows { 0 };
		int TemplateFallbacks { 0 };
		int MaxDrawRects { 0 };
		bool HitMaxDrawRects { false };
		bool SoftEdgeEnabled { false };
		bool LegacyCliffCoverBypassed { false };
		bool OverlayCacheEnabled { false };
		unsigned int PhobosFogStateVersion { 0 };
		unsigned int OverlayEffectiveVersion { 0 };
		unsigned int EffectiveVisibilityVersionHash { 0 };
		unsigned int OverlayEffectiveVisibilityVersionHash { 0 };
		unsigned int OverlayViewportHash { 0 };
		unsigned int OverlayConfigHash { 0 };
		bool OverlayCacheAllowed { false };
		const char* OverlayCacheDisabledReason { "StageAOnly" };
		bool OverlayCacheHit { false };
		bool OverlayCacheMiss { false };
		bool OverlayCacheRebuild { false };
		unsigned long long OverlayCacheHitCount { 0 };
		unsigned long long OverlayCacheMissCount { 0 };
		unsigned long long OverlayCacheRebuildCount { 0 };
		int OverlayCacheHitStreak { 0 };
		size_t OverlayCacheCachedFinalSpans { 0 };
		bool TemporalVisibilityActive { false };
		bool TemporalCellExpiryScanSkipped { true };
		int TemporalFullMapFirstInvalidFrame { 0 };
		int TemporalCacheFirstInvalidFrame { 0 };
		int TemporalCacheExpiresInFrames { 0 };
		bool TemporalCacheHitBlockedByExpiry { false };
		bool StageAOnly { true };
		unsigned long long StateVersionTouchCount { 0 };
		unsigned long long StateVersionTouchReasons[HouseExt::PhobosFogStateTouchReasonCount] {};
		unsigned long long OverlayEffectiveTouchCount { 0 };
		size_t OverlayEffectiveBatchTouchedCells { 0 };
		size_t OverlayEffectiveBatchChangedCells { 0 };
	};

	struct OverlayCacheStageADiagnostics
	{
		unsigned int PhobosFogStateVersion { 0 };
		unsigned int OverlayEffectiveVersion { 0 };
		unsigned int EffectiveVisibilityVersionHash { 0 };
		unsigned int OverlayEffectiveVisibilityVersionHash { 0 };
		unsigned int OverlayViewportHash { 0 };
		unsigned int OverlayConfigHash { 0 };
		bool CacheAllowed { false };
		const char* DisabledReason { "StageAOnly" };
		bool TemporalVisibilityActive { false };
		int TemporalFullMapFirstInvalidFrame { 0 };
		int TemporalCacheFirstInvalidFrame { 0 };
		unsigned long long StateVersionTouchCount { 0 };
		unsigned long long StateVersionTouchReasons[HouseExt::PhobosFogStateTouchReasonCount] {};
		unsigned long long OverlayEffectiveTouchCount { 0 };
		size_t OverlayEffectiveBatchTouchedCells { 0 };
		size_t OverlayEffectiveBatchChangedCells { 0 };
	};

	struct OverlayRegionCacheKey
	{
		HouseClass* ViewerHouse { nullptr };
		unsigned int OverlayEffectiveVisibilityVersionHash { 0 };
		unsigned int OverlayViewportHash { 0 };
		unsigned int OverlayConfigHash { 0 };
	};

	struct OverlayRegionCacheStats
	{
		int SourceCells { 0 };
		size_t RawMainSpans { 0 };
		size_t MergedMainSpans { 0 };
		size_t PreCompactMainSpans { 0 };
		size_t CompactMainSpans { 0 };
		int CompactClosedGaps { 0 };
		bool HeightFaceEnabled { false };
		int HeightFaceAccepted { 0 };
		size_t HeightFaceRawSpans { 0 };
		int HeightFaceMaxDrop { 0 };
		int FallbackDilationY { 0 };
		size_t FallbackDilationRawSpans { 0 };
		size_t FinalMergeInputSpans { 0 };
		size_t FinalMergedSpans { 0 };
		int ClosedGaps { 0 };
		int DrawRects { 0 };
		bool HitMaxDrawRects { false };
		bool SoftEdgeEnabled { false };
		bool LegacyCliffCoverBypassed { false };
		bool TemplateEnabled { false };
		size_t RectTemplateRows { 0 };
		size_t DiamondTemplateRows { 0 };
		size_t TemplateCandidateRows { 0 };
		size_t TemplateYRejectedRows { 0 };
		size_t TemplateXRejectedRows { 0 };
		size_t TemplateVisibleRows { 0 };
		size_t TemplateInstantiatedRows { 0 };
		size_t TemplateClippedRows { 0 };
		int TemplateFallbacks { 0 };
	};

	struct OverlayGeometryInput
	{
		bool ValidCell { false };
		int FlatX { 0 };
		int FlatY { 0 };
		int HeightX { 0 };
		int HeightY { 0 };
		bool FlatOnScreen { false };
		bool HeightOnScreen { false };
		bool operator==(const OverlayGeometryInput&) const = default;
	};

	struct OverlayVisibilityInput
	{
		OverlayCellKind Kind { OverlayCellKind::Unknown };
		int MainAlpha { 0 };
		int EdgeAlpha { 0 };
		bool operator==(const OverlayVisibilityInput&) const = default;
	};

	struct OverlayHouseInput
	{
		HouseClass* House { nullptr };
		unsigned int StateVersion { 0 };
		bool FullMapVisible { false };
		bool operator==(const OverlayHouseInput&) const = default;
	};

	struct OverlayRegionCacheEntry
	{
		bool Valid { false };
		OverlayRegionCacheKey Key {};
		std::vector<OverlaySpan> FinalRegionUnionSpans {};
		std::vector<OverlayDrawRect> MainRects {};
		std::vector<OverlayDrawRect> EdgeRects {};
		size_t EdgeUnionSpanCount { 0 };
		std::vector<OverlayGeometryInput> GeometryInputs {};
		std::vector<OverlayVisibilityInput> VisibilityInputs {};
		std::vector<OverlayHouseInput> HouseInputs {};
		int NextAlphaChangeFrame { 0 };
		int LastValidatedFrame { -1 };
		OverlayRegionCacheStats Stats {};
		int TemporalCacheFirstInvalidFrame { 0 };
	};

	struct OverlayRegionCacheDecision
	{
		bool CacheEnabled { false };
		bool CacheAllowed { false };
		const char* DisabledReason { "StageDisabled" };
		bool TemporalVisibilityActive { false };
		int TemporalFullMapFirstInvalidFrame { 0 };
		int TemporalCacheFirstInvalidFrame { 0 };
		OverlayRegionCacheKey Key {};
	};

	static std::vector<OverlayGeometryInput> OverlayScratchGeometryInputs {};
	static std::vector<OverlayVisibilityInput> OverlayScratchVisibilityInputs {};
	static std::vector<OverlayHouseInput> OverlayScratchHouseInputs {};
	static const std::vector<OverlayGeometryInput>* ActiveOverlayGeometryInputs = nullptr;
	static int ActiveOverlayGeometryMinX = 0;
	static int ActiveOverlayGeometryMinY = 0;
	static int ActiveOverlayGeometryWidth = 0;
	static int ActiveOverlayGeometryHeight = 0;
	static int OverlayFrameGeometryProbes = 0;
	static int OverlayFrameVisibilityQueries = 0;
	static OverlayRegionCacheEntry OverlayFinalRegionCache {};
	static unsigned long long OverlayRegionCacheTotalHits = 0;
	static unsigned long long OverlayRegionCacheTotalMisses = 0;
	static unsigned long long OverlayRegionCacheTotalRebuilds = 0;
	static int OverlayRegionCacheHitStreak = 0;
	// Scenario teardown must invalidate screen geometry even if the next game's
	// house address, visibility version and viewport happen to reuse the same key.
	void ResetCache()
	{
		OverlayFinalRegionCache = {};
		OverlayRegionCacheHitStreak = 0;
	}

	static std::vector<OverlaySpan> OverlayScratchEdgeSpans {};
	static std::vector<OverlaySpan> OverlayScratchCliffSpans {};
	static std::vector<OverlaySpan> OverlayScratchVerticalFaceSpans {};
	static std::vector<OverlaySpan> OverlayScratchMainUnionSpans {};
	static std::vector<OverlaySpan> OverlayScratchCompactMainInputSpans {};
	static std::vector<OverlaySpan> OverlayScratchCompactMainUnionSpans {};
	static std::vector<OverlaySpan> OverlayScratchDilationSpans {};
	static std::vector<OverlaySpan> OverlayScratchFinalRegionSpans {};
	static std::vector<OverlaySpan> OverlayScratchFinalRegionUnionSpans {};
	static std::vector<OverlaySpan> OverlayScratchEdgeUnionSpans {};
	static std::vector<OverlaySpan> OverlayScratchCliffUnionSpans {};
	static std::vector<OverlayDrawRect> OverlayScratchDrawRects {};
	static std::vector<OverlaySpan> OverlayScratchRowBucketSpans {};
	static std::vector<size_t> OverlayScratchRowBucketCounts {};
	static std::vector<size_t> OverlayScratchRowBucketOffsets {};
	static std::vector<OverlaySpanEvent> OverlayScratchRowBucketEvents {};
	static OverlayCellShapeTemplate OverlayRectShapeTemplate {};
	static OverlayCellShapeTemplate OverlayDiamondShapeTemplate {};

	static int ClampAlpha(const int value)
	{
		if (value < 0)
		{
			return 0;
		}

		if (value > 255)
		{
			return 255;
		}

		return value;
	}

	static int AbsInt(const int value)
	{
		return value < 0 ? -value : value;
	}

	static const char* BoolText(const bool value)
	{
		return value ? "true" : "false";
	}

	static void HashCombine(unsigned int& seed, const unsigned int value)
	{
		seed ^= value + 0x9e3779b9U + (seed << 6) + (seed >> 2);
	}

	static void HashCombineInt(unsigned int& seed, const int value)
	{
		HashCombine(seed, static_cast<unsigned int>(value));
	}

	static bool IsEligibleViewerHouse(HouseClass* const pHouse)
	{
		return pHouse && !pHouse->Defeated && !pHouse->IsObserver() && pHouse->Type && !pHouse->Type->MultiplayPassive;
	}

	static bool IsEligibleViewerOrAllyHouse(HouseClass* const pViewerHouse, HouseClass* const pHouse)
	{
		return IsEligibleViewerHouse(pViewerHouse) && IsEligibleViewerHouse(pHouse)
			&& (pHouse == pViewerHouse || pViewerHouse->IsAlliedWith(pHouse));
	}

	static int GetTemporalCacheFirstInvalidFrame(const int untilFrame)
	{
		// First frame where a cache entry that depended on this full-map hold must miss and rebuild.
		return untilFrame >= INT_MAX ? INT_MAX : untilFrame + 1;
	}

	static int GetTemporalCacheExpiresInFrames(const int firstInvalidFrame, const int currentFrame)
	{
		return firstInvalidFrame > 0 ? std::max(0, firstInvalidFrame - currentFrame) : 0;
	}

	static void AccumulateTemporalVisibilityWindow(
		HouseExt::ExtData* const pHouseExt,
		const int currentFrame,
		bool& active,
		int& firstInvalidFrame)
	{
		if (!pHouseExt)
		{
			return;
		}

		const auto considerUntilFrame = [&active, &firstInvalidFrame, currentFrame](const int untilFrame)
		{
			if (untilFrame >= currentFrame && untilFrame > 0)
			{
				const int candidate = GetTemporalCacheFirstInvalidFrame(untilFrame);
				active = true;

				if (firstInvalidFrame <= 0 || candidate < firstInvalidFrame)
				{
					firstInvalidFrame = candidate;
				}
			}
		};

		// Do not scan cell-level LastVisibleFrames here. OverlayEffectiveVersion carries those
		// effective changes without adding a full vector scan to the overlay cache hot path.
		considerUntilFrame(pHouseExt->PhobosFog_FullMapVisibleUntilFrame);
	}

	static bool HasTemporalVisibilityActive(HouseExt::ExtData* const pHouseExt, const int currentFrame)
	{
		bool active = false;
		int firstInvalidFrame = 0;
		AccumulateTemporalVisibilityWindow(pHouseExt, currentFrame, active, firstInvalidFrame);
		return active;
	}

	static unsigned int BuildEffectiveVisibilityVersionHash(
		HouseClass* const pViewerHouse,
		bool& temporalVisibilityActive)
	{
		temporalVisibilityActive = false;

		unsigned int hash = 2166136261U;

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerOrAllyHouse(pViewerHouse, pHouse))
			{
				continue;
			}

			const auto pHouseExt = HouseExt::ExtMap.TryFind(pHouse);

			HashCombine(hash, static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(pHouse)));
			HashCombineInt(hash, pHouse ? pHouse->ArrayIndex : -1);
			HashCombine(hash, pHouseExt ? pHouseExt->PhobosFog_StateVersion : 0U);

			if (HasTemporalVisibilityActive(pHouseExt, Unsorted::CurrentFrame))
			{
				temporalVisibilityActive = true;
			}
		}

		return hash;
	}

	static unsigned int BuildOverlayEffectiveVisibilityVersionHash(HouseClass* const pViewerHouse)
	{
		unsigned int hash = 2166136261U;

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerOrAllyHouse(pViewerHouse, pHouse))
			{
				continue;
			}

			const auto pHouseExt = HouseExt::ExtMap.TryFind(pHouse);

			HashCombine(hash, static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(pHouse)));
			HashCombineInt(hash, pHouse ? pHouse->ArrayIndex : -1);
			HashCombine(hash, pHouseExt ? pHouseExt->PhobosFog_OverlayEffectiveVersion : 0U);
		}

		return hash;
	}

	static void AccumulateTemporalVisibilityWindowForViewerAndAllies(
		HouseClass* const pViewerHouse,
		const int currentFrame,
		bool& active,
		int& firstInvalidFrame)
	{
		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerOrAllyHouse(pViewerHouse, pHouse))
			{
				continue;
			}

			AccumulateTemporalVisibilityWindow(HouseExt::ExtMap.TryFind(pHouse), currentFrame, active, firstInvalidFrame);
		}
	}

	static bool OverlayRegionCacheKeysEqual(const OverlayRegionCacheKey& lhs, const OverlayRegionCacheKey& rhs)
	{
		return lhs.ViewerHouse == rhs.ViewerHouse
			&& lhs.OverlayViewportHash == rhs.OverlayViewportHash
			&& lhs.OverlayConfigHash == rhs.OverlayConfigHash;
	}

	static void CollectStateVersionTouchCounters(
		HouseClass* const pViewerHouse,
		unsigned long long& touchCount,
		unsigned long long (&touchReasons)[HouseExt::PhobosFogStateTouchReasonCount])
	{
		touchCount = 0;

		for (auto& reasonCount : touchReasons)
		{
			reasonCount = 0;
		}

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerOrAllyHouse(pViewerHouse, pHouse))
			{
				continue;
			}

			const auto pHouseExt = HouseExt::ExtMap.TryFind(pHouse);

			if (!pHouseExt)
			{
				continue;
			}

			touchCount += pHouseExt->PhobosFog_StateVersionTouchCount;

			for (size_t i = 0; i < HouseExt::PhobosFogStateTouchReasonCount; ++i)
			{
				touchReasons[i] += pHouseExt->PhobosFog_StateVersionTouchReasons[i];
			}
		}
	}

	static void CollectOverlayEffectiveDiagnostics(
		HouseClass* const pViewerHouse,
		unsigned long long& touchCount,
		size_t& batchTouchedCells,
		size_t& batchChangedCells)
	{
		touchCount = 0;
		batchTouchedCells = 0;
		batchChangedCells = 0;

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerOrAllyHouse(pViewerHouse, pHouse))
			{
				continue;
			}

			const auto pHouseExt = HouseExt::ExtMap.TryFind(pHouse);

			if (!pHouseExt)
			{
				continue;
			}

			touchCount += pHouseExt->PhobosFog_OverlayEffectiveTouchCount;
			batchTouchedCells += pHouseExt->PhobosFog_LastOverlayEffectiveBatchTouchedCells;
			batchChangedCells += pHouseExt->PhobosFog_LastOverlayEffectiveBatchChangedCells;
		}
	}

	static unsigned int BuildOverlayViewportHash(
		const RectangleStruct& bounds,
		const CellStruct& firstCell,
		const int minX,
		const int maxX,
		const int minY,
		const int maxY,
		const Point2D& projectionSentinel)
	{
		unsigned int hash = 2166136261U;

		HashCombineInt(hash, bounds.X);
		HashCombineInt(hash, bounds.Y);
		HashCombineInt(hash, bounds.Width);
		HashCombineInt(hash, bounds.Height);
		HashCombineInt(hash, firstCell.X);
		HashCombineInt(hash, firstCell.Y);
		HashCombineInt(hash, minX);
		HashCombineInt(hash, maxX);
		HashCombineInt(hash, minY);
		HashCombineInt(hash, maxY);
		HashCombineInt(hash, projectionSentinel.X);
		HashCombineInt(hash, projectionSentinel.Y);

		return hash;
	}

	static unsigned int BuildOverlayConfigHash(
		const int alpha,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY,
		const int viewportCellPadding,
		const bool softEdge,
		const int softEdgeVisibleAlpha,
		const int fadeInFrames,
		const int softEdgePadding,
		const int alphaVariance,
		const int frontierMode,
		const int overlayShape,
		const int maxDrawRects,
		const bool heightAware,
		const int heightYOffset,
		const bool legacyCliffCoverBypassed,
		const bool useRegionDilationPrototype,
		const bool useHeightDiscontinuityFacePrototype,
		const int verticalFaceMinDropPixels,
		const int verticalFaceMaxDropPixels,
		const int verticalFaceFallbackDilationY,
		const int regionDilationY,
		const int regionCloseGapPixels,
		const int regionDilationAlphaNumerator,
		const int regionDilationAlphaDenominator)
	{
		unsigned int hash = 2166136261U;

		HashCombineInt(hash, alpha);
		HashCombineInt(hash, cellOverlayWidth);
		HashCombineInt(hash, cellOverlayHeight);
		HashCombineInt(hash, paddingX);
		HashCombineInt(hash, paddingY);
		HashCombineInt(hash, viewportCellPadding);
		HashCombineInt(hash, softEdge ? 1 : 0);
		HashCombineInt(hash, softEdgeVisibleAlpha);
		HashCombineInt(hash, fadeInFrames);
		HashCombineInt(hash, softEdgePadding);
		HashCombineInt(hash, alphaVariance);
		HashCombineInt(hash, frontierMode);
		HashCombineInt(hash, overlayShape);
		HashCombineInt(hash, maxDrawRects);
		HashCombineInt(hash, heightAware ? 1 : 0);
		HashCombineInt(hash, heightYOffset);
		HashCombineInt(hash, legacyCliffCoverBypassed ? 1 : 0);
		HashCombineInt(hash, useRegionDilationPrototype ? 1 : 0);
		HashCombineInt(hash, useHeightDiscontinuityFacePrototype ? 1 : 0);
		HashCombineInt(hash, verticalFaceMinDropPixels);
		HashCombineInt(hash, verticalFaceMaxDropPixels);
		HashCombineInt(hash, verticalFaceFallbackDilationY);
		HashCombineInt(hash, regionDilationY);
		HashCombineInt(hash, regionCloseGapPixels);
		HashCombineInt(hash, regionDilationAlphaNumerator);
		HashCombineInt(hash, regionDilationAlphaDenominator);

		return hash;
	}

	static OverlayRegionCacheDecision BuildOverlayRegionCacheDecision(
		HouseClass* const pViewerHouse,
		const bool cachePrototypeEnabled,
		const RectangleStruct& bounds,
		const CellStruct& firstCell,
		const int minX,
		const int maxX,
		const int minY,
		const int maxY,
		const Point2D& projectionSentinel,
		const int alpha,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY,
		const int viewportCellPadding,
		const bool softEdge,
		const int softEdgeVisibleAlpha,
		const int fadeInFrames,
		const int softEdgePadding,
		const int alphaVariance,
		const int frontierMode,
		const int overlayShape,
		const int maxDrawRects,
		const bool heightAware,
		const int heightYOffset,
		const bool legacyCliffCoverBypassed,
		const bool useRegionDilationPrototype,
		const bool useHeightDiscontinuityFacePrototype,
		const int verticalFaceMinDropPixels,
		const int verticalFaceMaxDropPixels,
		const int verticalFaceFallbackDilationY,
		const int regionDilationY,
		const int regionCloseGapPixels,
		const int regionDilationAlphaNumerator,
		const int regionDilationAlphaDenominator)
	{
		OverlayRegionCacheDecision result {};
		result.CacheEnabled = cachePrototypeEnabled;
		result.Key.ViewerHouse = pViewerHouse;
		result.Key.OverlayEffectiveVisibilityVersionHash = BuildOverlayEffectiveVisibilityVersionHash(pViewerHouse);
		result.Key.OverlayViewportHash = BuildOverlayViewportHash(bounds, firstCell, minX, maxX, minY, maxY, projectionSentinel);
		result.Key.OverlayConfigHash = BuildOverlayConfigHash(
			alpha,
			cellOverlayWidth,
			cellOverlayHeight,
			paddingX,
			paddingY,
			viewportCellPadding,
			softEdge,
			softEdgeVisibleAlpha,
			fadeInFrames,
			softEdgePadding,
			alphaVariance,
			frontierMode,
			overlayShape,
			maxDrawRects,
			heightAware,
			heightYOffset,
			legacyCliffCoverBypassed,
			useRegionDilationPrototype,
			useHeightDiscontinuityFacePrototype,
			verticalFaceMinDropPixels,
			verticalFaceMaxDropPixels,
			verticalFaceFallbackDilationY,
			regionDilationY,
			regionCloseGapPixels,
			regionDilationAlphaNumerator,
			regionDilationAlphaDenominator);

		if (!cachePrototypeEnabled)
		{
			result.DisabledReason = "StageDisabled";
			return result;
		}

		if (!pViewerHouse)
		{
			result.DisabledReason = "InvalidViewer";
			return result;
		}

		AccumulateTemporalVisibilityWindowForViewerAndAllies(
			pViewerHouse,
			Unsorted::CurrentFrame,
			result.TemporalVisibilityActive,
			result.TemporalCacheFirstInvalidFrame);
		result.TemporalFullMapFirstInvalidFrame = result.TemporalCacheFirstInvalidFrame;

		result.CacheAllowed = true;
		result.DisabledReason = "None";
		return result;
	}

	static OverlayCacheStageADiagnostics BuildOverlayCacheStageADiagnostics(
		HouseClass* const pViewerHouse,
		const RectangleStruct& bounds,
		const CellStruct& firstCell,
		const int minX,
		const int maxX,
		const int minY,
		const int maxY,
		const Point2D& projectionSentinel,
		const int alpha,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY,
		const int viewportCellPadding,
		const bool softEdge,
		const int softEdgeVisibleAlpha,
		const int fadeInFrames,
		const int softEdgePadding,
		const int alphaVariance,
		const int frontierMode,
		const int overlayShape,
		const int maxDrawRects,
		const bool heightAware,
		const int heightYOffset,
		const bool legacyCliffCoverBypassed,
		const bool useRegionDilationPrototype,
		const bool useHeightDiscontinuityFacePrototype,
		const int verticalFaceMinDropPixels,
		const int verticalFaceMaxDropPixels,
		const int verticalFaceFallbackDilationY,
		const int regionDilationY,
		const int regionCloseGapPixels,
		const int regionDilationAlphaNumerator,
		const int regionDilationAlphaDenominator)
	{
		OverlayCacheStageADiagnostics result {};
		const auto pViewerExt = HouseExt::ExtMap.TryFind(pViewerHouse);

		result.PhobosFogStateVersion = pViewerExt ? pViewerExt->PhobosFog_StateVersion : 0U;
		result.OverlayEffectiveVersion = pViewerExt ? pViewerExt->PhobosFog_OverlayEffectiveVersion : 0U;
		result.EffectiveVisibilityVersionHash = BuildEffectiveVisibilityVersionHash(pViewerHouse, result.TemporalVisibilityActive);
		AccumulateTemporalVisibilityWindowForViewerAndAllies(
			pViewerHouse,
			Unsorted::CurrentFrame,
			result.TemporalVisibilityActive,
			result.TemporalCacheFirstInvalidFrame);
		result.TemporalFullMapFirstInvalidFrame = result.TemporalCacheFirstInvalidFrame;
		result.OverlayEffectiveVisibilityVersionHash = BuildOverlayEffectiveVisibilityVersionHash(pViewerHouse);
		CollectStateVersionTouchCounters(pViewerHouse, result.StateVersionTouchCount, result.StateVersionTouchReasons);
		CollectOverlayEffectiveDiagnostics(
			pViewerHouse,
			result.OverlayEffectiveTouchCount,
			result.OverlayEffectiveBatchTouchedCells,
			result.OverlayEffectiveBatchChangedCells);
		result.OverlayViewportHash = BuildOverlayViewportHash(bounds, firstCell, minX, maxX, minY, maxY, projectionSentinel);
		result.OverlayConfigHash = BuildOverlayConfigHash(
			alpha,
			cellOverlayWidth,
			cellOverlayHeight,
			paddingX,
			paddingY,
			viewportCellPadding,
			softEdge,
			softEdgeVisibleAlpha,
			fadeInFrames,
			softEdgePadding,
			alphaVariance,
			frontierMode,
			overlayShape,
			maxDrawRects,
			heightAware,
			heightYOffset,
			legacyCliffCoverBypassed,
			useRegionDilationPrototype,
			useHeightDiscontinuityFacePrototype,
			verticalFaceMinDropPixels,
			verticalFaceMaxDropPixels,
			verticalFaceFallbackDilationY,
			regionDilationY,
			regionCloseGapPixels,
			regionDilationAlphaNumerator,
			regionDilationAlphaDenominator);

		result.CacheAllowed = true;
		result.DisabledReason = "None";

		return result;
	}

	static bool TryGetCellStateForHouse(HouseClass* const pHouse, const int cellIndex, HouseExt::PhobosFogCellState& state)
	{
		++OverlayFrameVisibilityQueries;
		return HouseExt::ExtData::TryGetEffectivePhobosFogCellStateForHouse(pHouse, cellIndex, state);
	}

	static bool TryGetLastVisibleFrameForHouse(HouseClass* const pHouse, const int cellIndex, int& frame)
	{
		++OverlayFrameVisibilityQueries;
		const auto pHouseExt = HouseExt::ExtMap.TryFind(pHouse);

		if (!pHouseExt || pHouseExt->PhobosFog_LastVisibleFrames.size() != MapClass::MaxCells)
		{
			return false;
		}

		frame = pHouseExt->PhobosFog_LastVisibleFrames[static_cast<size_t>(cellIndex)];
		return true;
	}

	static OverlayCellKind GetOverlayCellKindForViewerAndAllies(HouseClass* const pViewerHouse, const int cellIndex)
	{
		bool explored = false;
		HouseExt::PhobosFogCellState state = HouseExt::PhobosFogCellState::Unknown;

		if (TryGetCellStateForHouse(pViewerHouse, cellIndex, state))
		{
			if (state == HouseExt::PhobosFogCellState::Visible)
			{
				return OverlayCellKind::Visible;
			}

			explored = state == HouseExt::PhobosFogCellState::Explored;
		}

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerHouse(pHouse) || pHouse == pViewerHouse || !pViewerHouse->IsAlliedWith(pHouse))
			{
				continue;
			}

			if (!TryGetCellStateForHouse(pHouse, cellIndex, state))
			{
				continue;
			}

			if (state == HouseExt::PhobosFogCellState::Visible)
			{
				return OverlayCellKind::Visible;
			}

			explored |= state == HouseExt::PhobosFogCellState::Explored;
		}

		return explored ? OverlayCellKind::ExploredOverlay : OverlayCellKind::Unknown;
	}

	static int GetLastVisibleFrameForViewerAndAllies(HouseClass* const pViewerHouse, const int cellIndex)
	{
		int result = -1;
		int frame = -1;

		if (TryGetLastVisibleFrameForHouse(pViewerHouse, cellIndex, frame) && frame > result)
		{
			result = frame;
		}

		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerHouse(pHouse) || pHouse == pViewerHouse || !pViewerHouse->IsAlliedWith(pHouse))
			{
				continue;
			}

			if (TryGetLastVisibleFrameForHouse(pHouse, cellIndex, frame) && frame > result)
			{
				result = frame;
			}
		}

		return result;
	}

	static bool TryGetExploredOverlayCellIndex(const CellStruct& cell, int& cellIndex)
	{
		if (!MapClass::Instance.TryGetCellAt(cell))
		{
			return false;
		}

		cellIndex = MapClass::GetCellIndex(cell);
		return cellIndex >= 0 && cellIndex < MapClass::MaxCells;
	}

	static bool IsExploredOverlayCell(HouseClass* const pViewerHouse, const CellStruct& cell, int& cellIndex)
	{
		if (!TryGetExploredOverlayCellIndex(cell, cellIndex))
		{
			return false;
		}

		return GetOverlayCellKindForViewerAndAllies(pViewerHouse, cellIndex) == OverlayCellKind::ExploredOverlay;
	}

	static OverlayCellKind GetOverlayCellKindForCell(HouseClass* const pViewerHouse, const CellStruct& cell)
	{
		int cellIndex = -1;

		if (!TryGetExploredOverlayCellIndex(cell, cellIndex))
		{
			return OverlayCellKind::Unknown;
		}

		return GetOverlayCellKindForViewerAndAllies(pViewerHouse, cellIndex);
	}

	static std::pair<Point2D, bool> ProjectOverlayCellClient(const CellStruct& cell, const bool heightAware, const int heightYOffset)
	{
		++OverlayFrameGeometryProbes;
		CoordStruct coords = CellClass::Cell2Coord(cell);

		if (heightAware)
		{
			if (const auto pCell = MapClass::Instance.TryGetCellAt(cell))
			{
				coords = pCell->GetCellCoords();
			}
		}

		auto result = TacticalClass::Instance->CoordsToClient(coords);
		result.first.Y += heightYOffset;
		return result;
	}

	static std::pair<Point2D, bool> GetOverlayCellClient(const CellStruct& cell, const bool heightAware, const int heightYOffset)
	{
		const int x = cell.X - ActiveOverlayGeometryMinX;
		const int y = cell.Y - ActiveOverlayGeometryMinY;
		if (ActiveOverlayGeometryInputs && x >= 0 && y >= 0
			&& x < ActiveOverlayGeometryWidth && y < ActiveOverlayGeometryHeight)
		{
			const auto& input = (*ActiveOverlayGeometryInputs)[static_cast<size_t>(y * ActiveOverlayGeometryWidth + x)];
			return heightAware
				? std::pair<Point2D, bool> { { input.HeightX, input.HeightY }, input.HeightOnScreen }
				: std::pair<Point2D, bool> { { input.FlatX, input.FlatY }, input.FlatOnScreen };
		}
		return ProjectOverlayCellClient(cell, heightAware, heightYOffset);
	}

	static FrontierInfo ClassifyExploredOverlayFrontier(HouseClass* const pViewerHouse, const CellStruct& cell, const int frontierMode)
	{
		FrontierInfo result {};
		int cellIndex = -1;

		if (!IsExploredOverlayCell(pViewerHouse, cell, cellIndex))
		{
			return result;
		}

		constexpr CellStruct NeighborOffsets[] =
		{
			{ 0, -1 },
			{ 0, 1 },
			{ -1, 0 },
			{ 1, 0 },
			{ -1, -1 },
			{ 1, -1 },
			{ -1, 1 },
			{ 1, 1 }
		};

		const int neighborCount = frontierMode <= 4 ? 4 : 8;

		for (int i = 0; i < neighborCount; ++i)
		{
			const CellStruct neighbor
			{
				static_cast<short>(cell.X + NeighborOffsets[i].X),
				static_cast<short>(cell.Y + NeighborOffsets[i].Y)
			};

			const auto neighborKind = GetOverlayCellKindForCell(pViewerHouse, neighbor);

			if (neighborKind == OverlayCellKind::Visible)
			{
				result.HasVisibleNeighbor = true;

				if (i < 4)
				{
					result.HasVisibleNeighbor4[i] = true;
				}
			}

			if (neighborKind != OverlayCellKind::ExploredOverlay)
			{
				result.IsFrontier = true;
			}
		}

		return result;
	}

	static int ComputeOverlayFadeAlpha(const int baseAlpha, const int currentFrame, const int lastVisibleFrame, const int fadeInFrames)
	{
		const int alpha = ClampAlpha(baseAlpha);
		if (alpha <= 0 || fadeInFrames <= 0 || lastVisibleFrame < 0)
			return alpha;

		const auto elapsedFrames = static_cast<long long>(currentFrame) - lastVisibleFrame;
		if (elapsedFrames < 0 || elapsedFrames >= fadeInFrames)
			return alpha;

		return ClampAlpha(static_cast<int>(alpha * elapsedFrames / fadeInFrames));
	}

	static int NextOverlayAlphaChangeFrame(const int currentFrame, const int lastVisibleFrame, const int fadeInFrames)
	{
		if (fadeInFrames <= 0 || lastVisibleFrame < 0)
			return 0;
		if (lastVisibleFrame > currentFrame)
			return lastVisibleFrame;
		if (static_cast<long long>(currentFrame) - lastVisibleFrame < fadeInFrames)
			return currentFrame < INT_MAX ? currentFrame + 1 : INT_MAX;
		return 0;
	}

	static int ApplyFadeInAlpha(const int baseAlpha, HouseClass* const pViewerHouse, const int cellIndex, const int fadeInFrames)
	{
		const int alpha = ClampAlpha(baseAlpha);
		if (alpha <= 0 || fadeInFrames <= 0)
			return alpha;
		return ComputeOverlayFadeAlpha(alpha, Unsorted::CurrentFrame,
			GetLastVisibleFrameForViewerAndAllies(pViewerHouse, cellIndex), fadeInFrames);
	}

	static int StableCellNoise(const int cellIndex)
	{
		unsigned int value = static_cast<unsigned int>(cellIndex);
		value ^= value >> 16;
		value *= 0x7feb352dU;
		value ^= value >> 15;
		value *= 0x846ca68bU;
		value ^= value >> 16;
		return static_cast<int>(value & 0x7fffffffU);
	}

	static int ComputeCellAlpha(const int baseAlpha, const int cellIndex, const int variance)
	{
		if (variance <= 0)
		{
			return ClampAlpha(baseAlpha);
		}

		const int range = variance * 2 + 1;
		const int offset = StableCellNoise(cellIndex) % range - variance;
		return ClampAlpha(baseAlpha + offset);
	}

	static void BuildOverlayHouseInputs(HouseClass* const pViewerHouse, std::vector<OverlayHouseInput>& inputs)
	{
		inputs.clear();
		for (auto const pHouse : HouseClass::Array)
		{
			if (!IsEligibleViewerOrAllyHouse(pViewerHouse, pHouse))
				continue;
			const auto pExt = HouseExt::ExtMap.TryFind(pHouse);
			inputs.push_back({ pHouse, pExt ? pExt->PhobosFog_StateVersion : 0U,
				pExt && pExt->IsPhobosFogFullMapHardVisible() });
		}
	}

	static void BuildOverlayGeometryInputs(const int minX, const int maxX, const int minY, const int maxY,
		const int heightYOffset, std::vector<OverlayGeometryInput>& inputs)
	{
		inputs.clear();
		// One-cell halo covers frontier diagonals, soft edges and active height faces.
		for (int y = minY - 1; y <= maxY + 1; ++y)
		{
			for (int x = minX - 1; x <= maxX + 1; ++x)
			{
				const CellStruct cell { static_cast<short>(x), static_cast<short>(y) };
				const auto flat = ProjectOverlayCellClient(cell, false, heightYOffset);
				const auto height = ProjectOverlayCellClient(cell, true, heightYOffset);
				inputs.push_back({ MapClass::Instance.TryGetCellAt(cell) != nullptr,
					flat.first.X, flat.first.Y, height.first.X, height.first.Y, flat.second, height.second });
			}
		}
	}

	static void BuildOverlayVisibilityInputs(HouseClass* const pViewerHouse,
		const int minX, const int maxX, const int minY, const int maxY,
		const int alpha, const int alphaVariance, const int edgeAlpha, const int fadeInFrames,
		std::vector<OverlayVisibilityInput>& inputs, int& nextAlphaChangeFrame)
	{
		inputs.clear();
		nextAlphaChangeFrame = 0;
		for (int y = minY - 1; y <= maxY + 1; ++y)
		{
			for (int x = minX - 1; x <= maxX + 1; ++x)
			{
				const CellStruct cell { static_cast<short>(x), static_cast<short>(y) };
				int cellIndex = -1;
				OverlayVisibilityInput input {};
				if (TryGetExploredOverlayCellIndex(cell, cellIndex))
					input.Kind = GetOverlayCellKindForViewerAndAllies(pViewerHouse, cellIndex);
				if (input.Kind == OverlayCellKind::ExploredOverlay && x >= minX && x <= maxX && y >= minY && y <= maxY)
				{
					const int lastVisibleFrame = fadeInFrames > 0 ? GetLastVisibleFrameForViewerAndAllies(pViewerHouse, cellIndex) : -1;
					input.MainAlpha = ComputeOverlayFadeAlpha(ComputeCellAlpha(alpha, cellIndex, alphaVariance), Unsorted::CurrentFrame, lastVisibleFrame, fadeInFrames);
					input.EdgeAlpha = ComputeOverlayFadeAlpha(edgeAlpha, Unsorted::CurrentFrame, lastVisibleFrame, fadeInFrames);
					// Include zero-alpha sources and future timestamps: both may change later.
					const int next = NextOverlayAlphaChangeFrame(Unsorted::CurrentFrame, lastVisibleFrame, fadeInFrames);
					if (next > 0 && (nextAlphaChangeFrame == 0 || next < nextAlphaChangeFrame))
						nextAlphaChangeFrame = next;
				}
				inputs.push_back(input);
			}
		}
	}

	static bool CanReuseOverlayVisibilityInputs(const OverlayRegionCacheEntry& cache,
		const std::vector<OverlayHouseInput>& houses, const int currentFrame)
	{
		return cache.HouseInputs == houses && currentFrame >= cache.LastValidatedFrame
			&& (cache.NextAlphaChangeFrame == 0 || currentFrame < cache.NextAlphaChangeFrame)
			&& (cache.TemporalCacheFirstInvalidFrame == 0 || currentFrame < cache.TemporalCacheFirstInvalidFrame);
	}

	static bool TryClipRectToBounds(const RectangleStruct& rect, const RectangleStruct& bounds, RectangleStruct& clipped)
	{
		const int left = rect.X > bounds.X ? rect.X : bounds.X;
		const int top = rect.Y > bounds.Y ? rect.Y : bounds.Y;
		const int right = rect.X + rect.Width < bounds.X + bounds.Width ? rect.X + rect.Width : bounds.X + bounds.Width;
		const int bottom = rect.Y + rect.Height < bounds.Y + bounds.Height ? rect.Y + rect.Height : bounds.Y + bounds.Height;

		if (right <= left || bottom <= top)
		{
			return false;
		}

		clipped = RectangleStruct { left, top, right - left, bottom - top };
		return true;
	}

	static bool DrawClippedRectTrans(
		const RectangleStruct& rect,
		const RectangleStruct& bounds,
		ColorStruct& color,
		const int alpha,
		int& drawRectCount,
		const int maxDrawRects)
	{
		RectangleStruct clippedRect {};

		if (!TryClipRectToBounds(rect, bounds, clippedRect))
		{
			return true;
		}

		if (drawRectCount >= maxDrawRects)
		{
			return false;
		}

		DSurface::Composite->FillRectTrans(&clippedRect, &color, alpha);
		++drawRectCount;
		return true;
	}

	static bool TryGetDiamondSpan(const RectangleStruct& rect, const int y, int& x1, int& x2)
	{
		if (rect.Width <= 0 || rect.Height <= 0 || y < rect.Y || y >= rect.Y + rect.Height)
		{
			return false;
		}

		const int centerX = rect.X + rect.Width / 2;
		const int halfW = rect.Width / 2;
		const int halfH = rect.Height / 2;

		if (halfW <= 0 || halfH <= 0)
		{
			x1 = rect.X;
			x2 = rect.X + rect.Width;
			return x2 > x1;
		}

		const int localY = y - rect.Y;
		const int dy = AbsInt(localY - halfH);
		int rowHalfW = halfW * (halfH - dy) / halfH;

		if (rowHalfW <= 0)
		{
			rowHalfW = 1;
		}

		x1 = centerX - rowHalfW;
		x2 = centerX + rowHalfW;
		return x2 > x1;
	}

	static bool TryGetDiamondEdgePoints(
		const RectangleStruct& rect,
		const DiamondEdge edge,
		Point2D& a,
		Point2D& b)
	{
		if (rect.Width <= 0 || rect.Height <= 0)
		{
			return false;
		}

		const int centerX = rect.X + rect.Width / 2;
		const int centerY = rect.Y + rect.Height / 2;
		const Point2D top { centerX, rect.Y };
		const Point2D right { rect.X + rect.Width, centerY };
		const Point2D bottom { centerX, rect.Y + rect.Height };
		const Point2D left { rect.X, centerY };

		switch (edge)
		{
		case DiamondEdge::TopLeft:
			a = top;
			b = left;
			return true;
		case DiamondEdge::TopRight:
			a = top;
			b = right;
			return true;
		case DiamondEdge::BottomLeft:
			a = left;
			b = bottom;
			return true;
		case DiamondEdge::BottomRight:
			a = right;
			b = bottom;
			return true;
		default:
			return false;
		}
	}

	static bool TryBuildClippedSpan(
		const RectangleStruct& bounds,
		const int y,
		const int x1,
		const int x2,
		const int alpha,
		OverlaySpan& span)
	{
		const int safeAlpha = ClampAlpha(alpha);

		if (safeAlpha <= 0 || y < bounds.Y || y >= bounds.Y + bounds.Height || x2 <= x1)
		{
			return false;
		}

		const int clippedX1 = x1 > bounds.X ? x1 : bounds.X;
		const int clippedX2 = x2 < bounds.X + bounds.Width ? x2 : bounds.X + bounds.Width;

		if (clippedX2 <= clippedX1)
		{
			return false;
		}

		span = OverlaySpan { y, clippedX1, clippedX2, safeAlpha };
		return true;
	}

	static bool TryAddClippedSpan(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& bounds,
		const int y,
		const int x1,
		const int x2,
		const int alpha)
	{
		OverlaySpan span {};

		if (!TryBuildClippedSpan(bounds, y, x1, x2, alpha, span))
		{
			return false;
		}

		spans.push_back(span);
		return true;
	}

	static void AddClippedSpan(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& bounds,
		const int y,
		const int x1,
		const int x2,
		const int alpha)
	{
		TryAddClippedSpan(spans, bounds, y, x1, x2, alpha);
	}

	static void AddRectSpans(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& rect,
		const RectangleStruct& bounds,
		const int alpha)
	{
		if (rect.Width <= 0 || rect.Height <= 0)
		{
			return;
		}

		const int top = rect.Y > bounds.Y ? rect.Y : bounds.Y;
		const int bottom = rect.Y + rect.Height < bounds.Y + bounds.Height ? rect.Y + rect.Height : bounds.Y + bounds.Height;

		for (int y = top; y < bottom; ++y)
		{
			AddClippedSpan(spans, bounds, y, rect.X, rect.X + rect.Width, alpha);
		}
	}

	static void RebuildOverlayCellShapeTemplate(
		OverlayCellShapeTemplate& shapeTemplate,
		const int width,
		const int height,
		const bool diamond)
	{
		shapeTemplate.Valid = false;
		shapeTemplate.Diamond = diamond;
		shapeTemplate.Width = width;
		shapeTemplate.Height = height;
		shapeTemplate.Rows.clear();

		if (width <= 0 || height <= 0)
		{
			return;
		}

		shapeTemplate.Rows.reserve(static_cast<size_t>(height));

		if (!diamond)
		{
			for (int y = 0; y < height; ++y)
			{
				shapeTemplate.Rows.push_back(OverlayCellShapeTemplateRow { y, 0, width });
			}

			shapeTemplate.Valid = true;
			return;
		}

		const RectangleStruct rect { 0, 0, width, height };

		for (int y = 0; y < height; ++y)
		{
			int x1 = 0;
			int x2 = 0;

			if (TryGetDiamondSpan(rect, y, x1, x2))
			{
				shapeTemplate.Rows.push_back(OverlayCellShapeTemplateRow { y, x1, x2 });
			}
		}

		shapeTemplate.Valid = !shapeTemplate.Rows.empty();
	}

	static OverlayCellShapeTemplate& GetOverlayCellShapeTemplate(
		const int width,
		const int height,
		const bool diamond)
	{
		auto& shapeTemplate = diamond ? OverlayDiamondShapeTemplate : OverlayRectShapeTemplate;

		if (!shapeTemplate.Valid
			|| shapeTemplate.Width != width
			|| shapeTemplate.Height != height
			|| shapeTemplate.Diamond != diamond)
		{
			RebuildOverlayCellShapeTemplate(shapeTemplate, width, height, diamond);
		}

		return shapeTemplate;
	}

	static OverlayTemplateVisibleRange GetVisibleTemplateRowRange(
		const OverlayCellShapeTemplate& shapeTemplate,
		const int projectedTopY,
		const RectangleStruct& bounds)
	{
		OverlayTemplateVisibleRange range {};

		if (!shapeTemplate.Valid || shapeTemplate.Rows.empty() || bounds.Height <= 0)
		{
			return range;
		}

		const int visibleLocalYMin = bounds.Y - projectedTopY;
		const int visibleLocalYMax = bounds.Y + bounds.Height - 1 - projectedTopY;

		if (visibleLocalYMax < shapeTemplate.Rows.front().LocalY
			|| visibleLocalYMin > shapeTemplate.Rows.back().LocalY)
		{
			return range;
		}

		const auto beginIt = std::lower_bound(
			shapeTemplate.Rows.begin(),
			shapeTemplate.Rows.end(),
			visibleLocalYMin,
			[](const OverlayCellShapeTemplateRow& row, const int localY)
			{
				return row.LocalY < localY;
			});

		const auto endIt = std::upper_bound(
			beginIt,
			shapeTemplate.Rows.end(),
			visibleLocalYMax,
			[](const int localY, const OverlayCellShapeTemplateRow& row)
			{
				return localY < row.LocalY;
			});

		range.BeginIndex = static_cast<size_t>(beginIt - shapeTemplate.Rows.begin());
		range.EndIndex = static_cast<size_t>(endIt - shapeTemplate.Rows.begin());

		return range;
	}

	static bool TryBuildOverlayShapeTemplateSpan(
		const OverlayCellShapeTemplateRow& row,
		const RectangleStruct& rect,
		const RectangleStruct& bounds,
		const int alpha,
		OverlaySpan& span)
	{
		const int screenX1 = rect.X + row.X1Offset;
		const int screenX2 = rect.X + row.X2Offset;

		if (screenX2 <= bounds.X || screenX1 >= bounds.X + bounds.Width)
		{
			return false;
		}

		return TryBuildClippedSpan(
			bounds,
			rect.Y + row.LocalY,
			screenX1,
			screenX2,
			alpha,
			span);
	}

	static void AddCliffFaceStripSpans(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& bounds,
		const int alpha,
		const CliffCoverEdge& edge)
	{
		const int safeAlpha = ClampAlpha(alpha);

		if (safeAlpha <= 0)
		{
			return;
		}

		std::array<Point2D, 4> points
		{
			edge.HighA,
			edge.HighB,
			edge.LowB,
			edge.LowA
		};

		int minY = points[0].Y;
		int maxY = points[0].Y;

		for (const auto& point : points)
		{
			if (point.Y < minY)
			{
				minY = point.Y;
			}

			if (point.Y > maxY)
			{
				maxY = point.Y;
			}
		}

		const int top = minY > bounds.Y ? minY : bounds.Y;
		const int bottom = maxY < bounds.Y + bounds.Height ? maxY : bounds.Y + bounds.Height;

		for (int y = top; y <= bottom; ++y)
		{
			std::array<int, 4> intersections {};
			int intersectionCount = 0;

			for (size_t i = 0; i < points.size(); ++i)
			{
				const auto& a = points[i];
				const auto& b = points[(i + 1) % points.size()];

				if (a.Y == b.Y)
				{
					continue;
				}

				const int minEdgeY = a.Y < b.Y ? a.Y : b.Y;
				const int maxEdgeY = a.Y > b.Y ? a.Y : b.Y;

				if (y < minEdgeY || y >= maxEdgeY)
				{
					continue;
				}

				if (intersectionCount >= static_cast<int>(intersections.size()))
				{
					break;
				}

				intersections[static_cast<size_t>(intersectionCount++)] = a.X + (y - a.Y) * (b.X - a.X) / (b.Y - a.Y);
			}

			if (intersectionCount < 2)
			{
				continue;
			}

			std::sort(intersections.begin(), intersections.begin() + intersectionCount);
			AddClippedSpan(spans, bounds, y, intersections[0] - 1, intersections[static_cast<size_t>(intersectionCount - 1)] + 2, safeAlpha);
		}
	}

	static void BuildCoalescedRects(
		const std::vector<OverlaySpan>& unionSpans,
		std::vector<OverlayDrawRect>& rects)
	{
		rects.clear();
		rects.reserve(unionSpans.size());
		for (const auto& span : unionSpans)
		{
			rects.push_back(OverlayDrawRect { span.X1, span.Y, span.X2 - span.X1, 1, span.Alpha });
		}

		std::sort(rects.begin(), rects.end(), [](const OverlayDrawRect& lhs, const OverlayDrawRect& rhs)
		{
			if (lhs.X != rhs.X)
			{
				return lhs.X < rhs.X;
			}

			if (lhs.Width != rhs.Width)
			{
				return lhs.Width < rhs.Width;
			}

			if (lhs.Alpha != rhs.Alpha)
			{
				return lhs.Alpha < rhs.Alpha;
			}

			return lhs.Y < rhs.Y;
		});

		size_t output = 0;
		for (size_t i = 0; i < rects.size();)
		{
			OverlayDrawRect merged = rects[i++];

			while (i < rects.size()
				&& rects[i].X == merged.X
				&& rects[i].Width == merged.Width
				&& rects[i].Alpha == merged.Alpha
				&& rects[i].Y == merged.Y + merged.Height)
			{
				++merged.Height;
				++i;
			}

			rects[output++] = merged;
		}
		rects.resize(output);
	}

	static bool DrawPreparedRects(
		const std::vector<OverlayDrawRect>& rects,
		const RectangleStruct& bounds,
		ColorStruct& color,
		int& drawRectCount,
		const int maxDrawRects)
	{
		for (const auto& rect : rects)
		{
			RectangleStruct drawRect { rect.X, rect.Y, rect.Width, rect.Height };
			if (!DrawClippedRectTrans(drawRect, bounds, color, rect.Alpha, drawRectCount, maxDrawRects))
			{
				return false;
			}
		}
		return true;
	}

	static void MergeSpansToUnionSpans(
		std::vector<OverlaySpan>& spans,
		std::vector<OverlaySpan>& unionSpans)
	{
		unionSpans.clear();

		if (spans.empty())
		{
			return;
		}

		std::sort(spans.begin(), spans.end(), [](const OverlaySpan& lhs, const OverlaySpan& rhs)
		{
			if (lhs.Y != rhs.Y)
			{
				return lhs.Y < rhs.Y;
			}

			if (lhs.X1 != rhs.X1)
			{
				return lhs.X1 < rhs.X1;
			}

			return lhs.X2 < rhs.X2;
		});

		std::vector<OverlaySpanEvent> events;
		events.reserve(spans.size() * 2);
		unionSpans.reserve(spans.size());

		for (size_t i = 0; i < spans.size();)
		{
			const int y = spans[i].Y;
			events.clear();

			do
			{
				events.push_back(OverlaySpanEvent { spans[i].X1, spans[i].Alpha, true });
				events.push_back(OverlaySpanEvent { spans[i].X2, spans[i].Alpha, false });
				++i;
			}
			while (i < spans.size() && spans[i].Y == y);

			std::sort(events.begin(), events.end(), [](const OverlaySpanEvent& lhs, const OverlaySpanEvent& rhs)
			{
				return lhs.X < rhs.X;
			});

			std::array<int, 256> activeCounts {};
			int activeAlpha = 0;
			int previousX = events.front().X;
			size_t eventIndex = 0;

			while (eventIndex < events.size())
			{
				const int x = events[eventIndex].X;

				if (activeAlpha > 0 && x > previousX)
				{
					unionSpans.push_back(OverlaySpan { y, previousX, x, activeAlpha });
				}

				do
				{
					const auto& event = events[eventIndex];

					if (event.Begin)
					{
						++activeCounts[static_cast<size_t>(event.Alpha)];
					}
					else if (activeCounts[static_cast<size_t>(event.Alpha)] > 0)
					{
						--activeCounts[static_cast<size_t>(event.Alpha)];
					}

					++eventIndex;
				}
				while (eventIndex < events.size() && events[eventIndex].X == x);

				activeAlpha = 0;

				for (int alpha = 255; alpha > 0; --alpha)
				{
					if (activeCounts[static_cast<size_t>(alpha)] > 0)
					{
						activeAlpha = alpha;
						break;
					}
				}

				previousX = x;
			}
		}
	}

	static void AppendRowBucketUnionSpans(
		const OverlaySpan* rowSpans,
		const size_t rowSpanCount,
		const int y,
		std::vector<OverlaySpan>& unionSpans)
	{
		if (!rowSpans || rowSpanCount == 0)
		{
			return;
		}

		auto& events = OverlayScratchRowBucketEvents;
		events.clear();
		events.reserve(std::max(events.capacity(), rowSpanCount * 2));

		for (size_t i = 0; i < rowSpanCount; ++i)
		{
			events.push_back(OverlaySpanEvent { rowSpans[i].X1, rowSpans[i].Alpha, true });
			events.push_back(OverlaySpanEvent { rowSpans[i].X2, rowSpans[i].Alpha, false });
		}

		std::sort(events.begin(), events.end(), [](const OverlaySpanEvent& lhs, const OverlaySpanEvent& rhs)
		{
			return lhs.X < rhs.X;
		});

		std::array<int, 256> activeCounts {};
		int activeAlpha = 0;
		int previousX = events.front().X;
		size_t eventIndex = 0;

		while (eventIndex < events.size())
		{
			const int x = events[eventIndex].X;

			if (activeAlpha > 0 && x > previousX)
			{
				unionSpans.push_back(OverlaySpan { y, previousX, x, activeAlpha });
			}

			do
			{
				const auto& event = events[eventIndex];

				if (event.Begin)
				{
					++activeCounts[static_cast<size_t>(event.Alpha)];
				}
				else if (activeCounts[static_cast<size_t>(event.Alpha)] > 0)
				{
					--activeCounts[static_cast<size_t>(event.Alpha)];
				}

				++eventIndex;
			}
			while (eventIndex < events.size() && events[eventIndex].X == x);

			activeAlpha = 0;

			for (int alpha = 255; alpha > 0; --alpha)
			{
				if (activeCounts[static_cast<size_t>(alpha)] > 0)
				{
					activeAlpha = alpha;
					break;
				}
			}

			previousX = x;
		}
	}

	static void BuildTemplateRowBucketMainUnionSpans(
		HouseClass* const pViewerHouse,
		const RectangleStruct& bounds,
		const int minX,
		const int maxX,
		const int minY,
		const int maxY,
		const bool heightAware,
		const int heightYOffset,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY,
		const int alpha,
		const int alphaVariance,
		const int fadeInFrames,
		const int frontierMode,
		const int overlayShape,
		OverlayCellShapeTemplateStats& stats,
		std::vector<OverlaySpan>& unionSpans,
		size_t& inputSpans,
		int& nonEmptyRows)
	{
		unionSpans.clear();
		inputSpans = 0;
		nonEmptyRows = 0;

		if (bounds.Width <= 0 || bounds.Height <= 0)
		{
			return;
		}

		const size_t rowCount = static_cast<size_t>(bounds.Height);
		auto& rowCounts = OverlayScratchRowBucketCounts;
		auto& rowOffsets = OverlayScratchRowBucketOffsets;
		auto& bucketSpans = OverlayScratchRowBucketSpans;

		rowCounts.assign(rowCount, 0);
		rowOffsets.assign(rowCount + 1, 0);

		// Active main fill writes clipped template rows directly into row buckets.
		// The old mainSpans vector is no longer materialized on the real path.
		auto visitTemplateSpans = [&](const bool writeSpans)
		{
			for (int y = minY; y <= maxY; ++y)
			{
				for (int x = minX; x <= maxX; ++x)
				{
					const CellStruct cell { static_cast<short>(x), static_cast<short>(y) };
					int cellIndex = -1;

					if (!IsExploredOverlayCell(pViewerHouse, cell, cellIndex))
					{
						continue;
					}

					const auto frontier = ClassifyExploredOverlayFrontier(pViewerHouse, cell, frontierMode);
					const auto [center, visible] = GetOverlayCellClient(cell, heightAware, heightYOffset);
					(void)visible;

					const RectangleStruct rect
					{
						center.X - cellOverlayWidth / 2 - paddingX,
						center.Y - cellOverlayHeight / 2 - paddingY,
						cellOverlayWidth,
						cellOverlayHeight
					};

					const bool useDiamond = overlayShape == 2 || (overlayShape == 1 && frontier.IsFrontier);
					const int currentAlpha = ApplyFadeInAlpha(ComputeCellAlpha(alpha, cellIndex, alphaVariance), pViewerHouse, cellIndex, fadeInFrames);

					if (currentAlpha <= 0)
					{
						continue;
					}

					auto& shapeTemplate = GetOverlayCellShapeTemplate(rect.Width, rect.Height, useDiamond);

					if (!shapeTemplate.Valid)
					{
						if (!writeSpans)
						{
							++stats.Fallbacks;
						}

						continue;
					}

					const auto visibleRange = GetVisibleTemplateRowRange(shapeTemplate, rect.Y, bounds);
					const size_t yVisibleRows = visibleRange.EndIndex - visibleRange.BeginIndex;

					if (!writeSpans)
					{
						stats.CandidateRows += shapeTemplate.Rows.size();
						stats.YRejectedRows += shapeTemplate.Rows.size() - yVisibleRows;
						stats.InstantiatedRows += yVisibleRows;
					}

					const int boundsRight = bounds.X + bounds.Width;

					for (size_t i = visibleRange.BeginIndex; i < visibleRange.EndIndex; ++i)
					{
						const auto& rowTemplate = shapeTemplate.Rows[i];
						const int screenX1 = rect.X + rowTemplate.X1Offset;
						const int screenX2 = rect.X + rowTemplate.X2Offset;

						if (screenX2 <= bounds.X || screenX1 >= boundsRight)
						{
							if (!writeSpans)
							{
								++stats.XRejectedRows;
							}

							continue;
						}

						if (!writeSpans)
						{
							++stats.VisibleRows;
						}

						OverlaySpan span {};

						if (!TryBuildOverlayShapeTemplateSpan(
							rowTemplate,
							rect,
							bounds,
							currentAlpha,
							span))
						{
							continue;
						}

						if (!writeSpans)
						{
							++stats.ClippedRows;
						}

						const int rowIndex = span.Y - bounds.Y;

						if (rowIndex < 0 || rowIndex >= bounds.Height)
						{
							continue;
						}

						const size_t row = static_cast<size_t>(rowIndex);

						if (!writeSpans)
						{
							++rowCounts[row];
							++inputSpans;
						}
						else
						{
							const size_t writeIndex = rowOffsets[row] + rowCounts[row]++;
							bucketSpans[writeIndex] = span;
						}
					}
				}
			}
		};

		visitTemplateSpans(false);

		if (inputSpans == 0)
		{
			return;
		}

		for (size_t row = 0; row < rowCount; ++row)
		{
			rowOffsets[row + 1] = rowOffsets[row] + rowCounts[row];

			if (rowCounts[row] > 0)
			{
				++nonEmptyRows;
			}

			rowCounts[row] = 0;
		}

		bucketSpans.resize(inputSpans);
		visitTemplateSpans(true);
		unionSpans.reserve(inputSpans);

		for (size_t row = 0; row < rowCount; ++row)
		{
			const size_t rowBegin = rowOffsets[row];
			const size_t rowEnd = rowOffsets[row + 1];

			if (rowEnd <= rowBegin)
			{
				continue;
			}

			AppendRowBucketUnionSpans(
				bucketSpans.data() + rowBegin,
				rowEnd - rowBegin,
				bounds.Y + static_cast<int>(row),
				unionSpans);
		}
	}

	static bool DrawUnionSpans(
		const std::vector<OverlaySpan>& unionSpans,
		const RectangleStruct& bounds,
		ColorStruct& color,
		int& drawRectCount,
		const int maxDrawRects)
	{
		BuildCoalescedRects(unionSpans, OverlayScratchDrawRects);
		return DrawPreparedRects(OverlayScratchDrawRects, bounds, color, drawRectCount, maxDrawRects);
	}

	static int ScaleOverlayAlpha(const int alpha, const int numerator, const int denominator)
	{
		if (denominator <= 0)
		{
			return ClampAlpha(alpha);
		}

		return ClampAlpha(alpha * numerator / denominator);
	}

	static void GenerateDownDilationSpans(
		const std::vector<OverlaySpan>& sourceUnionSpans,
		const RectangleStruct& bounds,
		const int dilationY,
		const int alphaNumerator,
		const int alphaDenominator,
		std::vector<OverlaySpan>& outSpans)
	{
		outSpans.clear();

		if (sourceUnionSpans.empty() || dilationY <= 0)
		{
			return;
		}

		outSpans.reserve(sourceUnionSpans.size() * static_cast<size_t>(dilationY));

		const int bottom = bounds.Y + bounds.Height;

		for (const auto& span : sourceUnionSpans)
		{
			if (span.X2 <= span.X1)
			{
				continue;
			}

			const int alpha = ScaleOverlayAlpha(span.Alpha, alphaNumerator, alphaDenominator);

			if (alpha <= 0)
			{
				continue;
			}

			for (int dy = 1; dy <= dilationY; ++dy)
			{
				const int y = span.Y + dy;

				if (y < bounds.Y || y >= bottom)
				{
					continue;
				}

				outSpans.push_back(OverlaySpan { y, span.X1, span.X2, alpha });
			}
		}
	}

	static void MergeRegionSpansWithCloseGap(
		std::vector<OverlaySpan>& spans,
		std::vector<OverlaySpan>& unionSpans,
		const int closeGapPixels,
		int& closedGaps)
	{
		unionSpans.clear();
		closedGaps = 0;

		if (spans.empty())
		{
			return;
		}

		std::vector<OverlaySpan> baseUnionSpans;
		MergeSpansToUnionSpans(spans, baseUnionSpans);

		if (baseUnionSpans.empty())
		{
			return;
		}

		if (closeGapPixels <= 0)
		{
			unionSpans.swap(baseUnionSpans);
			return;
		}

		unionSpans.reserve(baseUnionSpans.size());
		OverlaySpan current = baseUnionSpans.front();

		for (size_t i = 1; i < baseUnionSpans.size(); ++i)
		{
			const auto& next = baseUnionSpans[i];
			const int gap = next.X1 - current.X2;

			if (next.Y == current.Y && gap >= 0 && gap <= closeGapPixels)
			{
				current.X2 = next.X2 > current.X2 ? next.X2 : current.X2;
				current.Alpha = next.Alpha > current.Alpha ? next.Alpha : current.Alpha;
				++closedGaps;
				continue;
			}

			unionSpans.push_back(current);
			current = next;
		}

		unionSpans.push_back(current);
	}

	static void AddRectangularSoftEdgeSpans(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& rect,
		const RectangleStruct& bounds,
		const int alpha,
		const int padding,
		const int direction)
	{
		constexpr int MaxSoftEdgePadding = 16;
		const int safePadding = padding < 1 ? 0 : padding < MaxSoftEdgePadding ? padding : MaxSoftEdgePadding;
		const int safeAlpha = ClampAlpha(alpha);

		if (rect.Width <= 0 || rect.Height <= 0 || safePadding <= 0 || safeAlpha <= 0)
		{
			return;
		}

		for (int layer = 0; layer < safePadding; ++layer)
		{
			const int layerAlpha = ClampAlpha(safeAlpha * (safePadding - layer) / safePadding);

			if (layerAlpha <= 0)
			{
				continue;
			}

			if (direction == 0)
			{
				AddClippedSpan(spans, bounds, rect.Y + layer, rect.X, rect.X + rect.Width, layerAlpha);
			}

			if (direction == 1)
			{
				AddClippedSpan(spans, bounds, rect.Y + rect.Height - layer - 1, rect.X, rect.X + rect.Width, layerAlpha);
			}

			if (direction == 2)
			{
				AddRectSpans(spans, RectangleStruct { rect.X + layer, rect.Y, 1, rect.Height }, bounds, layerAlpha);
			}

			if (direction == 3)
			{
				AddRectSpans(spans, RectangleStruct { rect.X + rect.Width - layer - 1, rect.Y, 1, rect.Height }, bounds, layerAlpha);
			}
		}
	}

	static DiamondEdge GetDiamondEdgeTowardNeighbor(const Point2D& center, const Point2D& neighborCenter)
	{
		const int dx = neighborCenter.X - center.X;
		const int dy = neighborCenter.Y - center.Y;

		if (dx < 0)
		{
			return dy <= 0 ? DiamondEdge::TopLeft : DiamondEdge::BottomLeft;
		}

		if (dx > 0)
		{
			return dy < 0 ? DiamondEdge::TopRight : DiamondEdge::BottomRight;
		}

		return dy < 0 ? DiamondEdge::TopLeft : DiamondEdge::BottomRight;
	}

	static void NormalizeEdgeEndpointOrder(Point2D& a, Point2D& b)
	{
		if (a.X > b.X || (a.X == b.X && a.Y > b.Y))
		{
			std::swap(a, b);
		}
	}

	static bool TryAddHeightDiscontinuityFaceSpans(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& bounds,
		const int alpha,
		const CellStruct& cell,
		const CellStruct& offset,
		const int heightYOffset,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY,
		const int minDropPixels,
		const int maxDropPixels,
		HeightFaceStats& stats)
	{
		++stats.Candidates;

		if (minDropPixels <= 0 || maxDropPixels <= 0)
		{
			return false;
		}

		const CellStruct neighbor
		{
			static_cast<short>(cell.X + offset.X),
			static_cast<short>(cell.Y + offset.Y)
		};

		if (!MapClass::Instance.TryGetCellAt(cell) || !MapClass::Instance.TryGetCellAt(neighbor))
		{
			return false;
		}

		const auto [currentCenter, currentOnScreen] = GetOverlayCellClient(cell, true, heightYOffset);
		const auto [neighborCenter, neighborOnScreen] = GetOverlayCellClient(neighbor, true, heightYOffset);
		(void)currentOnScreen;
		(void)neighborOnScreen;

		const RectangleStruct currentRect
		{
			currentCenter.X - cellOverlayWidth / 2 - paddingX,
			currentCenter.Y - cellOverlayHeight / 2 - paddingY,
			cellOverlayWidth,
			cellOverlayHeight
		};
		const RectangleStruct neighborRect
		{
			neighborCenter.X - cellOverlayWidth / 2 - paddingX,
			neighborCenter.Y - cellOverlayHeight / 2 - paddingY,
			cellOverlayWidth,
			cellOverlayHeight
		};

		Point2D currentA {};
		Point2D currentB {};
		Point2D neighborA {};
		Point2D neighborB {};

		if (!TryGetDiamondEdgePoints(currentRect, GetDiamondEdgeTowardNeighbor(currentCenter, neighborCenter), currentA, currentB)
			|| !TryGetDiamondEdgePoints(neighborRect, GetDiamondEdgeTowardNeighbor(neighborCenter, currentCenter), neighborA, neighborB))
		{
			return false;
		}

		NormalizeEdgeEndpointOrder(currentA, currentB);
		NormalizeEdgeEndpointOrder(neighborA, neighborB);

		const int dropA = neighborA.Y - currentA.Y;
		const int dropB = neighborB.Y - currentB.Y;
		const int maxDrop = std::max(dropA, dropB);

		if (maxDrop < minDropPixels)
		{
			return false;
		}

		const int faceDropA = std::min(std::max(dropA, 0), maxDropPixels);
		const int faceDropB = std::min(std::max(dropB, 0), maxDropPixels);

		if (faceDropA <= 0 && faceDropB <= 0)
		{
			return false;
		}

		CliffCoverEdge face {};
		face.HighA = currentA;
		face.HighB = currentB;
		face.LowA = Point2D { currentA.X, currentA.Y + faceDropA };
		face.LowB = Point2D { currentB.X, currentB.Y + faceDropB };
		face.NeighborKind = OverlayCellKind::Unknown;

		AddCliffFaceStripSpans(spans, bounds, alpha, face);
		++stats.Accepted;

		const int acceptedMinDrop = faceDropA <= 0
			? faceDropB
			: faceDropB <= 0
				? faceDropA
				: std::min(faceDropA, faceDropB);
		const int acceptedMaxDrop = std::max(faceDropA, faceDropB);

		if (stats.MinDrop == 0 || acceptedMinDrop < stats.MinDrop)
		{
			stats.MinDrop = acceptedMinDrop;
		}

		if (acceptedMaxDrop > stats.MaxDrop)
		{
			stats.MaxDrop = acceptedMaxDrop;
		}

		return true;
	}

	static bool IsCliffProbeFrame()
	{
		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt
			|| !pRulesExt->PhobosFog_Enabled
			|| !pRulesExt->PhobosFog_Debug
			|| !pRulesExt->PhobosFog_DrawExploredOverlay
			|| !pRulesExt->PhobosFog_ExploredOverlayCliffCover
			|| Unsorted::CurrentFrame % 60 != 0)
		{
			return false;
		}

		return true;
	}

	static bool IsCliffProbeLogFrame()
	{
		const auto pRulesExt = RulesExt::Global();

		return pRulesExt
			&& pRulesExt->PhobosFog_Debug
			&& Unsorted::CurrentFrame % 60 == 0;
	}

	static bool IsRegionMaskLogFrame()
	{
		const auto pRulesExt = RulesExt::Global();

		return pRulesExt
			&& pRulesExt->PhobosFog_Debug
			&& pRulesExt->PhobosFog_DrawExploredOverlay
			&& Unsorted::CurrentFrame % 60 == 0;
	}

	static int ClampDebugCount(const size_t value)
	{
		constexpr size_t MaxDebugCount = 0x7FFFFFFF;
		return value > MaxDebugCount ? static_cast<int>(MaxDebugCount) : static_cast<int>(value);
	}

	static void LogRegionMaskSummary(
		const int sourceCells,
		const size_t rawMainSpans,
		const size_t mergedMainSpans,
		const bool dilationEnabled,
		const int dilationY,
		const size_t dilationRawSpans,
		const bool heightFaceEnabled,
		const int heightFaceDirections,
		const int heightFaceCandidates,
		const int heightFaceAccepted,
		const size_t heightFaceRawSpans,
		const int heightFaceMinDrop,
		const int heightFaceMaxDrop,
		const int fallbackDilationY,
		const size_t fallbackDilationRawSpans,
		const size_t finalMergedSpans,
		const int closedGaps,
		const int drawRects,
		const int maxDrawRects,
		const bool softEdgeEnabled,
		const bool legacyCliffCoverBypassed,
		const bool hitMaxDrawRects)
	{
		if (!IsRegionMaskLogFrame())
		{
			return;
		}

		Debug::Log(
			"[PhobosFog][RegionMask] Frame=%d SourceCells=%d RawMainSpans=%d MergedMainSpans=%d DilationEnabled=%s DilationY=%d DilationRawSpans=%d HeightFaceEnabled=%s HeightFaceDirections=%d HeightFaceCandidates=%d HeightFaceAccepted=%d HeightFaceRawSpans=%d HeightFaceMinDrop=%d HeightFaceMaxDrop=%d FallbackDilationY=%d FallbackDilationRawSpans=%d FinalMergedSpans=%d ClosedGaps=%d DrawRects=%d MaxDrawRects=%d SoftEdgeEnabled=%s LegacyCliffCoverBypassed=%s HitMaxDrawRects=%s\n",
			Unsorted::CurrentFrame,
			sourceCells,
			ClampDebugCount(rawMainSpans),
			ClampDebugCount(mergedMainSpans),
			BoolText(dilationEnabled),
			dilationY,
			ClampDebugCount(dilationRawSpans),
			BoolText(heightFaceEnabled),
			heightFaceDirections,
			heightFaceCandidates,
			heightFaceAccepted,
			ClampDebugCount(heightFaceRawSpans),
			heightFaceMinDrop,
			heightFaceMaxDrop,
			fallbackDilationY,
			ClampDebugCount(fallbackDilationRawSpans),
			ClampDebugCount(finalMergedSpans),
			closedGaps,
			drawRects,
			maxDrawRects,
			BoolText(softEdgeEnabled),
			BoolText(legacyCliffCoverBypassed),
			BoolText(hitMaxDrawRects));
	}

	static void LogOverlayPerfSummary(const OverlayPerfSnapshot& snapshot)
	{
		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt || !pRulesExt->PhobosFog_Perf_Enabled)
		{
			return;
		}

		static int lastFrame = -1;
		static DWORD lastTickMs = 0;
		static int overlayDrawCalls = 0;
		static unsigned int lastStateVersion = 0;
		static unsigned int lastOverlayEffectiveVersion = 0;
		static unsigned int lastEffectiveVisibilityVersionHash = 0;
		static unsigned int lastOverlayEffectiveVisibilityVersionHash = 0;
		static unsigned int lastOverlayViewportHash = 0;
		static unsigned int lastOverlayConfigHash = 0;
		static unsigned long long lastStateVersionTouchCount = 0;
		static unsigned long long lastStateVersionTouchReasons[HouseExt::PhobosFogStateTouchReasonCount] {};
		static unsigned long long lastOverlayEffectiveTouchCount = 0;
		static unsigned long long lastOverlayCacheHitCount = 0;
		static unsigned long long lastOverlayCacheMissCount = 0;
		static unsigned long long lastOverlayCacheRebuildCount = 0;

		const int intervalFrames = pRulesExt->PhobosFog_Perf_IntervalFrames > 0
			? pRulesExt->PhobosFog_Perf_IntervalFrames.Get()
			: 300;
		const int currentFrame = snapshot.Frame;
		const DWORD nowTickMs = GetTickCount();

		++overlayDrawCalls;

		if (lastFrame < 0 || currentFrame < lastFrame)
		{
			lastFrame = currentFrame;
			lastTickMs = nowTickMs;
			overlayDrawCalls = 0;
			lastStateVersion = snapshot.PhobosFogStateVersion;
			lastOverlayEffectiveVersion = snapshot.OverlayEffectiveVersion;
			lastEffectiveVisibilityVersionHash = snapshot.EffectiveVisibilityVersionHash;
			lastOverlayEffectiveVisibilityVersionHash = snapshot.OverlayEffectiveVisibilityVersionHash;
			lastOverlayViewportHash = snapshot.OverlayViewportHash;
			lastOverlayConfigHash = snapshot.OverlayConfigHash;
			lastStateVersionTouchCount = snapshot.StateVersionTouchCount;
			lastOverlayEffectiveTouchCount = snapshot.OverlayEffectiveTouchCount;
			lastOverlayCacheHitCount = snapshot.OverlayCacheHitCount;
			lastOverlayCacheMissCount = snapshot.OverlayCacheMissCount;
			lastOverlayCacheRebuildCount = snapshot.OverlayCacheRebuildCount;

			for (size_t i = 0; i < HouseExt::PhobosFogStateTouchReasonCount; ++i)
			{
				lastStateVersionTouchReasons[i] = snapshot.StateVersionTouchReasons[i];
			}

			return;
		}

		const int frameDelta = currentFrame - lastFrame;

		if (frameDelta < intervalFrames)
		{
			return;
		}

		const DWORD elapsedMs = nowTickMs - lastTickMs;

		if (elapsedMs == 0 || frameDelta <= 0)
		{
			lastFrame = currentFrame;
			lastTickMs = nowTickMs;
			overlayDrawCalls = 0;
			return;
		}

		const double fps = static_cast<double>(frameDelta) * 1000.0 / static_cast<double>(elapsedMs);
		unsigned long long reasonDeltas[HouseExt::PhobosFogStateTouchReasonCount] {};

		for (size_t i = 0; i < HouseExt::PhobosFogStateTouchReasonCount; ++i)
		{
			const auto currentReasonCount = snapshot.StateVersionTouchReasons[i];
			const auto previousReasonCount = lastStateVersionTouchReasons[i];
			reasonDeltas[i] = currentReasonCount >= previousReasonCount
				? currentReasonCount - previousReasonCount
				: 0;
		}

		const auto stateTouchesSinceLastPerf = snapshot.StateVersionTouchCount >= lastStateVersionTouchCount
			? snapshot.StateVersionTouchCount - lastStateVersionTouchCount
			: 0;
		const auto overlayEffectiveTouchesSinceLastPerf = snapshot.OverlayEffectiveTouchCount >= lastOverlayEffectiveTouchCount
			? snapshot.OverlayEffectiveTouchCount - lastOverlayEffectiveTouchCount
			: 0;
		const auto overlayCacheHitsSinceLastPerf = snapshot.OverlayCacheHitCount >= lastOverlayCacheHitCount
			? snapshot.OverlayCacheHitCount - lastOverlayCacheHitCount
			: 0;
		const auto overlayCacheMissesSinceLastPerf = snapshot.OverlayCacheMissCount >= lastOverlayCacheMissCount
			? snapshot.OverlayCacheMissCount - lastOverlayCacheMissCount
			: 0;
		const auto overlayCacheRebuildsSinceLastPerf = snapshot.OverlayCacheRebuildCount >= lastOverlayCacheRebuildCount
			? snapshot.OverlayCacheRebuildCount - lastOverlayCacheRebuildCount
			: 0;
		const unsigned int stateVersionDelta = snapshot.PhobosFogStateVersion - lastStateVersion;
		const unsigned int overlayEffectiveVersionDelta = snapshot.OverlayEffectiveVersion - lastOverlayEffectiveVersion;
		const bool rawEffectiveHashChanged = snapshot.EffectiveVisibilityVersionHash != lastEffectiveVisibilityVersionHash;
		const bool overlayEffectiveHashChanged = snapshot.OverlayEffectiveVisibilityVersionHash != lastOverlayEffectiveVisibilityVersionHash;
		const bool viewportHashChanged = snapshot.OverlayViewportHash != lastOverlayViewportHash;
		const bool configHashChanged = snapshot.OverlayConfigHash != lastOverlayConfigHash;
		const auto reasonIndex = [](HouseExt::PhobosFogStateTouchReason reason)
		{
			return static_cast<size_t>(reason);
		};

		Debug::Log(
			"[PhobosFog][Perf] Frame=%d ElapsedMs=%u FrameDelta=%d FPS=%.2f OverlayDrawCalls=%d SourceCells=%d RawMainSpans=%d MergedMainSpans=%d PreCompactMainSpans=%d CompactMainSpans=%d CompactClosedGaps=%d HeightFaceEnabled=%s HeightFaceAccepted=%d HeightFaceRawSpans=%d HeightFaceMaxDrop=%d FallbackDilationY=%d FallbackDilationRawSpans=%d FinalMergeInputSpans=%d FinalMergedSpans=%d ClosedGaps=%d UnionSpans=%d DrawRects=%d DrawRectBatchReduction=%d DrawRectScratchCapacity=%d GeometryProbes=%d VisibilityQueries=%d RowBucketInputSpans=%d RowBucketNonEmptyRows=%d RowBucketMainUnionSpans=%d TemplateEnabled=%s RectTemplateRows=%d DiamondTemplateRows=%d TemplateCandidateRows=%d TemplateVisibleRows=%d TemplateInstantiatedRows=%d TemplateClippedRows=%d TemplateYRejectedRows=%d TemplateXRejectedRows=%d TemplateFallbacks=%d MaxDrawRects=%d HitMaxDrawRects=%s SoftEdgeEnabled=%s LegacyCliffCoverBypassed=%s PhobosFogStateVersion=%u StateVersionDelta=%u RawEffectiveVisibilityVersionHash=%u RawEffectiveHashChanged=%s OverlayEffectiveVersion=%u OverlayEffectiveVersionDelta=%u OverlayEffectiveVisibilityVersionHash=%u OverlayEffectiveHashChanged=%s OverlayEffectiveBatchTouchedCells=%d OverlayEffectiveBatchChangedCells=%d OverlayEffectiveTouchesSinceLastPerf=%llu OverlayViewportHash=%u ViewportHashChanged=%s OverlayConfigHash=%u ConfigHashChanged=%s OverlayCacheEnabled=%s OverlayCacheAllowed=%s OverlayCacheDisabledReason=%s OverlayCacheHit=%s OverlayCacheMiss=%s OverlayCacheRebuild=%s OverlayCacheHitsSinceLastPerf=%llu OverlayCacheMissesSinceLastPerf=%llu OverlayCacheRebuildsSinceLastPerf=%llu OverlayCacheHitStreak=%d OverlayCacheCachedFinalSpans=%d TemporalVisibilityActive=%s TemporalCellExpiryScanSkipped=%s TemporalFullMapFirstInvalidFrame=%d TemporalCacheFirstInvalidFrame=%d TemporalCacheExpiresInFrames=%d TemporalCacheHitBlockedByExpiry=%s StageAOnly=%s StateTouchesSinceLastPerf=%llu TouchUnknown=%llu TouchReset=%llu TouchEnsureResize=%llu TouchDegradeVisibleToExplored=%llu TouchMarkExplored=%llu TouchMarkVisible=%llu TouchMarkVisibleUntil=%llu TouchMarkAreaVisible=%llu TouchMarkCellSpreadVisible=%llu TouchMarkAllExplored=%llu TouchMarkAllVisible=%llu TouchFullMapVisibleUntil=%llu TouchSpySatPersistentVisibleEdge=%llu TouchOther=%llu\n",
			currentFrame,
			static_cast<unsigned int>(elapsedMs),
			frameDelta,
			fps,
			overlayDrawCalls,
			snapshot.SourceCells,
			ClampDebugCount(snapshot.RawMainSpans),
			ClampDebugCount(snapshot.MergedMainSpans),
			ClampDebugCount(snapshot.PreCompactMainSpans),
			ClampDebugCount(snapshot.CompactMainSpans),
			snapshot.CompactClosedGaps,
			BoolText(snapshot.HeightFaceEnabled),
			snapshot.HeightFaceAccepted,
			ClampDebugCount(snapshot.HeightFaceRawSpans),
			snapshot.HeightFaceMaxDrop,
			snapshot.FallbackDilationY,
			ClampDebugCount(snapshot.FallbackDilationRawSpans),
			ClampDebugCount(snapshot.FinalMergeInputSpans),
			ClampDebugCount(snapshot.FinalMergedSpans),
			snapshot.ClosedGaps,
			ClampDebugCount(snapshot.UnionSpans),
			snapshot.DrawRects,
			snapshot.DrawRectBatchReduction,
			ClampDebugCount(snapshot.DrawRectScratchCapacity),
			snapshot.GeometryProbes,
			snapshot.VisibilityQueries,
			ClampDebugCount(snapshot.RowBucketInputSpans),
			snapshot.RowBucketNonEmptyRows,
			ClampDebugCount(snapshot.RowBucketMainUnionSpans),
			BoolText(snapshot.TemplateEnabled),
			ClampDebugCount(snapshot.RectTemplateRows),
			ClampDebugCount(snapshot.DiamondTemplateRows),
			ClampDebugCount(snapshot.TemplateCandidateRows),
			ClampDebugCount(snapshot.TemplateVisibleRows),
			ClampDebugCount(snapshot.TemplateInstantiatedRows),
			ClampDebugCount(snapshot.TemplateClippedRows),
			ClampDebugCount(snapshot.TemplateYRejectedRows),
			ClampDebugCount(snapshot.TemplateXRejectedRows),
			snapshot.TemplateFallbacks,
			snapshot.MaxDrawRects,
			BoolText(snapshot.HitMaxDrawRects),
			BoolText(snapshot.SoftEdgeEnabled),
			BoolText(snapshot.LegacyCliffCoverBypassed),
			snapshot.PhobosFogStateVersion,
			stateVersionDelta,
			snapshot.EffectiveVisibilityVersionHash,
			BoolText(rawEffectiveHashChanged),
			snapshot.OverlayEffectiveVersion,
			overlayEffectiveVersionDelta,
			snapshot.OverlayEffectiveVisibilityVersionHash,
			BoolText(overlayEffectiveHashChanged),
			ClampDebugCount(snapshot.OverlayEffectiveBatchTouchedCells),
			ClampDebugCount(snapshot.OverlayEffectiveBatchChangedCells),
			overlayEffectiveTouchesSinceLastPerf,
			snapshot.OverlayViewportHash,
			BoolText(viewportHashChanged),
			snapshot.OverlayConfigHash,
			BoolText(configHashChanged),
			BoolText(snapshot.OverlayCacheEnabled),
			BoolText(snapshot.OverlayCacheAllowed),
			snapshot.OverlayCacheDisabledReason ? snapshot.OverlayCacheDisabledReason : "<null>",
			BoolText(snapshot.OverlayCacheHit),
			BoolText(snapshot.OverlayCacheMiss),
			BoolText(snapshot.OverlayCacheRebuild),
			overlayCacheHitsSinceLastPerf,
			overlayCacheMissesSinceLastPerf,
			overlayCacheRebuildsSinceLastPerf,
			snapshot.OverlayCacheHitStreak,
			ClampDebugCount(snapshot.OverlayCacheCachedFinalSpans),
			BoolText(snapshot.TemporalVisibilityActive),
			BoolText(snapshot.TemporalCellExpiryScanSkipped),
			snapshot.TemporalFullMapFirstInvalidFrame,
			snapshot.TemporalCacheFirstInvalidFrame,
			snapshot.TemporalCacheExpiresInFrames,
			BoolText(snapshot.TemporalCacheHitBlockedByExpiry),
			BoolText(snapshot.StageAOnly),
			stateTouchesSinceLastPerf,
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::Unknown)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::Reset)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::EnsureResize)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::DegradeVisibleToExplored)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::MarkExplored)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::MarkVisible)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::MarkVisibleUntil)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::MarkAreaVisible)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::MarkCellSpreadVisible)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::MarkAllExplored)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::MarkAllVisible)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::FullMapVisibleUntil)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::SpySatPersistentVisibleEdge)],
			reasonDeltas[reasonIndex(HouseExt::PhobosFogStateTouchReason::Other)]);

		lastFrame = currentFrame;
		lastTickMs = nowTickMs;
		overlayDrawCalls = 0;
		lastStateVersion = snapshot.PhobosFogStateVersion;
		lastOverlayEffectiveVersion = snapshot.OverlayEffectiveVersion;
		lastEffectiveVisibilityVersionHash = snapshot.EffectiveVisibilityVersionHash;
		lastOverlayEffectiveVisibilityVersionHash = snapshot.OverlayEffectiveVisibilityVersionHash;
		lastOverlayViewportHash = snapshot.OverlayViewportHash;
		lastOverlayConfigHash = snapshot.OverlayConfigHash;
		lastStateVersionTouchCount = snapshot.StateVersionTouchCount;
		lastOverlayEffectiveTouchCount = snapshot.OverlayEffectiveTouchCount;
		lastOverlayCacheHitCount = snapshot.OverlayCacheHitCount;
		lastOverlayCacheMissCount = snapshot.OverlayCacheMissCount;
		lastOverlayCacheRebuildCount = snapshot.OverlayCacheRebuildCount;

		for (size_t i = 0; i < HouseExt::PhobosFogStateTouchReasonCount; ++i)
		{
			lastStateVersionTouchReasons[i] = snapshot.StateVersionTouchReasons[i];
		}
	}

	static int GetCliffProbeOffsetIndex(const CellStruct& offset)
	{
		if (offset.X == 0 && offset.Y == -1)
		{
			return 0;
		}

		if (offset.X == 0 && offset.Y == 1)
		{
			return 1;
		}

		if (offset.X == -1 && offset.Y == 0)
		{
			return 2;
		}

		if (offset.X == 1 && offset.Y == 0)
		{
			return 3;
		}

		return -1;
	}

	static int GetCliffProbeEdgeIndex(const DiamondEdge edge)
	{
		switch (edge)
		{
		case DiamondEdge::TopLeft:
			return 0;
		case DiamondEdge::TopRight:
			return 1;
		case DiamondEdge::BottomLeft:
			return 2;
		case DiamondEdge::BottomRight:
			return 3;
		default:
			return -1;
		}
	}

	static void LogCliffProbeActivation(
		const int alpha,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY,
		const int viewportCellPadding,
		const bool heightAware,
		const int heightYOffset,
		const bool cliffCover,
		const int cliffCoverHeight,
		const int cliffCoverAlpha)
	{
		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt || !IsCliffProbeLogFrame())
		{
			return;
		}

		Debug::Log(
			"[PhobosFog][CliffProbeActivation] Frame=%d Enabled=%s DrawExploredOverlay=%s CliffCover=%s Alpha=%d CellSize=(%d,%d) Padding=(%d,%d) ViewportPadding=%d HeightAware=%s HeightYOffset=%d CliffCoverHeight=%d CliffCoverAlpha=%d Composite=%s Tactical=%s CurrentPlayer=%s\n",
			Unsorted::CurrentFrame,
			BoolText(pRulesExt->PhobosFog_Enabled),
			BoolText(pRulesExt->PhobosFog_DrawExploredOverlay),
			BoolText(cliffCover),
			alpha,
			cellOverlayWidth,
			cellOverlayHeight,
			paddingX,
			paddingY,
			viewportCellPadding,
			BoolText(heightAware),
			heightYOffset,
			cliffCoverHeight,
			cliffCoverAlpha,
			BoolText(DSurface::Composite != nullptr),
			BoolText(TacticalClass::Instance != nullptr),
			BoolText(HouseClass::CurrentPlayer != nullptr));
	}

	static void LogCliffProbeCollect(
		CliffProbeStats* const pStats,
		const CellStruct& cell,
		const int cellIndex,
		const bool hasCell,
		const int maxFaceHeight,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY)
	{
		if (!pStats)
		{
			return;
		}

		++pStats->CollectCalls;

		constexpr int MaxCliffProbeCollectLines = 16;

		if (pStats->CollectLines >= MaxCliffProbeCollectLines)
		{
			return;
		}

		++pStats->CollectLines;

		Debug::Log(
			"[PhobosFog][CliffProbeCollect] Frame=%d Call=%d CellIndex=%d Cell=(%d,%d) HasCell=%s MaxFaceHeight=%d CellSize=(%d,%d) Padding=(%d,%d)\n",
			Unsorted::CurrentFrame,
			pStats->CollectCalls,
			cellIndex,
			cell.X,
			cell.Y,
			BoolText(hasCell),
			maxFaceHeight,
			cellOverlayWidth,
			cellOverlayHeight,
			paddingX,
			paddingY);
	}

	static void RecordCliffCoverProbe(
		CliffProbeStats* const pStats,
		const CellStruct& cell,
		const int cellIndex,
		const OverlayCellKind currentKind,
		const int currentZ,
		const int currentLevel,
		const CellStruct& offset,
		const CellStruct& neighbor,
		const int neighborIndex,
		const char* const pReason,
		const bool accepted,
		const bool edgeKnown,
		const int zDelta,
		const int levelDelta,
		const int neighborZ,
		const int neighborLevel,
		const bool currentIsCliffTile,
		const bool neighborIsCliffTile,
		const bool cliffTile,
		const bool significantHeightDrop,
		const OverlayCellKind neighborKind,
		const int projectedPixels,
		const int faceDepth,
		const Point2D& flatCenter,
		const Point2D& drawCenter,
		const Point2D& neighborFlatCenter,
		const Point2D& neighborDrawCenter,
		const Point2D& faceDirection,
		const Point2D& highA,
		const Point2D& highB,
		const Point2D& lowA,
		const Point2D& lowB,
		const DiamondEdge highEdge,
		const DiamondEdge lowEdge,
		const bool forward)
	{
		if (!pStats)
		{
			return;
		}

		++pStats->Candidates;

		const int offsetIndex = GetCliffProbeOffsetIndex(offset);
		const int edgeIndex = GetCliffProbeEdgeIndex(highEdge);

		if (cliffTile)
		{
			++pStats->CliffTileCandidates;
		}

		if (offsetIndex >= 0)
		{
			++pStats->ByOffset[static_cast<size_t>(offsetIndex)];
		}

		if (edgeKnown && edgeIndex >= 0)
		{
			++pStats->ByEdge[static_cast<size_t>(edgeIndex)];
		}

		if (accepted)
		{
			++pStats->Accepted;

			if (std::strcmp(pReason, "AcceptedCliffTile") == 0)
			{
				++pStats->AcceptedCliffTile;
			}
			else if (std::strcmp(pReason, "AcceptedOrdinaryForward") == 0)
			{
				++pStats->AcceptedOrdinaryForward;
			}

			if (cliffTile)
			{
				++pStats->CliffTileAccepted;
			}
			else
			{
				++pStats->NonCliffAccepted;
			}

			if (currentIsCliffTile)
			{
				++pStats->AcceptedByCurrentCliffTile;
			}

			if (neighborIsCliffTile)
			{
				++pStats->AcceptedByNeighborCliffTile;
			}

			if (offsetIndex >= 0)
			{
				++pStats->AcceptedByOffset[static_cast<size_t>(offsetIndex)];
			}

			if (edgeKnown && edgeIndex >= 0)
			{
				++pStats->AcceptedByEdge[static_cast<size_t>(edgeIndex)];
			}
		}
		else if (std::strcmp(pReason, "RejectNotHigher") == 0)
		{
			if (cliffTile)
			{
				++pStats->CliffTileRejected;
			}

			++pStats->RejectedNotHigher;
		}
		else if (std::strcmp(pReason, "RejectBackFaceOffset") == 0)
		{
			if (cliffTile)
			{
				++pStats->CliffTileRejected;
			}

			++pStats->RejectedBackFace;

			if (currentIsCliffTile)
			{
				++pStats->RejectedBackFaceByCurrentCliffTile;
			}

			if (neighborIsCliffTile)
			{
				++pStats->RejectedBackFaceByNeighborCliffTile;
			}

			if (offsetIndex >= 0)
			{
				++pStats->RejectedBackFaceByOffset[static_cast<size_t>(offsetIndex)];
			}

			if (edgeKnown && edgeIndex >= 0)
			{
				++pStats->RejectedBackFaceByEdge[static_cast<size_t>(edgeIndex)];
			}
		}
		else if (std::strcmp(pReason, "RejectBelowThreshold") == 0
			|| std::strcmp(pReason, "RejectFaceHeightZero") == 0)
		{
			if (cliffTile)
			{
				++pStats->CliffTileRejected;
			}

			++pStats->RejectedThreshold;
			++pStats->RejectedBelowThreshold;
		}
		else if (std::strcmp(pReason, "RejectNoNeighborCell") == 0
			|| std::strcmp(pReason, "RejectEdgeCapacity") == 0
			|| std::strcmp(pReason, "RejectEdgePoints") == 0)
		{
			if (cliffTile)
			{
				++pStats->CliffTileRejected;
			}

			++pStats->RejectedInvalid;
		}
		else
		{
			if (cliffTile)
			{
				++pStats->CliffTileRejected;
			}

			++pStats->RejectedUnknown;
		}

		if (!cliffTile || !significantHeightDrop)
		{
			return;
		}

		constexpr int MaxCliffProbeDetailLines = 64;

		if (pStats->DetailLines >= MaxCliffProbeDetailLines)
		{
			return;
		}

		++pStats->DetailLines;

		const int quadMinX = std::min(std::min(highA.X, highB.X), std::min(lowA.X, lowB.X));
		const int quadMaxX = std::max(std::max(highA.X, highB.X), std::max(lowA.X, lowB.X));
		const int quadMinY = std::min(std::min(highA.Y, highB.Y), std::min(lowA.Y, lowB.Y));
		const int quadMaxY = std::max(std::max(highA.Y, highB.Y), std::max(lowA.Y, lowB.Y));
		const int dropVectorX = lowA.X - highA.X;
		const int dropVectorY = lowA.Y - highA.Y;

		Debug::Log(
			"[PhobosFog][CliffProbeDetail] Frame=%d CellIndex=%d Cell=(%d,%d) NeighborIndex=%d Neighbor=(%d,%d) Offset=(%d,%d) Kind=%s NeighborKind=%s Z=%d NZ=%d ZDelta=%d Level=%d NLevel=%d LevelDelta=%d ProjectedPixels=%d FaceDepth=%d FlatC=(%d,%d) DrawC=(%d,%d) NFlatC=(%d,%d) NDrawC=(%d,%d) Edge=%s LowEdge=%s Forward=%s OrdinaryForward=%s FlatDir=(%d,%d) DrawDir=(%d,%d) DropVector=(%d,%d) HighA=(%d,%d) HighB=(%d,%d) LowA=(%d,%d) LowB=(%d,%d) QuadX=[%d,%d] QuadY=[%d,%d] Accepted=%s Reason=%s CurrentIsCliffTile=%s NeighborIsCliffTile=%s MergedCliffTile=%s Significant=%s\n",
			Unsorted::CurrentFrame,
			cellIndex,
			cell.X,
			cell.Y,
			neighborIndex,
			neighbor.X,
			neighbor.Y,
			offset.X,
			offset.Y,
			GetOverlayCellKindName(currentKind),
			GetOverlayCellKindName(neighborKind),
			currentZ,
			neighborZ,
			zDelta,
			currentLevel,
			neighborLevel,
			levelDelta,
			projectedPixels,
			faceDepth,
			flatCenter.X,
			flatCenter.Y,
			drawCenter.X,
			drawCenter.Y,
			neighborFlatCenter.X,
			neighborFlatCenter.Y,
			neighborDrawCenter.X,
			neighborDrawCenter.Y,
			edgeKnown ? GetDiamondEdgeName(highEdge) : "<none>",
			edgeKnown ? GetDiamondEdgeName(lowEdge) : "<none>",
			BoolText(forward),
			BoolText(forward),
			neighborFlatCenter.X - flatCenter.X,
			neighborFlatCenter.Y - flatCenter.Y,
			neighborDrawCenter.X - drawCenter.X,
			neighborDrawCenter.Y - drawCenter.Y,
			dropVectorX,
			dropVectorY,
			highA.X,
			highA.Y,
			highB.X,
			highB.Y,
			lowA.X,
			lowA.Y,
			lowB.X,
			lowB.Y,
			quadMinX,
			quadMaxX,
			quadMinY,
			quadMaxY,
			BoolText(accepted),
			pReason,
			BoolText(currentIsCliffTile),
			BoolText(neighborIsCliffTile),
			BoolText(cliffTile),
			BoolText(significantHeightDrop));
	}

	static void LogCliffProbeSummary(const CliffProbeStats& stats)
	{
		Debug::Log(
			"[PhobosFog][CliffProbeSummary] Frame=%d CollectCalls=%d CollectLines=%d Candidates=%d Accepted=%d AcceptedCliffTile=%d AcceptedOrdinaryForward=%d RejectedNotHigher=%d RejectedBackFace=%d RejectedThreshold=%d RejectedBelowThreshold=%d RejectedInvalid=%d RejectedUnknown=%d CliffTileCandidates=%d CliffTileAccepted=%d CliffTileRejected=%d NonCliffAccepted=%d AcceptedByCurrentCliffTile=%d AcceptedByNeighborCliffTile=%d RejectedBackFaceByCurrentCliffTile=%d RejectedBackFaceByNeighborCliffTile=%d ByOffset_0_Minus1=%d ByOffset_0_1=%d ByOffset_Minus1_0=%d ByOffset_1_0=%d AcceptedByOffset_0_Minus1=%d AcceptedByOffset_0_1=%d AcceptedByOffset_Minus1_0=%d AcceptedByOffset_1_0=%d RejectedBackFaceByOffset_0_Minus1=%d RejectedBackFaceByOffset_0_1=%d RejectedBackFaceByOffset_Minus1_0=%d RejectedBackFaceByOffset_1_0=%d ByEdge_TopLeft=%d ByEdge_TopRight=%d ByEdge_BottomLeft=%d ByEdge_BottomRight=%d AcceptedByEdge_TopLeft=%d AcceptedByEdge_TopRight=%d AcceptedByEdge_BottomLeft=%d AcceptedByEdge_BottomRight=%d RejectedBackFaceByEdge_TopLeft=%d RejectedBackFaceByEdge_TopRight=%d RejectedBackFaceByEdge_BottomLeft=%d RejectedBackFaceByEdge_BottomRight=%d DetailLines=%d\n",
			Unsorted::CurrentFrame,
			stats.CollectCalls,
			stats.CollectLines,
			stats.Candidates,
			stats.Accepted,
			stats.AcceptedCliffTile,
			stats.AcceptedOrdinaryForward,
			stats.RejectedNotHigher,
			stats.RejectedBackFace,
			stats.RejectedThreshold,
			stats.RejectedBelowThreshold,
			stats.RejectedInvalid,
			stats.RejectedUnknown,
			stats.CliffTileCandidates,
			stats.CliffTileAccepted,
			stats.CliffTileRejected,
			stats.NonCliffAccepted,
			stats.AcceptedByCurrentCliffTile,
			stats.AcceptedByNeighborCliffTile,
			stats.RejectedBackFaceByCurrentCliffTile,
			stats.RejectedBackFaceByNeighborCliffTile,
			stats.ByOffset[0],
			stats.ByOffset[1],
			stats.ByOffset[2],
			stats.ByOffset[3],
			stats.AcceptedByOffset[0],
			stats.AcceptedByOffset[1],
			stats.AcceptedByOffset[2],
			stats.AcceptedByOffset[3],
			stats.RejectedBackFaceByOffset[0],
			stats.RejectedBackFaceByOffset[1],
			stats.RejectedBackFaceByOffset[2],
			stats.RejectedBackFaceByOffset[3],
			stats.ByEdge[0],
			stats.ByEdge[1],
			stats.ByEdge[2],
			stats.ByEdge[3],
			stats.AcceptedByEdge[0],
			stats.AcceptedByEdge[1],
			stats.AcceptedByEdge[2],
			stats.AcceptedByEdge[3],
			stats.RejectedBackFaceByEdge[0],
			stats.RejectedBackFaceByEdge[1],
			stats.RejectedBackFaceByEdge[2],
			stats.RejectedBackFaceByEdge[3],
			stats.DetailLines);
	}

	static void AddDiamondSoftEdgeSideSpans(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& rect,
		const RectangleStruct& bounds,
		const int alpha,
		const int padding,
		const DiamondEdge edge)
	{
		constexpr int MaxSoftEdgePadding = 16;
		const int safePadding = padding < 1 ? 0 : padding < MaxSoftEdgePadding ? padding : MaxSoftEdgePadding;
		const int safeAlpha = ClampAlpha(alpha);

		if (rect.Width <= 0 || rect.Height <= 0 || safePadding <= 0 || safeAlpha <= 0)
		{
			return;
		}

		const int top = rect.Y > bounds.Y ? rect.Y : bounds.Y;
		const int bottom = rect.Y + rect.Height < bounds.Y + bounds.Height ? rect.Y + rect.Height : bounds.Y + bounds.Height;
		const int centerY = rect.Y + rect.Height / 2;

		for (int y = top; y < bottom; ++y)
		{
			int rowX1 = 0;
			int rowX2 = 0;

			if (!TryGetDiamondSpan(rect, y, rowX1, rowX2))
			{
				continue;
			}

			const bool isTopHalf = y <= centerY;

			if ((edge == DiamondEdge::TopLeft || edge == DiamondEdge::TopRight) != isTopHalf)
			{
				continue;
			}

			for (int layer = safePadding - 1; layer >= 0; --layer)
			{
				const int layerAlpha = ClampAlpha(safeAlpha * (safePadding - layer) / safePadding);

				if (layerAlpha <= 0)
				{
					continue;
				}

				const int width = layer + 1;

				if (edge == DiamondEdge::TopLeft || edge == DiamondEdge::BottomLeft)
				{
					const int spanX2 = rowX1 + width < rowX2 ? rowX1 + width : rowX2;
					AddClippedSpan(spans, bounds, y, rowX1, spanX2, layerAlpha);
				}
				else
				{
					const int spanX1 = rowX2 - width > rowX1 ? rowX2 - width : rowX1;
					AddClippedSpan(spans, bounds, y, spanX1, rowX2, layerAlpha);
				}
			}
		}
	}

	static bool IsForwardCliffCoverOffset(const CellStruct& offset)
	{
		return (offset.X == 1 && offset.Y == 0) || (offset.X == 0 && offset.Y == 1);
	}

	static void AddDiamondSoftEdgeTowardNeighborSpans(
		std::vector<OverlaySpan>& spans,
		const RectangleStruct& rect,
		const RectangleStruct& bounds,
		const int alpha,
		const int padding,
		const Point2D& center,
		const Point2D& neighborCenter)
	{
		AddDiamondSoftEdgeSideSpans(
			spans,
			rect,
			bounds,
			alpha,
			padding,
			GetDiamondEdgeTowardNeighbor(center, neighborCenter));
	}

	static bool IsCliffFaceTile(CellClass* const pCell)
	{
		return pCell && (pCell->Tile_Is_Cliff() || pCell->Tile_Is_DestroyableCliff());
	}

	static int ComputeHeightDropPixels(const CellStruct& cell, const CellStruct& neighbor, const int heightYOffset)
	{
		const auto [highHeightCenter, highHeightVisible] = GetOverlayCellClient(cell, true, heightYOffset);
		const auto [lowHeightCenter, lowHeightVisible] = GetOverlayCellClient(neighbor, true, heightYOffset);
		const auto [highFlatCenter, highFlatVisible] = GetOverlayCellClient(cell, false, heightYOffset);
		const auto [lowFlatCenter, lowFlatVisible] = GetOverlayCellClient(neighbor, false, heightYOffset);
		(void)highHeightVisible;
		(void)lowHeightVisible;
		(void)highFlatVisible;
		(void)lowFlatVisible;

		const int heightAwareDelta = lowHeightCenter.Y - highHeightCenter.Y;
		const int flatDelta = lowFlatCenter.Y - highFlatCenter.Y;

		return AbsInt(heightAwareDelta - flatDelta);
	}

	static void CollectCliffCoverEdges(
		HouseClass* const pViewerHouse,
		const CellStruct& cell,
		const int heightYOffset,
		const int cellOverlayWidth,
		const int cellOverlayHeight,
		const int paddingX,
		const int paddingY,
		const int maxFaceHeight,
		std::array<CliffCoverEdge, 4>& edges,
		int& edgeCount,
		CliffProbeStats* const pProbeStats)
	{
		edgeCount = 0;

		if (maxFaceHeight <= 0)
		{
			return;
		}

		const auto pCell = MapClass::Instance.TryGetCellAt(cell);
		const int cellIndex = pCell ? MapClass::GetCellIndex(cell) : -1;

		LogCliffProbeCollect(
			pProbeStats,
			cell,
			cellIndex,
			pCell != nullptr,
			maxFaceHeight,
			cellOverlayWidth,
			cellOverlayHeight,
			paddingX,
			paddingY);

		if (!pCell)
		{
			return;
		}

		const auto cellCoords = pCell->GetCellCoords();
		const int cellLevel = pCell->GetLevel();
		const auto currentKind = pProbeStats ? GetOverlayCellKindForCell(pViewerHouse, cell) : OverlayCellKind::ExploredOverlay;
		const auto [drawCenter, drawCenterVisible] = GetOverlayCellClient(cell, true, heightYOffset);
		const auto [flatCenter, flatCenterVisible] = GetOverlayCellClient(cell, false, heightYOffset);
		(void)drawCenterVisible;
		(void)flatCenterVisible;

		constexpr int SignificantZDrop = 96;
		constexpr int MinCliffFacePixels = 10;
		constexpr int MinCliffTileFacePixels = 6;
		constexpr int MinCliffTileCoverDepth = 10;
		constexpr int MaxCliffFaceDepth = 24;
		constexpr CellStruct NeighborOffsets[] =
		{
			{ 0, -1 },
			{ 0, 1 },
			{ -1, 0 },
			{ 1, 0 }
		};
		const int maxAnchoredFaceDepth = std::min(maxFaceHeight, MaxCliffFaceDepth);

		if (maxAnchoredFaceDepth <= 0)
		{
			return;
		}

		for (const auto& offset : NeighborOffsets)
		{
			const CellStruct neighbor
			{
				static_cast<short>(cell.X + offset.X),
				static_cast<short>(cell.Y + offset.Y)
			};

			const auto neighborKind = GetOverlayCellKindForCell(pViewerHouse, neighbor);
			const auto pNeighborCell = MapClass::Instance.TryGetCellAt(neighbor);
			const int neighborIndex = pNeighborCell ? MapClass::GetCellIndex(neighbor) : -1;
			const Point2D zeroPoint {};
			Point2D neighborFlatCenterForProbe {};
			Point2D neighborDrawCenterForProbe {};

			if (pProbeStats)
			{
				const auto [neighborFlatCenter, neighborFlatVisible] = GetOverlayCellClient(neighbor, false, heightYOffset);
				const auto [neighborDrawCenter, neighborDrawVisible] = GetOverlayCellClient(neighbor, true, heightYOffset);
				(void)neighborFlatVisible;
				(void)neighborDrawVisible;
				neighborFlatCenterForProbe = neighborFlatCenter;
				neighborDrawCenterForProbe = neighborDrawCenter;
			}

			auto recordProbe = [&](const char* const pReason,
				const bool accepted,
				const bool edgeKnown,
				const int zDelta,
				const int levelDelta,
				const int neighborZ,
				const int neighborLevel,
				const bool currentIsCliffTile,
				const bool neighborIsCliffTile,
				const bool cliffTile,
				const bool significantHeightDrop,
				const int projectedPixels,
				const int faceDepth,
				const Point2D& faceDirection,
				const Point2D& highA,
				const Point2D& highB,
				const Point2D& lowA,
				const Point2D& lowB,
				const DiamondEdge highEdge,
				const DiamondEdge lowEdge,
				const bool forward)
			{
				RecordCliffCoverProbe(
					pProbeStats,
					cell,
					cellIndex,
					currentKind,
					cellCoords.Z,
					cellLevel,
					offset,
					neighbor,
					neighborIndex,
					pReason,
					accepted,
					edgeKnown,
					zDelta,
					levelDelta,
					neighborZ,
					neighborLevel,
					currentIsCliffTile,
					neighborIsCliffTile,
					cliffTile,
					significantHeightDrop,
					neighborKind,
					projectedPixels,
					faceDepth,
					flatCenter,
					drawCenter,
					neighborFlatCenterForProbe,
					neighborDrawCenterForProbe,
					faceDirection,
					highA,
					highB,
					lowA,
					lowB,
					highEdge,
					lowEdge,
					forward);
			};

			if (!pNeighborCell)
			{
				recordProbe(
					"RejectNoNeighborCell",
					false,
					false,
					0,
					0,
					0,
					0,
					false,
					false,
					false,
					false,
					0,
					0,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					DiamondEdge::TopLeft,
					DiamondEdge::TopLeft,
					false);
				continue;
			}

			const auto neighborCoords = pNeighborCell->GetCellCoords();
			const int zDelta = cellCoords.Z - neighborCoords.Z;
			const int levelDelta = cellLevel - pNeighborCell->GetLevel();
			const bool currentIsCliffTile = IsCliffFaceTile(pCell);
			const bool neighborIsCliffTile = IsCliffFaceTile(pNeighborCell);
			const bool cliffTile = currentIsCliffTile || neighborIsCliffTile;
			const bool significantHeightDrop = zDelta >= SignificantZDrop || levelDelta >= 1;

			if (zDelta <= 0 && levelDelta <= 0)
			{
				recordProbe(
					"RejectNotHigher",
					false,
					false,
					zDelta,
					levelDelta,
					neighborCoords.Z,
					pNeighborCell->GetLevel(),
					currentIsCliffTile,
					neighborIsCliffTile,
					cliffTile,
					significantHeightDrop,
					0,
					0,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					DiamondEdge::TopLeft,
					DiamondEdge::TopLeft,
					false);
				continue;
			}

			if (!significantHeightDrop)
			{
				recordProbe(
					"RejectBelowThreshold",
					false,
					false,
					zDelta,
					levelDelta,
					neighborCoords.Z,
					pNeighborCell->GetLevel(),
					currentIsCliffTile,
					neighborIsCliffTile,
					cliffTile,
					significantHeightDrop,
					0,
					0,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					DiamondEdge::TopLeft,
					DiamondEdge::TopLeft,
					false);
				continue;
			}

			int faceHeight = ComputeHeightDropPixels(cell, neighbor, heightYOffset);
			const int rawFaceHeight = faceHeight;

			if (faceHeight < (cliffTile ? MinCliffTileFacePixels : MinCliffFacePixels))
			{
				if (!cliffTile && levelDelta < 1)
				{
					recordProbe(
						"RejectBelowThreshold",
						false,
						false,
						zDelta,
						levelDelta,
						neighborCoords.Z,
						pNeighborCell->GetLevel(),
						currentIsCliffTile,
						neighborIsCliffTile,
						cliffTile,
						significantHeightDrop,
						rawFaceHeight,
						rawFaceHeight,
						zeroPoint,
						zeroPoint,
						zeroPoint,
						zeroPoint,
						zeroPoint,
						DiamondEdge::TopLeft,
						DiamondEdge::TopLeft,
						false);
					continue;
				}

				faceHeight = MinCliffTileCoverDepth;
			}

			if (faceHeight > maxAnchoredFaceDepth)
			{
				faceHeight = maxAnchoredFaceDepth;
			}

			if (faceHeight <= 0 || edgeCount >= static_cast<int>(edges.size()))
			{
				recordProbe(
					faceHeight <= 0 ? "RejectFaceHeightZero" : "RejectEdgeCapacity",
					false,
					false,
					zDelta,
					levelDelta,
					neighborCoords.Z,
					pNeighborCell->GetLevel(),
					currentIsCliffTile,
					neighborIsCliffTile,
					cliffTile,
					significantHeightDrop,
					rawFaceHeight,
					faceHeight,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					DiamondEdge::TopLeft,
					DiamondEdge::TopLeft,
					false);
				continue;
			}

			const auto [neighborFlatCenter, neighborFlatVisible] = GetOverlayCellClient(neighbor, false, heightYOffset);
			(void)neighborFlatVisible;
			const RectangleStruct highRect
			{
				drawCenter.X - cellOverlayWidth / 2 - paddingX,
				drawCenter.Y - cellOverlayHeight / 2 - paddingY,
				cellOverlayWidth,
				cellOverlayHeight
			};
			const auto highEdge = GetDiamondEdgeTowardNeighbor(flatCenter, neighborFlatCenter);
			const auto lowEdge = GetDiamondEdgeTowardNeighbor(neighborFlatCenter, flatCenter);
			const bool ordinaryForward = IsForwardCliffCoverOffset(offset);
			const bool cliffTileBacked = cliffTile && significantHeightDrop;
			const bool shouldDraw = cliffTileBacked || ordinaryForward;

			if (!shouldDraw)
			{
				const Point2D faceDirection
				{
					neighborFlatCenter.X - flatCenter.X,
					neighborFlatCenter.Y - flatCenter.Y
				};

				recordProbe(
					"RejectBackFaceOffset",
					false,
					true,
					zDelta,
					levelDelta,
					neighborCoords.Z,
					pNeighborCell->GetLevel(),
					currentIsCliffTile,
					neighborIsCliffTile,
					cliffTile,
					significantHeightDrop,
					rawFaceHeight,
					faceHeight,
					faceDirection,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					highEdge,
					lowEdge,
					ordinaryForward);
				continue;
			}

			Point2D highA {};
			Point2D highB {};

			if (!TryGetDiamondEdgePoints(highRect, highEdge, highA, highB))
			{
				const Point2D faceDirection
				{
					neighborFlatCenter.X - flatCenter.X,
					neighborFlatCenter.Y - flatCenter.Y
				};

				recordProbe(
					"RejectEdgePoints",
					false,
					true,
					zDelta,
					levelDelta,
					neighborCoords.Z,
					pNeighborCell->GetLevel(),
					currentIsCliffTile,
					neighborIsCliffTile,
					cliffTile,
					significantHeightDrop,
					rawFaceHeight,
					faceHeight,
					faceDirection,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					zeroPoint,
					highEdge,
					lowEdge,
					ordinaryForward);
				continue;
			}

			const Point2D faceDirection
			{
				neighborFlatCenter.X - flatCenter.X,
				neighborFlatCenter.Y - flatCenter.Y
			};
			const Point2D lowA { highA.X, highA.Y + faceHeight };
			const Point2D lowB { highB.X, highB.Y + faceHeight };

			edges[static_cast<size_t>(edgeCount++)] = CliffCoverEdge
			{
				highA,
				highB,
				lowA,
				lowB,
				neighborKind
			};

			recordProbe(
				cliffTileBacked ? "AcceptedCliffTile" : "AcceptedOrdinaryForward",
				true,
				true,
				zDelta,
				levelDelta,
				neighborCoords.Z,
				pNeighborCell->GetLevel(),
				currentIsCliffTile,
				neighborIsCliffTile,
				cliffTile,
				significantHeightDrop,
				rawFaceHeight,
				faceHeight,
				faceDirection,
				highA,
				highB,
				lowA,
				lowB,
				highEdge,
				lowEdge,
				ordinaryForward);
		}
	}

	static void Draw()
	{
		OverlayFrameGeometryProbes = 0;
		OverlayFrameVisibilityQueries = 0;
		const auto pRulesExt = RulesExt::Global();

		if (!pRulesExt)
		{
			return;
		}

		const int alpha = ClampAlpha(pRulesExt->PhobosFog_ExploredOverlayAlpha.Get());
		const int cellOverlayWidth = pRulesExt->PhobosFog_ExploredOverlayCellWidth.Get();
		const int cellOverlayHeight = pRulesExt->PhobosFog_ExploredOverlayCellHeight.Get();
		const int paddingX = pRulesExt->PhobosFog_ExploredOverlayPaddingX.Get();
		const int paddingY = pRulesExt->PhobosFog_ExploredOverlayPaddingY.Get();
		const int viewportCellPadding = pRulesExt->PhobosFog_ExploredOverlayViewportPaddingCells.Get();
		const bool softEdge = pRulesExt->PhobosFog_ExploredOverlaySoftEdge;
		const int softEdgeVisibleAlpha = ClampAlpha(pRulesExt->PhobosFog_ExploredOverlaySoftEdgeVisibleAlpha.Get());
		const int fadeInFrames = pRulesExt->PhobosFog_ExploredOverlayFadeInFrames.Get();
		const int softEdgePadding = pRulesExt->PhobosFog_ExploredOverlaySoftEdgePadding.Get();
		const int alphaVariance = pRulesExt->PhobosFog_ExploredOverlayAlphaVariance.Get();
		const int frontierMode = pRulesExt->PhobosFog_ExploredOverlayFrontierMode.Get();
		const int overlayShape = pRulesExt->PhobosFog_ExploredOverlayShape.Get();
		const int maxDrawRects = pRulesExt->PhobosFog_ExploredOverlayMaxDrawRects.Get();
		const bool heightAware = pRulesExt->PhobosFog_ExploredOverlayHeightAware;
		const int heightYOffset = pRulesExt->PhobosFog_ExploredOverlayHeightYOffset.Get();
		const bool cliffCover = pRulesExt->PhobosFog_ExploredOverlayCliffCover;
		const int cliffCoverHeight = pRulesExt->PhobosFog_ExploredOverlayCliffCoverHeight.Get();
		const int cliffCoverAlpha = ClampAlpha(pRulesExt->PhobosFog_ExploredOverlayCliffCoverAlpha.Get());
		constexpr bool UseOverlayRegionCachePrototype = true;
		// Legacy cliff-cover collection is intentionally bypassed; C2 height-discontinuity
		// face spans are the active vertical coverage path for the P9 baseline.
		constexpr bool UseLegacyCliffCoverInRegionMaskPrototype = false;
		constexpr bool UseRegionDilationPrototype = true;
		constexpr bool UseHeightDiscontinuityFacePrototype = true;
		constexpr bool UseCellShapeGeometryTemplatePrototype = true;
		constexpr int VerticalFaceMinDropPixels = 4;
		constexpr int VerticalFaceMaxDropPixels = 64;
		constexpr int VerticalFaceFallbackDilationY = 2;
		constexpr int RegionDilationY = VerticalFaceFallbackDilationY;
		constexpr int RegionCloseGapPixels = 2;
		constexpr int RegionDilationAlphaNumerator = 1;
		constexpr int RegionDilationAlphaDenominator = 1;
		const bool legacyCliffCoverRequested = cliffCover && cliffCoverHeight > 0 && cliffCoverAlpha > 0;
		const bool legacyCliffCoverBypassed = legacyCliffCoverRequested && !UseLegacyCliffCoverInRegionMaskPrototype;
		const bool softEdgeEnabled = softEdge && softEdgeVisibleAlpha > 0 && softEdgePadding > 0;
		const bool dilationEnabled = UseRegionDilationPrototype && RegionDilationY > 0;
		const bool perfEnabled = pRulesExt->PhobosFog_Perf_Enabled;

		if constexpr (UseLegacyCliffCoverInRegionMaskPrototype)
		{
			LogCliffProbeActivation(
				alpha,
				cellOverlayWidth,
				cellOverlayHeight,
				paddingX,
				paddingY,
				viewportCellPadding,
				heightAware,
				heightYOffset,
				cliffCover,
				cliffCoverHeight,
				cliffCoverAlpha);
		}

		if (!pRulesExt->PhobosFog_Enabled
			|| !pRulesExt->PhobosFog_DrawExploredOverlay)
		{
			return;
		}

		CliffProbeStats cliffProbeStats {};
		auto const pCliffProbeStats = UseLegacyCliffCoverInRegionMaskPrototype && IsCliffProbeFrame() ? &cliffProbeStats : nullptr;
		auto logCliffProbeSummary = [&]()
		{
			if (pCliffProbeStats)
			{
				LogCliffProbeSummary(*pCliffProbeStats);
			}
		};

		if (alpha <= 0 || !DSurface::Composite || !TacticalClass::Instance)
		{
			logCliffProbeSummary();
			return;
		}

		const auto pViewerHouse = HouseClass::CurrentPlayer;

		if (!IsEligibleViewerHouse(pViewerHouse))
		{
			logCliffProbeSummary();
			return;
		}

		const auto& bounds = DSurface::ViewBounds;

		if (bounds.Width <= 0 || bounds.Height <= 0)
		{
			logCliffProbeSummary();
			return;
		}

		const Point2D corners[] =
		{
			{ bounds.X, bounds.Y },
			{ bounds.X + bounds.Width - 1, bounds.Y },
			{ bounds.X, bounds.Y + bounds.Height - 1 },
			{ bounds.X + bounds.Width - 1, bounds.Y + bounds.Height - 1 }
		};

		auto firstCell = CellClass::Coord2Cell(TacticalClass::Instance->ClientToCoords(corners[0]));
		int minX = firstCell.X;
		int maxX = firstCell.X;
		int minY = firstCell.Y;
		int maxY = firstCell.Y;

		for (int i = 1; i < 4; ++i)
		{
			const auto cell = CellClass::Coord2Cell(TacticalClass::Instance->ClientToCoords(corners[i]));

			if (cell.X < minX)
			{
				minX = cell.X;
			}
			else if (cell.X > maxX)
			{
				maxX = cell.X;
			}

			if (cell.Y < minY)
			{
				minY = cell.Y;
			}
			else if (cell.Y > maxY)
			{
				maxY = cell.Y;
			}
		}

		minX -= viewportCellPadding;
		maxX += viewportCellPadding;
		minY -= viewportCellPadding;
		maxY += viewportCellPadding;

		const int cellCountX = maxX - minX + 1;
		const int cellCountY = maxY - minY + 1;

		if (cellCountX <= 0 || cellCountY <= 0 || cellCountX > MaxOverlayCellsPerFrame / cellCountY)
		{
			logCliffProbeSummary();
			return;
		}

		const CellStruct projectionCell { static_cast<short>(minX), static_cast<short>(minY) };
		const auto [projectionSentinel, projectionVisible] = GetOverlayCellClient(projectionCell, heightAware, heightYOffset);
		(void)projectionVisible;

		const auto cacheDecision = BuildOverlayRegionCacheDecision(
			pViewerHouse,
			UseOverlayRegionCachePrototype,
			bounds,
			firstCell,
			minX,
			maxX,
			minY,
			maxY,
			projectionSentinel,
			alpha,
			cellOverlayWidth,
			cellOverlayHeight,
			paddingX,
			paddingY,
			viewportCellPadding,
			softEdge,
			softEdgeVisibleAlpha,
			fadeInFrames,
			softEdgePadding,
			alphaVariance,
			frontierMode,
			overlayShape,
			maxDrawRects,
			heightAware,
			heightYOffset,
			legacyCliffCoverBypassed,
			UseRegionDilationPrototype,
			UseHeightDiscontinuityFacePrototype,
			VerticalFaceMinDropPixels,
			VerticalFaceMaxDropPixels,
			VerticalFaceFallbackDilationY,
			RegionDilationY,
			RegionCloseGapPixels,
			RegionDilationAlphaNumerator,
			RegionDilationAlphaDenominator);

		OverlayCacheStageADiagnostics cacheDiagnostics {};
		if (perfEnabled)
		{
			cacheDiagnostics = BuildOverlayCacheStageADiagnostics(
				pViewerHouse,
				bounds,
				firstCell,
				minX,
				maxX,
				minY,
				maxY,
				projectionSentinel,
				alpha,
				cellOverlayWidth,
				cellOverlayHeight,
				paddingX,
				paddingY,
				viewportCellPadding,
				softEdge,
				softEdgeVisibleAlpha,
				fadeInFrames,
				softEdgePadding,
				alphaVariance,
				frontierMode,
				overlayShape,
				maxDrawRects,
				heightAware,
				heightYOffset,
				legacyCliffCoverBypassed,
				UseRegionDilationPrototype,
				UseHeightDiscontinuityFacePrototype,
				VerticalFaceMinDropPixels,
				VerticalFaceMaxDropPixels,
				VerticalFaceFallbackDilationY,
				RegionDilationY,
				RegionCloseGapPixels,
				RegionDilationAlphaNumerator,
				RegionDilationAlphaDenominator);
			cacheDiagnostics.OverlayEffectiveVisibilityVersionHash = cacheDecision.Key.OverlayEffectiveVisibilityVersionHash;
			cacheDiagnostics.OverlayViewportHash = cacheDecision.Key.OverlayViewportHash;
			cacheDiagnostics.OverlayConfigHash = cacheDecision.Key.OverlayConfigHash;
			cacheDiagnostics.CacheAllowed = cacheDecision.CacheAllowed;
			cacheDiagnostics.DisabledReason = cacheDecision.DisabledReason;
			cacheDiagnostics.TemporalVisibilityActive = cacheDecision.TemporalVisibilityActive;
			cacheDiagnostics.TemporalFullMapFirstInvalidFrame = cacheDecision.TemporalFullMapFirstInvalidFrame;
			cacheDiagnostics.TemporalCacheFirstInvalidFrame = cacheDecision.TemporalCacheFirstInvalidFrame;
		}

		ColorStruct color { 0, 0, 0 };
		int drawRectCount = 0;

		BuildOverlayHouseInputs(pViewerHouse, OverlayScratchHouseInputs);
		BuildOverlayGeometryInputs(minX, maxX, minY, maxY, heightYOffset, OverlayScratchGeometryInputs);
		// Reuse these probes on rebuild; do not repeat the engine projections per span.
		ActiveOverlayGeometryInputs = &OverlayScratchGeometryInputs;
		ActiveOverlayGeometryMinX = minX - 1;
		ActiveOverlayGeometryMinY = minY - 1;
		ActiveOverlayGeometryWidth = cellCountX + 2;
		ActiveOverlayGeometryHeight = cellCountY + 2;
		struct GeometryScope
		{
			~GeometryScope() { ActiveOverlayGeometryInputs = nullptr; }
		} geometryScope;

		const bool overlayCacheKeyMatches = cacheDecision.CacheEnabled
			&& cacheDecision.CacheAllowed && OverlayFinalRegionCache.Valid
			&& OverlayRegionCacheKeysEqual(OverlayFinalRegionCache.Key, cacheDecision.Key);
		const bool geometryMatches = overlayCacheKeyMatches
			&& OverlayFinalRegionCache.GeometryInputs == OverlayScratchGeometryInputs;
		const bool temporalCacheHitBlockedByExpiry = overlayCacheKeyMatches
			&& OverlayFinalRegionCache.TemporalCacheFirstInvalidFrame > 0
			&& Unsorted::CurrentFrame >= OverlayFinalRegionCache.TemporalCacheFirstInvalidFrame;
		const bool reuseVisibilityInputs = geometryMatches
			&& CanReuseOverlayVisibilityInputs(OverlayFinalRegionCache, OverlayScratchHouseInputs, Unsorted::CurrentFrame);
		int nextAlphaChangeFrame = OverlayFinalRegionCache.NextAlphaChangeFrame;
		if (!reuseVisibilityInputs)
		{
			BuildOverlayVisibilityInputs(pViewerHouse, minX, maxX, minY, maxY,
				alpha, alphaVariance, softEdgeEnabled ? softEdgeVisibleAlpha : 0, fadeInFrames,
				OverlayScratchVisibilityInputs, nextAlphaChangeFrame);
		}
		const bool overlayCacheHit = geometryMatches && (reuseVisibilityInputs
			|| OverlayFinalRegionCache.VisibilityInputs == OverlayScratchVisibilityInputs);

		if (overlayCacheHit)
		{
			OverlayFinalRegionCache.Key = cacheDecision.Key;
			OverlayFinalRegionCache.HouseInputs = OverlayScratchHouseInputs;
			OverlayFinalRegionCache.NextAlphaChangeFrame = nextAlphaChangeFrame;
			OverlayFinalRegionCache.LastValidatedFrame = Unsorted::CurrentFrame;
			OverlayFinalRegionCache.TemporalCacheFirstInvalidFrame = cacheDecision.TemporalCacheFirstInvalidFrame;
			++OverlayRegionCacheTotalHits;
			++OverlayRegionCacheHitStreak;

			const bool regionDrawSucceeded = DrawPreparedRects(
				OverlayFinalRegionCache.MainRects,
				bounds,
				color,
				drawRectCount,
				maxDrawRects)
				&& DrawPreparedRects(OverlayFinalRegionCache.EdgeRects, bounds, color, drawRectCount, maxDrawRects);
			const bool hitMaxDrawRects = !regionDrawSucceeded;
			const size_t unionSpanCount = OverlayFinalRegionCache.FinalRegionUnionSpans.size() + OverlayFinalRegionCache.EdgeUnionSpanCount;
			const int drawRectBatchReduction = std::max(0, ClampDebugCount(unionSpanCount) - drawRectCount);

			OverlayPerfSnapshot perfSnapshot {};
			perfSnapshot.Frame = Unsorted::CurrentFrame;
			perfSnapshot.SourceCells = OverlayFinalRegionCache.Stats.SourceCells;
			perfSnapshot.RawMainSpans = OverlayFinalRegionCache.Stats.RawMainSpans;
			perfSnapshot.MergedMainSpans = OverlayFinalRegionCache.Stats.MergedMainSpans;
			perfSnapshot.PreCompactMainSpans = OverlayFinalRegionCache.Stats.PreCompactMainSpans;
			perfSnapshot.CompactMainSpans = OverlayFinalRegionCache.Stats.CompactMainSpans;
			perfSnapshot.CompactClosedGaps = OverlayFinalRegionCache.Stats.CompactClosedGaps;
			perfSnapshot.HeightFaceEnabled = OverlayFinalRegionCache.Stats.HeightFaceEnabled;
			perfSnapshot.HeightFaceAccepted = OverlayFinalRegionCache.Stats.HeightFaceAccepted;
			perfSnapshot.HeightFaceRawSpans = OverlayFinalRegionCache.Stats.HeightFaceRawSpans;
			perfSnapshot.HeightFaceMaxDrop = OverlayFinalRegionCache.Stats.HeightFaceMaxDrop;
			perfSnapshot.FallbackDilationY = OverlayFinalRegionCache.Stats.FallbackDilationY;
			perfSnapshot.FallbackDilationRawSpans = OverlayFinalRegionCache.Stats.FallbackDilationRawSpans;
			perfSnapshot.FinalMergeInputSpans = OverlayFinalRegionCache.Stats.FinalMergeInputSpans;
			perfSnapshot.FinalMergedSpans = OverlayFinalRegionCache.Stats.FinalMergedSpans;
			perfSnapshot.ClosedGaps = OverlayFinalRegionCache.Stats.ClosedGaps;
			perfSnapshot.UnionSpans = unionSpanCount;
			perfSnapshot.DrawRects = drawRectCount;
			perfSnapshot.DrawRectBatchReduction = drawRectBatchReduction;
			perfSnapshot.DrawRectScratchCapacity = OverlayScratchDrawRects.capacity();
			perfSnapshot.TemplateEnabled = OverlayFinalRegionCache.Stats.TemplateEnabled;
			perfSnapshot.RectTemplateRows = OverlayFinalRegionCache.Stats.RectTemplateRows;
			perfSnapshot.DiamondTemplateRows = OverlayFinalRegionCache.Stats.DiamondTemplateRows;
			perfSnapshot.TemplateCandidateRows = OverlayFinalRegionCache.Stats.TemplateCandidateRows;
			perfSnapshot.TemplateYRejectedRows = OverlayFinalRegionCache.Stats.TemplateYRejectedRows;
			perfSnapshot.TemplateXRejectedRows = OverlayFinalRegionCache.Stats.TemplateXRejectedRows;
			perfSnapshot.TemplateVisibleRows = OverlayFinalRegionCache.Stats.TemplateVisibleRows;
			perfSnapshot.TemplateInstantiatedRows = OverlayFinalRegionCache.Stats.TemplateInstantiatedRows;
			perfSnapshot.TemplateClippedRows = OverlayFinalRegionCache.Stats.TemplateClippedRows;
			perfSnapshot.TemplateFallbacks = OverlayFinalRegionCache.Stats.TemplateFallbacks;
			perfSnapshot.MaxDrawRects = maxDrawRects;
			perfSnapshot.HitMaxDrawRects = hitMaxDrawRects;
			perfSnapshot.SoftEdgeEnabled = OverlayFinalRegionCache.Stats.SoftEdgeEnabled;
			perfSnapshot.LegacyCliffCoverBypassed = OverlayFinalRegionCache.Stats.LegacyCliffCoverBypassed;
			perfSnapshot.OverlayCacheEnabled = cacheDecision.CacheEnabled;
			perfSnapshot.PhobosFogStateVersion = cacheDiagnostics.PhobosFogStateVersion;
			perfSnapshot.OverlayEffectiveVersion = cacheDiagnostics.OverlayEffectiveVersion;
			perfSnapshot.EffectiveVisibilityVersionHash = cacheDiagnostics.EffectiveVisibilityVersionHash;
			perfSnapshot.OverlayEffectiveVisibilityVersionHash = cacheDecision.Key.OverlayEffectiveVisibilityVersionHash;
			perfSnapshot.OverlayViewportHash = cacheDecision.Key.OverlayViewportHash;
			perfSnapshot.OverlayConfigHash = cacheDecision.Key.OverlayConfigHash;
			perfSnapshot.OverlayCacheAllowed = cacheDecision.CacheAllowed;
			perfSnapshot.OverlayCacheDisabledReason = cacheDecision.DisabledReason;
			perfSnapshot.OverlayCacheHit = true;
			perfSnapshot.OverlayCacheMiss = false;
			perfSnapshot.OverlayCacheRebuild = false;
			perfSnapshot.OverlayCacheHitCount = OverlayRegionCacheTotalHits;
			perfSnapshot.OverlayCacheMissCount = OverlayRegionCacheTotalMisses;
			perfSnapshot.OverlayCacheRebuildCount = OverlayRegionCacheTotalRebuilds;
			perfSnapshot.OverlayCacheHitStreak = OverlayRegionCacheHitStreak;
			perfSnapshot.OverlayCacheCachedFinalSpans = OverlayFinalRegionCache.FinalRegionUnionSpans.size();
			perfSnapshot.TemporalVisibilityActive = cacheDecision.TemporalVisibilityActive;
			perfSnapshot.TemporalCellExpiryScanSkipped = true;
			perfSnapshot.TemporalFullMapFirstInvalidFrame = cacheDecision.TemporalFullMapFirstInvalidFrame;
			perfSnapshot.TemporalCacheFirstInvalidFrame = OverlayFinalRegionCache.TemporalCacheFirstInvalidFrame;
			perfSnapshot.TemporalCacheExpiresInFrames = GetTemporalCacheExpiresInFrames(
				perfSnapshot.TemporalCacheFirstInvalidFrame,
				Unsorted::CurrentFrame);
			perfSnapshot.TemporalCacheHitBlockedByExpiry = false;
			perfSnapshot.StageAOnly = false;
			perfSnapshot.StateVersionTouchCount = cacheDiagnostics.StateVersionTouchCount;
			perfSnapshot.OverlayEffectiveTouchCount = cacheDiagnostics.OverlayEffectiveTouchCount;
			perfSnapshot.OverlayEffectiveBatchTouchedCells = cacheDiagnostics.OverlayEffectiveBatchTouchedCells;
			perfSnapshot.OverlayEffectiveBatchChangedCells = cacheDiagnostics.OverlayEffectiveBatchChangedCells;

			for (size_t i = 0; i < HouseExt::PhobosFogStateTouchReasonCount; ++i)
			{
				perfSnapshot.StateVersionTouchReasons[i] = cacheDiagnostics.StateVersionTouchReasons[i];
			}

			perfSnapshot.GeometryProbes = OverlayFrameGeometryProbes;
			perfSnapshot.VisibilityQueries = OverlayFrameVisibilityQueries;
			LogOverlayPerfSummary(perfSnapshot);

			if (!regionDrawSucceeded)
			{
				OverlayFinalRegionCache.Valid = false;
				return;
			}

			return;
		}

		const bool overlayCacheMiss = cacheDecision.CacheEnabled && cacheDecision.CacheAllowed;

		if (overlayCacheMiss)
		{
			++OverlayRegionCacheTotalMisses;
		}

		++OverlayRegionCacheTotalRebuilds;
		OverlayRegionCacheHitStreak = 0;

		int sourceCells = 0;
		auto& edgeSpans = OverlayScratchEdgeSpans;
		auto& cliffSpans = OverlayScratchCliffSpans;
		auto& verticalFaceSpans = OverlayScratchVerticalFaceSpans;
		HeightFaceStats heightFaceStats {};
		OverlayCellShapeTemplateStats shapeTemplateStats {};
		shapeTemplateStats.Enabled = UseCellShapeGeometryTemplatePrototype;

		edgeSpans.clear();
		cliffSpans.clear();
		verticalFaceSpans.clear();

		edgeSpans.reserve(static_cast<size_t>(cellCountX * cellCountY));
		if constexpr (UseLegacyCliffCoverInRegionMaskPrototype)
		{
			cliffSpans.reserve(static_cast<size_t>(cellCountX * cellCountY));
		}
		if constexpr (UseHeightDiscontinuityFacePrototype)
		{
			verticalFaceSpans.reserve(static_cast<size_t>(cellCountX * cellCountY));
		}
		if constexpr (UseCellShapeGeometryTemplatePrototype)
		{
			auto& rectTemplate = GetOverlayCellShapeTemplate(cellOverlayWidth, cellOverlayHeight, false);
			auto& diamondTemplate = GetOverlayCellShapeTemplate(cellOverlayWidth, cellOverlayHeight, true);
			shapeTemplateStats.RectTemplateRows = rectTemplate.Rows.size();
			shapeTemplateStats.DiamondTemplateRows = diamondTemplate.Rows.size();
		}

		for (int y = minY; y <= maxY; ++y)
		{
			for (int x = minX; x <= maxX; ++x)
			{
				const CellStruct cell { static_cast<short>(x), static_cast<short>(y) };
				int cellIndex = -1;

				if (!IsExploredOverlayCell(pViewerHouse, cell, cellIndex))
				{
					continue;
				}

				++sourceCells;

				const auto frontier = ClassifyExploredOverlayFrontier(pViewerHouse, cell, frontierMode);
				const auto [center, visible] = GetOverlayCellClient(cell, heightAware, heightYOffset);
				(void)visible;

				RectangleStruct rect
				{
					center.X - cellOverlayWidth / 2 - paddingX,
					center.Y - cellOverlayHeight / 2 - paddingY,
					cellOverlayWidth,
					cellOverlayHeight
				};

				const bool useDiamond = overlayShape == 2 || (overlayShape == 1 && frontier.IsFrontier);
				const int currentAlpha = ApplyFadeInAlpha(ComputeCellAlpha(alpha, cellIndex, alphaVariance), pViewerHouse, cellIndex, fadeInFrames);

				if constexpr (UseHeightDiscontinuityFacePrototype)
				{
					if (currentAlpha > 0)
					{
						constexpr CellStruct HeightFaceOffsets[] =
						{
							{ 1, 0 },
							{ 0, 1 }
						};

						for (const auto& offset : HeightFaceOffsets)
						{
							TryAddHeightDiscontinuityFaceSpans(
								verticalFaceSpans,
								bounds,
								currentAlpha,
								cell,
								offset,
								heightYOffset,
								cellOverlayWidth,
								cellOverlayHeight,
								paddingX,
								paddingY,
								VerticalFaceMinDropPixels,
								VerticalFaceMaxDropPixels,
								heightFaceStats);
						}
					}
				}

				if (softEdge && softEdgeVisibleAlpha > 0 && softEdgePadding > 0 && frontier.HasVisibleNeighbor)
				{
					const int currentSoftEdgeAlpha = ApplyFadeInAlpha(softEdgeVisibleAlpha, pViewerHouse, cellIndex, fadeInFrames);

					if (currentSoftEdgeAlpha > 0)
					{
						constexpr CellStruct EdgeNeighborOffsets[] =
						{
							{ 0, -1 },
							{ 0, 1 },
							{ -1, 0 },
							{ 1, 0 }
						};

						for (int i = 0; i < 4; ++i)
						{
							if (!frontier.HasVisibleNeighbor4[i])
							{
								continue;
							}

							if (useDiamond)
							{
								const CellStruct neighbor
								{
									static_cast<short>(cell.X + EdgeNeighborOffsets[i].X),
									static_cast<short>(cell.Y + EdgeNeighborOffsets[i].Y)
								};
								const auto [neighborCenter, neighborOnScreen] = GetOverlayCellClient(neighbor, heightAware, heightYOffset);
								(void)neighborOnScreen;
								const RectangleStruct neighborRect
								{
									neighborCenter.X - cellOverlayWidth / 2 - paddingX,
									neighborCenter.Y - cellOverlayHeight / 2 - paddingY,
									cellOverlayWidth,
									cellOverlayHeight
								};

								AddDiamondSoftEdgeTowardNeighborSpans(
									edgeSpans,
									neighborRect,
									bounds,
									currentSoftEdgeAlpha,
									softEdgePadding,
									neighborCenter,
									center);
							}
							else
							{
								const int oppositeDirection = i ^ 1;
								const CellStruct neighbor
								{
									static_cast<short>(cell.X + EdgeNeighborOffsets[i].X),
									static_cast<short>(cell.Y + EdgeNeighborOffsets[i].Y)
								};
								const auto [neighborCenter, neighborOnScreen] = GetOverlayCellClient(neighbor, heightAware, heightYOffset);
								(void)neighborOnScreen;
								const RectangleStruct neighborRect
								{
									neighborCenter.X - cellOverlayWidth / 2 - paddingX,
									neighborCenter.Y - cellOverlayHeight / 2 - paddingY,
									cellOverlayWidth,
									cellOverlayHeight
								};

								AddRectangularSoftEdgeSpans(
									edgeSpans,
									neighborRect,
									bounds,
									currentSoftEdgeAlpha,
									softEdgePadding,
									oppositeDirection);
							}
						}
					}
				}

				if constexpr (UseLegacyCliffCoverInRegionMaskPrototype)
				{
					if (legacyCliffCoverRequested)
					{
						std::array<CliffCoverEdge, 4> cliffEdges {};
						int cliffEdgeCount = 0;

						CollectCliffCoverEdges(
							pViewerHouse,
							cell,
							heightYOffset,
							cellOverlayWidth,
							cellOverlayHeight,
							paddingX,
							paddingY,
							cliffCoverHeight,
							cliffEdges,
							cliffEdgeCount,
							pCliffProbeStats);

						if (cliffEdgeCount > 0)
						{
							const int currentCliffCoverAlpha = ApplyFadeInAlpha(ComputeCellAlpha(cliffCoverAlpha, cellIndex, alphaVariance), pViewerHouse, cellIndex, fadeInFrames);

							if (currentCliffCoverAlpha > 0)
							{
								for (int i = 0; i < cliffEdgeCount; ++i)
								{
									const auto& edge = cliffEdges[static_cast<size_t>(i)];
									const int edgeAlpha = edge.NeighborKind == OverlayCellKind::Visible
										? ClampAlpha(currentCliffCoverAlpha * 2 / 3)
										: currentCliffCoverAlpha;

									AddCliffFaceStripSpans(
										cliffSpans,
										bounds,
										edgeAlpha,
										edge);
								}
							}
						}
					}
				}
			}
		}

		logCliffProbeSummary();

		auto& mainUnionSpans = OverlayScratchMainUnionSpans;
		mainUnionSpans.clear();
		size_t rowBucketInputSpans = 0;
		int rowBucketNonEmptyRows = 0;
		BuildTemplateRowBucketMainUnionSpans(
			pViewerHouse,
			bounds,
			minX,
			maxX,
			minY,
			maxY,
			heightAware,
			heightYOffset,
			cellOverlayWidth,
			cellOverlayHeight,
			paddingX,
			paddingY,
			alpha,
			alphaVariance,
			fadeInFrames,
			frontierMode,
			overlayShape,
			shapeTemplateStats,
			mainUnionSpans,
			rowBucketInputSpans,
			rowBucketNonEmptyRows);

		auto& compactMainInputSpans = OverlayScratchCompactMainInputSpans;
		auto& compactMainUnionSpans = OverlayScratchCompactMainUnionSpans;
		auto& dilationSpans = OverlayScratchDilationSpans;
		auto& finalRegionSpans = OverlayScratchFinalRegionSpans;
		auto& finalRegionUnionSpans = OverlayScratchFinalRegionUnionSpans;
		compactMainInputSpans.clear();
		compactMainUnionSpans.clear();
		dilationSpans.clear();
		finalRegionSpans.clear();
		finalRegionUnionSpans.clear();
		int compactClosedGaps = 0;
		int closedGaps = 0;

		compactMainInputSpans.reserve(mainUnionSpans.size());
		compactMainInputSpans.insert(compactMainInputSpans.end(), mainUnionSpans.begin(), mainUnionSpans.end());
		MergeRegionSpansWithCloseGap(compactMainInputSpans, compactMainUnionSpans, RegionCloseGapPixels, compactClosedGaps);

		if constexpr (UseRegionDilationPrototype)
		{
			GenerateDownDilationSpans(
				compactMainUnionSpans,
				bounds,
				RegionDilationY,
				RegionDilationAlphaNumerator,
				RegionDilationAlphaDenominator,
				dilationSpans);
		}

		finalRegionSpans.reserve(compactMainUnionSpans.size() + verticalFaceSpans.size() + dilationSpans.size());
		finalRegionSpans.insert(finalRegionSpans.end(), compactMainUnionSpans.begin(), compactMainUnionSpans.end());
		finalRegionSpans.insert(finalRegionSpans.end(), verticalFaceSpans.begin(), verticalFaceSpans.end());
		finalRegionSpans.insert(finalRegionSpans.end(), dilationSpans.begin(), dilationSpans.end());
		const size_t finalMergeInputSpans = finalRegionSpans.size();

		MergeRegionSpansWithCloseGap(finalRegionSpans, finalRegionUnionSpans, RegionCloseGapPixels, closedGaps);

		OverlayRegionCacheStats rebuildStats {};
		rebuildStats.SourceCells = sourceCells;
		rebuildStats.RawMainSpans = rowBucketInputSpans;
		rebuildStats.MergedMainSpans = mainUnionSpans.size();
		rebuildStats.PreCompactMainSpans = mainUnionSpans.size();
		rebuildStats.CompactMainSpans = compactMainUnionSpans.size();
		rebuildStats.CompactClosedGaps = compactClosedGaps;
		rebuildStats.HeightFaceEnabled = UseHeightDiscontinuityFacePrototype;
		rebuildStats.HeightFaceAccepted = heightFaceStats.Accepted;
		rebuildStats.HeightFaceRawSpans = verticalFaceSpans.size();
		rebuildStats.HeightFaceMaxDrop = heightFaceStats.MaxDrop;
		rebuildStats.FallbackDilationY = VerticalFaceFallbackDilationY;
		rebuildStats.FallbackDilationRawSpans = dilationSpans.size();
		rebuildStats.FinalMergeInputSpans = finalMergeInputSpans;
		rebuildStats.FinalMergedSpans = finalRegionUnionSpans.size();
		rebuildStats.ClosedGaps = closedGaps;
		rebuildStats.SoftEdgeEnabled = softEdgeEnabled;
		rebuildStats.LegacyCliffCoverBypassed = legacyCliffCoverBypassed;
		rebuildStats.TemplateEnabled = shapeTemplateStats.Enabled;
		rebuildStats.RectTemplateRows = shapeTemplateStats.RectTemplateRows;
		rebuildStats.DiamondTemplateRows = shapeTemplateStats.DiamondTemplateRows;
		rebuildStats.TemplateCandidateRows = shapeTemplateStats.CandidateRows;
		rebuildStats.TemplateYRejectedRows = shapeTemplateStats.YRejectedRows;
		rebuildStats.TemplateXRejectedRows = shapeTemplateStats.XRejectedRows;
		rebuildStats.TemplateVisibleRows = shapeTemplateStats.VisibleRows;
		rebuildStats.TemplateInstantiatedRows = shapeTemplateStats.InstantiatedRows;
		rebuildStats.TemplateClippedRows = shapeTemplateStats.ClippedRows;
		rebuildStats.TemplateFallbacks = shapeTemplateStats.Fallbacks;

		if (cacheDecision.CacheEnabled && cacheDecision.CacheAllowed)
		{
			OverlayFinalRegionCache.Valid = true;
			OverlayFinalRegionCache.GeometryInputs = OverlayScratchGeometryInputs;
			OverlayFinalRegionCache.VisibilityInputs = OverlayScratchVisibilityInputs;
			OverlayFinalRegionCache.HouseInputs = OverlayScratchHouseInputs;
			OverlayFinalRegionCache.NextAlphaChangeFrame = nextAlphaChangeFrame;
			OverlayFinalRegionCache.LastValidatedFrame = Unsorted::CurrentFrame;
			OverlayFinalRegionCache.Key = cacheDecision.Key;
			OverlayFinalRegionCache.FinalRegionUnionSpans.clear();
			OverlayFinalRegionCache.FinalRegionUnionSpans.insert(
				OverlayFinalRegionCache.FinalRegionUnionSpans.end(),
				finalRegionUnionSpans.begin(),
				finalRegionUnionSpans.end());
			BuildCoalescedRects(finalRegionUnionSpans, OverlayFinalRegionCache.MainRects);
			OverlayFinalRegionCache.EdgeRects.clear();
			OverlayFinalRegionCache.EdgeUnionSpanCount = 0;
			OverlayFinalRegionCache.Stats = rebuildStats;
			OverlayFinalRegionCache.TemporalCacheFirstInvalidFrame = cacheDecision.TemporalCacheFirstInvalidFrame;
		}

		const bool cachePreparedRects = cacheDecision.CacheEnabled && cacheDecision.CacheAllowed;
		bool regionDrawSucceeded = cachePreparedRects
			? DrawPreparedRects(OverlayFinalRegionCache.MainRects, bounds, color, drawRectCount, maxDrawRects)
			: DrawUnionSpans(finalRegionUnionSpans, bounds, color, drawRectCount, maxDrawRects);
		size_t unionSpanCount = finalRegionUnionSpans.size();

		// Keep pass order and one shared rectangle budget. Never submit edges after
		// an incomplete main pass, and cache an empty edge pass as empty as well.
		if (regionDrawSucceeded && !edgeSpans.empty())
		{
			auto& edgeUnionSpans = OverlayScratchEdgeUnionSpans;
			MergeSpansToUnionSpans(edgeSpans, edgeUnionSpans);
			unionSpanCount += edgeUnionSpans.size();
			if (cachePreparedRects)
			{
				BuildCoalescedRects(edgeUnionSpans, OverlayFinalRegionCache.EdgeRects);
				OverlayFinalRegionCache.EdgeUnionSpanCount = edgeUnionSpans.size();
				regionDrawSucceeded = DrawPreparedRects(OverlayFinalRegionCache.EdgeRects, bounds, color, drawRectCount, maxDrawRects);
			}
			else
			{
				regionDrawSucceeded = DrawUnionSpans(edgeUnionSpans, bounds, color, drawRectCount, maxDrawRects);
			}
		}

		if constexpr (UseLegacyCliffCoverInRegionMaskPrototype)
		{
			if (regionDrawSucceeded && !cliffSpans.empty())
			{
				auto& cliffUnionSpans = OverlayScratchCliffUnionSpans;
				MergeSpansToUnionSpans(cliffSpans, cliffUnionSpans);
				unionSpanCount += cliffUnionSpans.size();
				regionDrawSucceeded = DrawUnionSpans(cliffUnionSpans, bounds, color, drawRectCount, maxDrawRects);
			}
		}

		const bool hitMaxDrawRects = !regionDrawSucceeded;
		const int drawRectBatchReduction = std::max(0, ClampDebugCount(unionSpanCount) - drawRectCount);

		rebuildStats.DrawRects = drawRectCount;
		rebuildStats.HitMaxDrawRects = hitMaxDrawRects;

		if (cacheDecision.CacheEnabled && cacheDecision.CacheAllowed)
		{
			if (regionDrawSucceeded)
			{
				OverlayFinalRegionCache.Stats.DrawRects = drawRectCount;
				OverlayFinalRegionCache.Stats.HitMaxDrawRects = hitMaxDrawRects;
			}
			else
			{
				OverlayFinalRegionCache.Valid = false;
			}
		}

		LogRegionMaskSummary(
			sourceCells,
			rowBucketInputSpans,
			mainUnionSpans.size(),
			dilationEnabled,
			RegionDilationY,
			dilationSpans.size(),
			UseHeightDiscontinuityFacePrototype,
			2,
			heightFaceStats.Candidates,
			heightFaceStats.Accepted,
			verticalFaceSpans.size(),
			heightFaceStats.MinDrop,
			heightFaceStats.MaxDrop,
			VerticalFaceFallbackDilationY,
			dilationSpans.size(),
			finalRegionUnionSpans.size(),
			closedGaps,
			drawRectCount,
			maxDrawRects,
			softEdgeEnabled,
			legacyCliffCoverBypassed,
			hitMaxDrawRects);

		OverlayPerfSnapshot perfSnapshot {};
		perfSnapshot.Frame = Unsorted::CurrentFrame;
		perfSnapshot.SourceCells = sourceCells;
		perfSnapshot.RawMainSpans = rowBucketInputSpans;
		perfSnapshot.MergedMainSpans = mainUnionSpans.size();
		perfSnapshot.PreCompactMainSpans = mainUnionSpans.size();
		perfSnapshot.CompactMainSpans = compactMainUnionSpans.size();
		perfSnapshot.CompactClosedGaps = compactClosedGaps;
		perfSnapshot.HeightFaceEnabled = UseHeightDiscontinuityFacePrototype;
		perfSnapshot.HeightFaceAccepted = heightFaceStats.Accepted;
		perfSnapshot.HeightFaceRawSpans = verticalFaceSpans.size();
		perfSnapshot.HeightFaceMaxDrop = heightFaceStats.MaxDrop;
		perfSnapshot.FallbackDilationY = VerticalFaceFallbackDilationY;
		perfSnapshot.FallbackDilationRawSpans = dilationSpans.size();
		perfSnapshot.FinalMergeInputSpans = finalMergeInputSpans;
		perfSnapshot.FinalMergedSpans = finalRegionUnionSpans.size();
		perfSnapshot.ClosedGaps = closedGaps;
		perfSnapshot.UnionSpans = unionSpanCount;
		perfSnapshot.DrawRects = drawRectCount;
		perfSnapshot.DrawRectBatchReduction = drawRectBatchReduction;
		perfSnapshot.DrawRectScratchCapacity = OverlayScratchDrawRects.capacity();
		perfSnapshot.RowBucketInputSpans = rowBucketInputSpans;
		perfSnapshot.RowBucketNonEmptyRows = rowBucketNonEmptyRows;
		perfSnapshot.RowBucketMainUnionSpans = mainUnionSpans.size();
		perfSnapshot.TemplateEnabled = shapeTemplateStats.Enabled;
		perfSnapshot.RectTemplateRows = shapeTemplateStats.RectTemplateRows;
		perfSnapshot.DiamondTemplateRows = shapeTemplateStats.DiamondTemplateRows;
		perfSnapshot.TemplateCandidateRows = shapeTemplateStats.CandidateRows;
		perfSnapshot.TemplateYRejectedRows = shapeTemplateStats.YRejectedRows;
		perfSnapshot.TemplateXRejectedRows = shapeTemplateStats.XRejectedRows;
		perfSnapshot.TemplateVisibleRows = shapeTemplateStats.VisibleRows;
		perfSnapshot.TemplateInstantiatedRows = shapeTemplateStats.InstantiatedRows;
		perfSnapshot.TemplateClippedRows = shapeTemplateStats.ClippedRows;
		perfSnapshot.TemplateFallbacks = shapeTemplateStats.Fallbacks;
		perfSnapshot.MaxDrawRects = maxDrawRects;
		perfSnapshot.HitMaxDrawRects = hitMaxDrawRects;
		perfSnapshot.SoftEdgeEnabled = softEdgeEnabled;
		perfSnapshot.LegacyCliffCoverBypassed = legacyCliffCoverBypassed;
		perfSnapshot.OverlayCacheEnabled = cacheDecision.CacheEnabled;
		perfSnapshot.PhobosFogStateVersion = cacheDiagnostics.PhobosFogStateVersion;
		perfSnapshot.OverlayEffectiveVersion = cacheDiagnostics.OverlayEffectiveVersion;
		perfSnapshot.EffectiveVisibilityVersionHash = cacheDiagnostics.EffectiveVisibilityVersionHash;
		perfSnapshot.OverlayEffectiveVisibilityVersionHash = cacheDecision.Key.OverlayEffectiveVisibilityVersionHash;
		perfSnapshot.OverlayViewportHash = cacheDecision.Key.OverlayViewportHash;
		perfSnapshot.OverlayConfigHash = cacheDecision.Key.OverlayConfigHash;
		perfSnapshot.OverlayCacheAllowed = cacheDecision.CacheAllowed;
		perfSnapshot.OverlayCacheDisabledReason = cacheDecision.DisabledReason;
		perfSnapshot.OverlayCacheHit = false;
		perfSnapshot.OverlayCacheMiss = overlayCacheMiss;
		perfSnapshot.OverlayCacheRebuild = true;
		perfSnapshot.OverlayCacheHitCount = OverlayRegionCacheTotalHits;
		perfSnapshot.OverlayCacheMissCount = OverlayRegionCacheTotalMisses;
		perfSnapshot.OverlayCacheRebuildCount = OverlayRegionCacheTotalRebuilds;
		perfSnapshot.OverlayCacheHitStreak = OverlayRegionCacheHitStreak;
		perfSnapshot.OverlayCacheCachedFinalSpans = OverlayFinalRegionCache.Valid ? OverlayFinalRegionCache.FinalRegionUnionSpans.size() : 0;
		perfSnapshot.TemporalVisibilityActive = cacheDecision.TemporalVisibilityActive;
		perfSnapshot.TemporalCellExpiryScanSkipped = true;
		perfSnapshot.TemporalFullMapFirstInvalidFrame = cacheDecision.TemporalFullMapFirstInvalidFrame;
		perfSnapshot.TemporalCacheFirstInvalidFrame = cacheDecision.TemporalCacheFirstInvalidFrame;
		perfSnapshot.TemporalCacheExpiresInFrames = GetTemporalCacheExpiresInFrames(
			perfSnapshot.TemporalCacheFirstInvalidFrame,
			Unsorted::CurrentFrame);
		perfSnapshot.TemporalCacheHitBlockedByExpiry = temporalCacheHitBlockedByExpiry;
		perfSnapshot.StageAOnly = false;
		perfSnapshot.StateVersionTouchCount = cacheDiagnostics.StateVersionTouchCount;
		perfSnapshot.OverlayEffectiveTouchCount = cacheDiagnostics.OverlayEffectiveTouchCount;
		perfSnapshot.OverlayEffectiveBatchTouchedCells = cacheDiagnostics.OverlayEffectiveBatchTouchedCells;
		perfSnapshot.OverlayEffectiveBatchChangedCells = cacheDiagnostics.OverlayEffectiveBatchChangedCells;

		for (size_t i = 0; i < HouseExt::PhobosFogStateTouchReasonCount; ++i)
		{
			perfSnapshot.StateVersionTouchReasons[i] = cacheDiagnostics.StateVersionTouchReasons[i];
		}

		perfSnapshot.GeometryProbes = OverlayFrameGeometryProbes;
		perfSnapshot.VisibilityQueries = OverlayFrameVisibilityQueries;
		LogOverlayPerfSummary(perfSnapshot);
	}
}

// Loads the veinhole monster art
// Call removed from YR by WW
DEFINE_HOOK(0x4AD097, DisplayClass_ReadIni_LoadVeinholeArt, 0x6)
{
	const int theater = static_cast<int>(ScenarioClass::Instance->Theater);
	VeinholeMonsterClass::LoadVeinholeArt(theater);

	return 0;
}

// Applies damage to the veinhole monster
DEFINE_HOOK(0x489671, Damage_at_Cell_Update_Veinhole, 0x6)
{
	GET(OverlayTypeClass*, pOverlay, EAX);
	GET(WarheadTypeClass*, pWH, ESI);
	GET_STACK(CellStruct, pCell, STACK_OFFSET(0xE0, -0x4C));
	GET_STACK(int, damage, STACK_OFFSET(0xE0, -0xBC));
	GET_STACK(ObjectClass*, pAttacker, STACK_OFFSET(0xE0, 0x8));
	GET_STACK(HouseClass*, pAttackingHouse, STACK_OFFSET(0xE0, 0x14));

	if (pOverlay->IsVeinholeMonster)
	{
		if (VeinholeMonsterClass* pVeinhole = VeinholeMonsterClass::GetVeinholeMonsterFrom(&pCell))
			pVeinhole->ReceiveDamage(&damage, 0, pWH, pAttacker, false, false, pAttackingHouse);
	}

	return 0;
}

DEFINE_HOOK(0x6D4656, TacticalClass_Draw_Veinhole, 0x5)
{
	enum { ContinueDraw = 0x6D465B };

	VeinholeMonsterClass::DrawAll();
	IonBlastClass::DrawAll();
	PhobosFogExploredOverlay::Draw();

	return ContinueDraw;
}

DEFINE_HOOK(0x5349A5, Map_ClearVectors_Veinhole, 0x5)
{
	VeinholeMonsterClass::DeleteAll();
	VeinholeMonsterClass::DeleteVeinholeGrowthData();
	return 0;
}

// DEFINE_HOOK(0x55B4E1, LogicClass_Update_Veinhole, 0x5) // Goto ScenarioExt

// Handles the veins' attack animation
DEFINE_HOOK(0x4243BC, AnimClass_Update_VeinholeAttack, 0x6)
{
	GET(AnimClass*, pAnim, ESI);

	if (pAnim->Type->IsVeins)
		AnimExt::VeinAttackAI(pAnim);

	return 0;
}

///
/// Weeder
///

// These 2 I am not sure, maybe they have smth to do with AI, maybe they are for the unit queue at the refinery
DEFINE_HOOK(0x736823, UnitClass_Update_WeederMissionMove, 0x6)
{
	enum
	{
		Continue = 0x736831,
		Skip = 0x736981
	};

	GET(UnitTypeClass*, pUnitType, EAX);

	if (pUnitType->Harvester || pUnitType->Weeder)
		return Continue;

	return Skip;
}

DEFINE_HOOK(0x7368C6, UnitClass_Update_WeederMissionMove2, 0x6)
{
	enum
	{
		Continue = 0x7368D4,
		Skip = 0x736981
	};

	GET(BuildingTypeClass*, pBuildingType, EDX);

	if (pBuildingType->Refinery || pBuildingType->Weeder)
		return Continue;

	return Skip;
}

// Not sure if necessary
/*
// These 2 have something to do with ZAdjustment when unloading
DEFINE_HOOK(0x7043E7, TechnoClass_Get_ZAdjustment_Weeder, 0x6)
{
	enum
	{
		Continue = 0x7043F1,
		Skip = 0x704421
	};

	GET(UnitTypeClass*, pUnitType, ECX);

	if (pUnitType->Harvester || pUnitType->Weeder)
		return Continue;

	return Skip;
}

DEFINE_HOOK(0x70440C, TechnoClass_Get_ZAdjustment_Weeder2, 0x6)
{
	enum
	{
		Continue = 0x704416,
		Skip = 0x704421
	};

	GET(BuildingTypeClass*, pBuildingType, EAX);

	if (pBuildingType->Refinery || pBuildingType->Weeder)
		return Continue;

	return Skip;
}

DEFINE_HOOK(0x741C32, UnitClass_SetDestination_SpecialAnim_Weeder, 0x6)
{
	enum
	{
		CheckSpecialAnimExists = 0x741C3C,
		Skip = 0x741C4F
	};

	GET(UnitTypeClass*, pUnitType, ECX);

	if (pUnitType->Harvester || pUnitType->Weeder)
		return CheckSpecialAnimExists;

	return Skip;
}
*/

DEFINE_HOOK(0x73D0DB, UnitClass_DrawAt_Weeder_Oregath, 0x6)
{
	enum
	{
		DrawOregath = 0x73D0E9,
		Skip = 0x73D298
	};

	GET(UnitClass*, pUnit, ESI);

	if (pUnit->IsHarvesting)
		return DrawOregath;

	const auto pType = pUnit->Type;

	if (pType->Harvester || pType->Weeder)
		return DrawOregath;

	return Skip;
}
/*
DEFINE_HOOK(0x73D2A6, UnitClass_DrawAt_Weeder_UnloadingClass, 0x6)
{
	enum
	{
		ShowUnloadingClass = 0x73D2B0,
		Skip = 0x73D2CA
	};

	GET(UnitTypeClass*, pUnitType, EAX);

	if (pUnitType->Harvester || pUnitType->Weeder)
		return ShowUnloadingClass;

	return Skip;
}
*/

// Enables the weeder to harvest veins
DEFINE_HOOK(0x73D49E, UnitClass_Harvesting_Weeder, 0x7)
{
	enum
	{
		Harvest = 0x73D4DA,
		Skip = 0x73D5FE
	};

	GET(UnitClass*, pUnit, ESI);
	GET(CellClass*, pCell, EBP);
	constexpr unsigned char weedOverlayData = 0x30;

	const auto pType = pUnit->Type;
	const auto landType = pCell->LandType;
	const bool harvesterCanHarvest = pType->Harvester && landType == LandType::Tiberium;
	const bool weederCanWeed = pType->Weeder && landType == LandType::Weeds && pCell->OverlayData >= weedOverlayData;

	if ((harvesterCanHarvest || weederCanWeed) && pUnit->GetStoragePercentage() < 1.0)
		return Harvest;

	return Skip;
}

// Not sure if necessary
/*
DEFINE_HOOK(0x73E005, UnitClass_Unload_WeederAnim, 0x6)
{
	enum
	{
		ProceedWithAnim = 0x73E013,
		Skip = 0x73E093
	};

	GET(UnitTypeClass*, pUnitType, ECX);

	if (pUnitType->Harvester || pUnitType->Weeder)
		return ProceedWithAnim;

	return Skip;
}
*/

// This lets the weeder actually enter the waste facility and unload
// WW removed weeders from this check in YR
DEFINE_HOOK(0x43C788, BuildingClass_ReceivedRadioCommand_Weeder_CompleteEnter, 0x6)
{
	enum
	{
		CompleteEnter = 0x43C796,
		Skip = 0x43CE43
	};

	GET(BuildingTypeClass*, pBuildingType, EAX);

	if (pBuildingType->DockUnload || pBuildingType->Weeder)
		return CompleteEnter;

	return Skip;
}

// This assigns the weeder to the "Harvest" mission when it is granted as a free unit
// Ares made the weeder receive the "Guard" command instead
DEFINE_HOOK(0x446EAD, BuildingClass_GrandOpening_FreeWeeder_Mission, 0x6)
{
	GET(UnitClass*, pUnit, EDI);

	if (pUnit->Type->Weeder)
		pUnit->ForceMission(Mission::Harvest);

	pUnit->NextMission();

	return 0x446EB7;
}

// Teleport cooldown for weeders
// DEFINE_HOOK(0x719580, TeleportLocomotion_Weeder, 0x6) //Goto Hooks.Teleport.cpp

// DockUnload bypass for Weeders when teleporting
DEFINE_HOOK(0x7424BD, UnitClass_AssignDestination_Weeder_Teleport, 0x6)
{
	GET(BuildingTypeClass*, pDestination, ECX);

	return pDestination->DockUnload || pDestination->Weeder ? 0x7424CB : 0x7425DB;
}

//// Skip check for Weeder so that weeders go through teleport stuff
//DEFINE_JUMP(LJMP, 0x73E844, 0x73E793)
//
//
//DEFINE_HOOK(0x73E84A, UnitClass_Mission_Harvest, 0x6)
//{
//	GET(UnitClass*, pUnit, EBP);
//
//	bool isOnTiberium;
//	if (pUnit->Type->Weeder)
//		isOnTiberium = pUnit->MoveToWeed(RulesClass::Instance->TiberiumLongScan / Unsorted::LeptonsPerCell);
//	else
//		isOnTiberium = pUnit->MoveToTiberium(RulesClass::Instance->TiberiumLongScan / Unsorted::LeptonsPerCell);
//
//	R->EBX(isOnTiberium);
//	return 0x73E86B;
//}

DEFINE_HOOK(0x73E9A0, UnitClass_Weeder_StopHarvesting, 0x6)
{
	enum
	{
		StopHarvesting = 0x73E9CA,
		Skip = 0x73EA8D
	};

	GET(UnitClass*, pUnit, EBP);

	const auto pType = pUnit->Type;

	if ((pType->Harvester || pType->Weeder) && pUnit->GetStoragePercentage() == 1.0)
	{
		return StopHarvesting;
	}

	return Skip;
}
