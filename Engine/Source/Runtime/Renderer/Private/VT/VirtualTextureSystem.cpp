// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureSystem.h"

#include "AllocatedVirtualTexture.h"
#include "HAL/IConsoleManager.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "Stats/Stats.h"
#include "VirtualTexturing.h"
#include "VT/AdaptiveVirtualTexture.h"
#include "VT/TexturePagePool.h"
#include "VT/UniquePageList.h"
#include "VT/UniqueRequestList.h"
#include "VT/VirtualTextureFeedback.h"
#include "VT/VirtualTexturePhysicalSpace.h"
#include "VT/VirtualTextureScalability.h"
#include "VT/VirtualTextureSpace.h"

DECLARE_CYCLE_STAT(TEXT("VirtualTextureSystem Update"), STAT_VirtualTextureSystem_Update, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Gather Requests"), STAT_ProcessRequests_Gather, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Sort Requests"), STAT_ProcessRequests_Sort, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Submit Requests"), STAT_ProcessRequests_Submit, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Map Requests"), STAT_ProcessRequests_Map, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Map New VTs"), STAT_ProcessRequests_MapNew, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Finalize Requests"), STAT_ProcessRequests_Finalize, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Merge Unique Pages"), STAT_ProcessRequests_MergePages, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Merge Requests"), STAT_ProcessRequests_MergeRequests, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Submit Tasks"), STAT_ProcessRequests_SubmitTasks, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Wait Tasks"), STAT_ProcessRequests_WaitTasks, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Queue Adaptive Requests"), STAT_ProcessRequests_QueueAdaptiveRequests, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Finalize Adaptive Requests"), STAT_ProcessRequests_FinalizeAdaptiveRequests, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Feedback Map"), STAT_FeedbackMap, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Feedback Analysis"), STAT_FeedbackAnalysis, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Page Table Updates"), STAT_PageTableUpdates, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Flush Cache"), STAT_FlushCache, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Update Stats"), STAT_UpdateStats, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible"), STAT_NumPageVisible, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible resident"), STAT_NumPageVisibleResident, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible not resident"), STAT_NumPageVisibleNotResident, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page prefetch"), STAT_NumPagePrefetch, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page update"), STAT_NumPageUpdate, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num mapped page update"), STAT_NumMappedPageUpdate, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num continuous page update"), STAT_NumContinuousPageUpdate, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page allocation fails"), STAT_NumPageAllocateFails, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num stacks requested"), STAT_NumStacksRequested, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num stacks produced"), STAT_NumStacksProduced, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num flush caches"), STAT_NumFlushCache, STATGROUP_VirtualTexturing);

DECLARE_MEMORY_STAT_POOL(TEXT("Total Physical Memory"), STAT_TotalPhysicalMemory, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);
DECLARE_MEMORY_STAT_POOL(TEXT("Total Pagetable Memory"), STAT_TotalPagetableMemory, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);

DECLARE_GPU_STAT(VirtualTexture);
DECLARE_GPU_DRAWCALL_STAT(VirtualTextureAllocate);

static TAutoConsoleVariable<int32> CVarVTVerbose(
	TEXT("r.VT.Verbose"),
	0,
	TEXT("Be pedantic about certain things that shouln't occur unless something is wrong. This may cause a lot of logspam 100's of lines per frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVTEnableFeedBack(
	TEXT("r.VT.EnableFeedBack"),
	1,
	TEXT("process readback buffer? dev option."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVTParallelFeedbackTasks(
	TEXT("r.VT.ParallelFeedbackTasks"),
	0,
	TEXT("Use worker threads for virtual texture feedback tasks."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTNumFeedbackTasks(
	TEXT("r.VT.NumFeedbackTasks"),
	1,
	TEXT("Number of tasks to create to read virtual texture feedback."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTNumGatherTasks(
	TEXT("r.VT.NumGatherTasks"),
	1,
	TEXT("Number of tasks to create to combine virtual texture feedback."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTPageUpdateFlushCount(
	TEXT("r.VT.PageUpdateFlushCount"),
	8,
	TEXT("Number of page updates to buffer before attempting to flush by taking a lock."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTForceContinuousUpdate(
	TEXT("r.VT.ForceContinuousUpdate"),
	0,
	TEXT("Force continuous update on all virtual textures."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTProduceLockedTilesOnFlush(
	TEXT("r.VT.ProduceLockedTilesOnFlush"),
	1,
	TEXT("Should locked tiles be (re)produced when flushing the cache"),
	ECVF_RenderThreadSafe
);

static FORCEINLINE uint32 EncodePage(uint32 ID, uint32 vLevel, uint32 vTileX, uint32 vTileY)
{
	uint32 Page;
	Page = vTileX << 0;
	Page |= vTileY << 12;
	Page |= vLevel << 24;
	Page |= ID << 28;
	return Page;
}

struct FPageUpdateBuffer
{
	static const uint32 PageCapacity = 128u;
	uint16 PhysicalAddresses[PageCapacity];
	uint32 PrevPhysicalAddress = ~0u;
	uint32 NumPages = 0u;
	uint32 NumPageUpdates = 0u;
	uint32 WorkingSetSize = 0u;
};

struct FFeedbackAnalysisParameters
{
	FVirtualTextureSystem* System = nullptr;
	const uint32* FeedbackBuffer = nullptr;
	FUniquePageList* UniquePageList = nullptr;
	uint32 FeedbackSize = 0u;
};

struct FGatherRequestsParameters
{
	FVirtualTextureSystem* System = nullptr;
	const FUniquePageList* UniquePageList = nullptr;
	FPageUpdateBuffer* PageUpdateBuffers = nullptr;
	FUniqueRequestList* RequestList = nullptr;
	uint32 PageUpdateFlushCount = 0u;
	uint32 PageStartIndex = 0u;
	uint32 NumPages = 0u;
	uint32 FrameRequested;
};

class FFeedbackAnalysisTask
{
public:
	explicit FFeedbackAnalysisTask(const FFeedbackAnalysisParameters& InParams) : Parameters(InParams) {}

	FFeedbackAnalysisParameters Parameters;

	static void DoTask(FFeedbackAnalysisParameters& InParams)
	{
		InParams.UniquePageList->Initialize();
		InParams.System->FeedbackAnalysisTask(InParams);
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
		DoTask(Parameters);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId GetStatId() const { return TStatId(); }
};

class FGatherRequestsTask
{
public:
	explicit FGatherRequestsTask(const FGatherRequestsParameters& InParams) : Parameters(InParams) {}

	FGatherRequestsParameters Parameters;

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
		Parameters.RequestList->Initialize();
		Parameters.System->GatherRequestsTask(Parameters);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId GetStatId() const { return TStatId(); }
};

static FVirtualTextureSystem* GVirtualTextureSystem = nullptr;

void FVirtualTextureSystem::Initialize()
{
	if (!GVirtualTextureSystem)
	{
		GVirtualTextureSystem = new FVirtualTextureSystem();
	}
}

void FVirtualTextureSystem::Shutdown()
{
	if (GVirtualTextureSystem)
	{
		delete GVirtualTextureSystem;
		GVirtualTextureSystem = nullptr;
	}
}

FVirtualTextureSystem& FVirtualTextureSystem::Get()
{
	check(GVirtualTextureSystem);
	return *GVirtualTextureSystem;
}

FVirtualTextureSystem::FVirtualTextureSystem()
	: Frame(1u) // Need to start on Frame 1, otherwise the first call to update will fail to allocate any pages
	, bFlushCaches(false)
	, FlushCachesCommand(TEXT("r.VT.Flush"), TEXT("Flush all the physical caches in the VT system."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::FlushCachesFromConsole))
	, DumpCommand(TEXT("r.VT.Dump"), TEXT("Lot a whole lot of info on the VT system state."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::DumpFromConsole))
	, ListPhysicalPools(TEXT("r.VT.ListPhysicalPools"), TEXT("Lot a whole lot of info on the VT system state."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::ListPhysicalPoolsFromConsole))
#if WITH_EDITOR
	, SaveAllocatorImages(TEXT("r.VT.SaveAllocatorImages"), TEXT("Save images showing allocator usage."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::SaveAllocatorImagesFromConsole))
#endif
{
}

FVirtualTextureSystem::~FVirtualTextureSystem()
{
	DestroyPendingVirtualTextures();

	check(AllocatedVTs.Num() == 0);

	for (uint32 SpaceID = 0u; SpaceID < MaxSpaces; ++SpaceID)
	{
		FVirtualTextureSpace* Space = Spaces[SpaceID].Get();
		if (Space)
		{
			check(Space->GetRefCount() == 0u);
			DEC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, Space->GetSizeInBytes());
			BeginReleaseResource(Space);
		}
	}
	for(int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
		if (PhysicalSpace)
		{
			check(PhysicalSpace->GetRefCount() == 0u);
			DEC_MEMORY_STAT_BY(STAT_TotalPhysicalMemory, PhysicalSpace->GetSizeInBytes());
			BeginReleaseResource(PhysicalSpace);
		}
	}
}

void FVirtualTextureSystem::FlushCachesFromConsole()
{
	FlushCache();
}

void FVirtualTextureSystem::FlushCache()
{
	// We defer the actual flush to the render thread in the Update function
	bFlushCaches = true;
}

void FVirtualTextureSystem::FlushCache(FVirtualTextureProducerHandle const& ProducerHandle, FIntRect const& TextureRegion, uint32 MaxLevel)
{
	checkSlow(IsInRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_FlushCache);
	INC_DWORD_STAT_BY(STAT_NumFlushCache, 1);

	FVirtualTextureProducer const* Producer = Producers.FindProducer(ProducerHandle);
	if (Producer != nullptr)
	{
		FVTProducerDescription const& ProducerDescription = Producer->GetDescription();

		TArray<FVirtualTexturePhysicalSpace*> PhysicalSpacesForProducer;
		for (uint32 i = 0; i < Producer->GetNumPhysicalGroups(); ++i)
		{
			PhysicalSpacesForProducer.AddUnique(Producer->GetPhysicalSpaceForPhysicalGroup(i));
		}

		check(TransientCollectedPages.Num() == 0);

		for (int32 i = 0; i < PhysicalSpacesForProducer.Num(); ++i)
		{
			FTexturePagePool& Pool = PhysicalSpacesForProducer[i]->GetPagePool();
			Pool.EvictPages(this, ProducerHandle, ProducerDescription, TextureRegion, MaxLevel, TransientCollectedPages);
		}

		for (auto& Page : TransientCollectedPages)
		{
			MappedTilesToProduce.Add(Page);
		}

		// Don't resize to allow this container to grow as needed (avoid allocations when collecting)
		TransientCollectedPages.Reset();
	}
}

void FVirtualTextureSystem::DumpFromConsole()
{
	bool verbose = false;
	for (int ID = 0; ID < 16; ID++)
	{
		FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space)
		{
			Space->DumpToConsole(verbose);
		}
	}
}

void FVirtualTextureSystem::ListPhysicalPoolsFromConsole()
{
	for(int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		if (PhysicalSpaces[i])
		{
			const FVirtualTexturePhysicalSpace& PhysicalSpace = *PhysicalSpaces[i];
			const FVTPhysicalSpaceDescription& Desc = PhysicalSpace.GetDescription();
			const FTexturePagePool& PagePool = PhysicalSpace.GetPagePool();
			const uint32 TotalSizeInBytes = PhysicalSpace.GetSizeInBytes();

			UE_LOG(LogConsoleResponse, Display, TEXT("PhysicaPool: [%i] %ix%i:"), i, Desc.TileSize, Desc.TileSize);
			
			for (int Layer = 0; Layer < Desc.NumLayers; ++Layer)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("  Layer %i=%s"), Layer, GPixelFormats[Desc.Format[Layer]].Name);
			}

			const int32 AllocatedTiles = PagePool.GetNumAllocatedPages();
			const float AllocatedLoad = (float)AllocatedTiles / (float)PhysicalSpace.GetNumTiles();
			const float AllocatedMemory = AllocatedLoad * TotalSizeInBytes / 1024.0f / 1024.0f;

			const int32 LockedTiles = PagePool.GetNumLockedPages();
			const float LockedLoad = (float)LockedTiles / (float)PhysicalSpace.GetNumTiles();
			const float LockedMemory = LockedLoad * TotalSizeInBytes / 1024.0f / 1024.0f;

			UE_LOG(LogConsoleResponse, Display, TEXT("  SizeInMegabyte= %f"), (float)TotalSizeInBytes / 1024.0f / 1024.0f);
			UE_LOG(LogConsoleResponse, Display, TEXT("  Dimensions= %ix%i"), PhysicalSpace.GetTextureSize(), PhysicalSpace.GetTextureSize());
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles= %i"), PhysicalSpace.GetNumTiles());
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Allocated= %i (%fMB)"), AllocatedTiles, AllocatedMemory);
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Locked= %i (%fMB)"), LockedTiles, LockedMemory);
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Mapped= %i"), PagePool.GetNumMappedPages());
		}
	}

	for (int ID = 0; ID < 16; ID++)
	{
		const FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space == nullptr)
		{
			continue;
		}

		const FVTSpaceDescription& Desc = Space->GetDescription();
		const FVirtualTextureAllocator& Allocator = Space->GetAllocator();
		const uint32 PageTableWidth = Space->GetPageTableWidth();
		const uint32 PageTableHeight = Space->GetPageTableHeight();
		const uint32 TotalSizeInBytes = Space->GetSizeInBytes();
		const uint32 NumAllocatedPages = Allocator.GetNumAllocatedPages();
		const uint32 NumTotalPages = PageTableWidth * PageTableHeight;
		const double AllocatedRatio = (double)NumAllocatedPages / NumTotalPages;

		const uint32 PhysicalTileSize = Desc.TileSize + Desc.TileBorderSize * 2u;
		const TCHAR* FormatName = nullptr;
		switch (Desc.PageTableFormat)
		{
		case EVTPageTableFormat::UInt16: FormatName = TEXT("UInt16"); break;
		case EVTPageTableFormat::UInt32: FormatName = TEXT("UInt32"); break;
		default: checkNoEntry(); break;
		}

		UE_LOG(LogConsoleResponse, Display, TEXT("Pool: [%i] %s (%ix%i) x %i:"), ID, FormatName, PhysicalTileSize, PhysicalTileSize, Desc.NumPageTableLayers);
		UE_LOG(LogConsoleResponse, Display, TEXT("  PageTableSize= %ix%i"), PageTableWidth, PageTableHeight);
		UE_LOG(LogConsoleResponse, Display, TEXT("  Allocations= %i, %i%% (%fMB)"),
			Allocator.GetNumAllocations(),
			(int)(AllocatedRatio * 100.0),
			(float)(AllocatedRatio * TotalSizeInBytes / 1024.0 / 1024.0));
	}
}

uint32 GetTypeHash(const FAllocatedVTDescription& Description)
{
	return FCrc::MemCrc32(&Description, sizeof(Description));
}

#if WITH_EDITOR
void FVirtualTextureSystem::SaveAllocatorImagesFromConsole()
{
	for (int ID = 0; ID < 16; ID++)
	{
		const FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space)
		{
			Space->SaveAllocatorDebugImage();
		}
	}
}
#endif // WITH_EDITOR

IAllocatedVirtualTexture* FVirtualTextureSystem::AllocateVirtualTexture(const FAllocatedVTDescription& Desc)
{
	check(Desc.NumTextureLayers <= VIRTUALTEXTURE_SPACE_MAXLAYERS);

	// Make sure any pending VTs are destroyed before attempting to allocate a new one
	// Otherwise, we might find/return an existing IAllocatedVirtualTexture* that's pending deletion
	DestroyPendingVirtualTextures();

	// Check to see if we already have an allocated VT that matches this description
	// This can happen often as multiple material instances will share the same textures
	FAllocatedVirtualTexture*& AllocatedVT = AllocatedVTs.FindOrAdd(Desc);
	if (AllocatedVT)
	{
		AllocatedVT->IncrementRefCount();
		return AllocatedVT;
	}

	uint32 BlockWidthInTiles = 0u;
	uint32 BlockHeightInTiles = 0u;
	uint32 MinWidthInBlocks = ~0u;
	uint32 MinHeightInBlocks = ~0u;
	uint32 DepthInTiles = 0u;
	bool bSupport16BitPageTable = true;
	FVirtualTextureProducer* ProducerForLayer[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { nullptr };
	bool bAnyLayerProducerWantsPersistentHighestMip = false;
	for (uint32 LayerIndex = 0u; LayerIndex < Desc.NumTextureLayers; ++LayerIndex)
	{
		FVirtualTextureProducer* Producer = Producers.FindProducer(Desc.ProducerHandle[LayerIndex]);
		ProducerForLayer[LayerIndex] = Producer;
		if (Producer)
		{
			const FVTProducerDescription& ProducerDesc = Producer->GetDescription();
			BlockWidthInTiles = FMath::Max(BlockWidthInTiles, ProducerDesc.BlockWidthInTiles);
			BlockHeightInTiles = FMath::Max(BlockHeightInTiles, ProducerDesc.BlockHeightInTiles);
			MinWidthInBlocks = FMath::Min<uint32>(MinWidthInBlocks, ProducerDesc.WidthInBlocks);
			MinHeightInBlocks = FMath::Min<uint32>(MinHeightInBlocks, ProducerDesc.HeightInBlocks);
			DepthInTiles = FMath::Max(DepthInTiles, ProducerDesc.DepthInTiles);

			uint32 ProducerLayerIndex = Desc.ProducerLayerIndex[LayerIndex];
			uint32 ProducerPhysicalGroup = Producer->GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex);
			FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroup);
			if (!PhysicalSpace->DoesSupport16BitPageTable())
			{
				bSupport16BitPageTable = false;
			}
			bAnyLayerProducerWantsPersistentHighestMip |= Producer->GetDescription().bPersistentHighestMip;
		}
	}

	check(BlockWidthInTiles > 0u);
	check(BlockHeightInTiles > 0u);
	check(DepthInTiles > 0u);

	// Find a block width that is evenly divided by all layers (least common multiple)
	// Start with min size, then increment by min size until a valid size is found
	uint32 WidthInBlocks = MinWidthInBlocks;
	{
		bool bFoundValidWidthInBlocks = false;
		while (!bFoundValidWidthInBlocks)
		{
			bFoundValidWidthInBlocks = true;
			for (uint32 LayerIndex = 0u; LayerIndex < Desc.NumTextureLayers; ++LayerIndex)
			{
				const FVirtualTextureProducer* Producer = ProducerForLayer[LayerIndex];
				if (Producer)
				{
					if ((WidthInBlocks % Producer->GetDescription().WidthInBlocks) != 0u)
					{
						WidthInBlocks += MinWidthInBlocks;
						check(WidthInBlocks > MinWidthInBlocks); // check for overflow
						bFoundValidWidthInBlocks = false;
						break;
					}
				}
			}
		}
	}

	// Same thing for height
	uint32 HeightInBlocks = MinHeightInBlocks;
	{
		bool bFoundValidHeightInBlocks = false;
		while (!bFoundValidHeightInBlocks)
		{
			bFoundValidHeightInBlocks = true;
			for (uint32 LayerIndex = 0u; LayerIndex < Desc.NumTextureLayers; ++LayerIndex)
			{
				const FVirtualTextureProducer* Producer = ProducerForLayer[LayerIndex];
				if (Producer)
				{
					if ((HeightInBlocks % Producer->GetDescription().HeightInBlocks) != 0u)
					{
						HeightInBlocks += MinHeightInBlocks;
						check(HeightInBlocks > MinHeightInBlocks); // check for overflow
						bFoundValidHeightInBlocks = false;
						break;
					}
				}
			}
		}
	}

	// Sum the total number of physical groups from all producers
	uint32 NumPhysicalGroups = 0;
	if (Desc.bShareDuplicateLayers)
	{
		TArray<FVirtualTextureProducer*> UniqueProducers;
		for (uint32 LayerIndex = 0u; LayerIndex < Desc.NumTextureLayers; ++LayerIndex)
		{
			if (ProducerForLayer[LayerIndex] != nullptr)
			{
				UniqueProducers.AddUnique(ProducerForLayer[LayerIndex]);
			}
		}
		for (int32 ProducerIndex = 0u; ProducerIndex < UniqueProducers.Num(); ++ProducerIndex)
		{
			NumPhysicalGroups += UniqueProducers[ProducerIndex]->GetNumPhysicalGroups();
		}
	}
	else
	{
		NumPhysicalGroups = Desc.NumTextureLayers;
	}

	AllocatedVT = new FAllocatedVirtualTexture(this, Frame, Desc, ProducerForLayer, BlockWidthInTiles, BlockHeightInTiles, WidthInBlocks, HeightInBlocks, DepthInTiles);
	if (bAnyLayerProducerWantsPersistentHighestMip)
	{
		AllocatedVTsToMap.Add(AllocatedVT);
	}
	return AllocatedVT;
}

void FVirtualTextureSystem::DestroyVirtualTexture(IAllocatedVirtualTexture* AllocatedVT)
{
	AllocatedVT->Destroy(this);
}

void FVirtualTextureSystem::ReleaseVirtualTexture(FAllocatedVirtualTexture* AllocatedVT)
{
	if (IsInRenderingThread())
	{
		AllocatedVT->Release(this);
	}
	else
	{
		FScopeLock Lock(&PendingDeleteLock);
		PendingDeleteAllocatedVTs.Add(AllocatedVT);
	}
}

void FVirtualTextureSystem::RemoveAllocatedVT(FAllocatedVirtualTexture* AllocatedVT)
{
	// shouldn't be more than 1 instance of this in the list
	verify(AllocatedVTsToMap.Remove(AllocatedVT) <= 1);
	// should always exist in this map
	verify(AllocatedVTs.Remove(AllocatedVT->GetDescription()) == 1);
}

void FVirtualTextureSystem::DestroyPendingVirtualTextures()
{
	check(IsInRenderingThread());
	TArray<FAllocatedVirtualTexture*> AllocatedVTsToDelete;
	{
		FScopeLock Lock(&PendingDeleteLock);
		AllocatedVTsToDelete = MoveTemp(PendingDeleteAllocatedVTs);
	}
	for (FAllocatedVirtualTexture* AllocatedVT : AllocatedVTsToDelete)
	{
		AllocatedVT->Release(this);
	}
}

IAdaptiveVirtualTexture* FVirtualTextureSystem::AllocateAdaptiveVirtualTexture(const FAdaptiveVTDescription& AdaptiveVTDesc, const FAllocatedVTDescription& AllocatedVTDesc)
{
	check(IsInRenderingThread());
	FAdaptiveVirtualTexture* AdaptiveVT = new FAdaptiveVirtualTexture(AdaptiveVTDesc, AllocatedVTDesc);
	AdaptiveVT->Init(this);
	check(AdaptiveVTs[AdaptiveVT->GetSpaceID()] == nullptr);
	AdaptiveVTs[AdaptiveVT->GetSpaceID()] = AdaptiveVT;
	return AdaptiveVT;
}

void FVirtualTextureSystem::DestroyAdaptiveVirtualTexture(IAdaptiveVirtualTexture* AdaptiveVT)
{
	check(IsInRenderingThread());
	check(AdaptiveVTs[AdaptiveVT->GetSpaceID()] == AdaptiveVT);
	AdaptiveVTs[AdaptiveVT->GetSpaceID()] = nullptr;
	AdaptiveVT->Destroy(this);
}

FVirtualTextureProducerHandle FVirtualTextureSystem::RegisterProducer(const FVTProducerDescription& InDesc, IVirtualTexture* InProducer)
{
	return Producers.RegisterProducer(this, InDesc, InProducer);
}

void FVirtualTextureSystem::ReleaseProducer(const FVirtualTextureProducerHandle& Handle)
{
	Producers.ReleaseProducer(this, Handle);
}

void FVirtualTextureSystem::AddProducerDestroyedCallback(const FVirtualTextureProducerHandle& Handle, FVTProducerDestroyedFunction* Function, void* Baton)
{
	Producers.AddDestroyedCallback(Handle, Function, Baton);
}

uint32 FVirtualTextureSystem::RemoveAllProducerDestroyedCallbacks(const void* Baton)
{
	return Producers.RemoveAllCallbacks(Baton);
}

FVirtualTextureProducer* FVirtualTextureSystem::FindProducer(const FVirtualTextureProducerHandle& Handle)
{
	return Producers.FindProducer(Handle);
}

FVirtualTextureSpace* FVirtualTextureSystem::AcquireSpace(const FVTSpaceDescription& InDesc, uint8 InForceSpaceID, FAllocatedVirtualTexture* AllocatedVT)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	uint32 NumFailedAllocations = 0u;

	// If InDesc requests a private space, don't reuse any existing spaces (unless it is a forced space)
	if (!InDesc.bPrivateSpace || InForceSpaceID != 0xff)
	{
		for (uint32 SpaceIndex = 0u; SpaceIndex < MaxSpaces; ++SpaceIndex)
		{
			if (SpaceIndex == InForceSpaceID || InForceSpaceID == 0xff)
			{
				FVirtualTextureSpace* Space = Spaces[SpaceIndex].Get();
				if (Space && Space->GetDescription() == InDesc)
				{
					const int32 PagetableMemory = Space->GetSizeInBytes();
					const uint32 vAddress = Space->AllocateVirtualTexture(AllocatedVT);
					if (vAddress != ~0u)
					{
						const int32 NewPagetableMemory = Space->GetSizeInBytes();
						INC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, NewPagetableMemory - PagetableMemory);

						AllocatedVT->AssignVirtualAddress(vAddress);
						Space->AddRef();
						return Space;
					}
					else
					{
						++NumFailedAllocations;
					}
				}
			}
		}
	}

	// Try to allocate a new space
	if (InForceSpaceID == 0xff)
	{
		for (uint32 SpaceIndex = 0u; SpaceIndex < MaxSpaces; ++SpaceIndex)
		{
			if (!Spaces[SpaceIndex])
			{
				const uint32 InitialPageTableSize = InDesc.bPrivateSpace ? InDesc.MaxSpaceSize : FMath::Max(AllocatedVT->GetWidthInTiles(), AllocatedVT->GetHeightInTiles());
				FVirtualTextureSpace* Space = new FVirtualTextureSpace(this, SpaceIndex, InDesc, InitialPageTableSize);
				Spaces[SpaceIndex].Reset(Space);
				INC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, Space->GetSizeInBytes());
				BeginInitResource(Space);

				const uint32 vAddress = Space->AllocateVirtualTexture(AllocatedVT);
				AllocatedVT->AssignVirtualAddress(vAddress);

				Space->AddRef();
				return Space;
			}
		}
	}

	// Out of space slots
	checkf(false, TEXT("Failed to acquire space for VT (%d x %d), failed to allocate from %d existing matching spaces"),
		AllocatedVT->GetWidthInTiles(), AllocatedVT->GetHeightInTiles(), NumFailedAllocations);
	return nullptr;
}

void FVirtualTextureSystem::ReleaseSpace(FVirtualTextureSpace* Space)
{
	check(IsInRenderingThread());
	const uint32 NumRefs = Space->Release();
	if (NumRefs == 0u && Space->GetDescription().bPrivateSpace)
	{
		// Private spaces are destroyed when ref count reaches 0
		// This can only happen on render thread, so we can call ReleaseResource() directly and then delete the pointer immediately
		DEC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, Space->GetSizeInBytes());
		Space->ReleaseResource();
		Spaces[Space->GetID()].Reset();
	}
}

FVirtualTexturePhysicalSpace* FVirtualTextureSystem::AcquirePhysicalSpace(const FVTPhysicalSpaceDescription& InDesc)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
		if (PhysicalSpace && PhysicalSpace->GetDescription() == InDesc)
		{
			return PhysicalSpace;
		}
	}

	uint32 ID = PhysicalSpaces.Num();
	check(ID <= 0x0fff);

	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		if (!PhysicalSpaces[i])
		{
			ID = i;
			break;
		}
	}

	if (ID == PhysicalSpaces.Num())
	{
		PhysicalSpaces.AddZeroed();
	}

	FVirtualTexturePhysicalSpace* PhysicalSpace = new FVirtualTexturePhysicalSpace(InDesc, ID);
	PhysicalSpaces[ID] = PhysicalSpace;

	INC_MEMORY_STAT_BY(STAT_TotalPhysicalMemory, PhysicalSpace->GetSizeInBytes());
	BeginInitResource(PhysicalSpace);
	return PhysicalSpace;
}

void FVirtualTextureSystem::ReleasePendingSpaces()
{
	check(IsInRenderingThread());
	for (int32 Id = 0; Id < PhysicalSpaces.Num(); ++Id)
	{
		// Physical space is released when ref count hits 0
		// Might need to have some mechanism to hold an extra reference if we know we will be recycling very soon (such when doing level reload)
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[Id];
		if ((bool)PhysicalSpace && PhysicalSpace->GetRefCount() == 0u)
		{
			DEC_MEMORY_STAT_BY(STAT_TotalPhysicalMemory, PhysicalSpace->GetSizeInBytes());

			const FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
			check(PagePool.GetNumMappedPages() == 0u);
			check(PagePool.GetNumLockedPages() == 0u);

			PhysicalSpace->ReleaseResource();
			delete PhysicalSpace;
			PhysicalSpaces[Id] = nullptr;
		}
	}
}

void FVirtualTextureSystem::LockTile(const FVirtualTextureLocalTile& Tile)
{
	check(IsInRenderingThread());

	if (TileLocks.Lock(Tile))
	{
		checkSlow(!TilesToLock.Contains(Tile));
		TilesToLock.Add(Tile);
	}
}

static void UnlockTileInternal(const FVirtualTextureProducerHandle& ProducerHandle, const FVirtualTextureProducer* Producer, const FVirtualTextureLocalTile& Tile, uint32 Frame)
{
	for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer->GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
		FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
		const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, Tile.Local_vAddress, Tile.Local_vLevel);
		if (pAddress != ~0u)
		{
			PagePool.Unlock(Frame, pAddress);
		}
	}
}

void FVirtualTextureSystem::UnlockTile(const FVirtualTextureLocalTile& Tile, const FVirtualTextureProducer* Producer)
{
	check(IsInRenderingThread());

	if (TileLocks.Unlock(Tile))
	{
		// Tile is no longer locked
		const int32 NumTilesRemoved = TilesToLock.Remove(Tile);
		check(NumTilesRemoved <= 1);
		// If tile was still in the 'TilesToLock' list, that means it was never actually locked, so we don't need to do the unlock here
		if (NumTilesRemoved == 0)
		{
			UnlockTileInternal(Tile.GetProducerHandle(), Producer, Tile, Frame);
		}
	}
}

void FVirtualTextureSystem::ForceUnlockAllTiles(const FVirtualTextureProducerHandle& ProducerHandle, const FVirtualTextureProducer* Producer)
{
	check(IsInRenderingThread());

	TArray<FVirtualTextureLocalTile> TilesToUnlock;
	TileLocks.ForceUnlockAll(ProducerHandle, TilesToUnlock);

	for (const FVirtualTextureLocalTile& Tile : TilesToUnlock)
	{
		const int32 NumTilesRemoved = TilesToLock.Remove(Tile);
		check(NumTilesRemoved <= 1);
		if (NumTilesRemoved == 0)
		{
			UnlockTileInternal(ProducerHandle, Producer, Tile, Frame);
		}
	}
}

static float ComputeMipLevel(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize)
{
	const uint32 TextureWidth = AllocatedVT->GetWidthInPixels();
	const uint32 TextureHeight = AllocatedVT->GetHeightInPixels();
	const FVector2D dfdx(TextureWidth / InScreenSpaceSize.X, 0.0f);
	const FVector2D dfdy(0.0f, TextureHeight / InScreenSpaceSize.Y);
	const float ppx = FVector2D::DotProduct(dfdx, dfdx);
	const float ppy = FVector2D::DotProduct(dfdy, dfdy);
	return 0.5f * FMath::Log2(FMath::Max(ppx, ppy));
}

void FVirtualTextureSystem::RequestTiles(const FVector2D& InScreenSpaceSize, int32 InMipLevel)
{
	check(IsInRenderingThread());

	for (const auto& Pair : AllocatedVTs)
	{
		RequestTilesForRegion(Pair.Value, InScreenSpaceSize, FIntRect(), InMipLevel);
	}
}

void FVirtualTextureSystem::RequestTilesForRegion(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FIntRect& InTextureRegion, int32 InMipLevel)
{
	FIntRect TextureRegion(InTextureRegion);
	if (TextureRegion.IsEmpty())
	{
		TextureRegion.Max.X = AllocatedVT->GetWidthInPixels();
		TextureRegion.Max.Y = AllocatedVT->GetHeightInPixels();
	}
	else
	{
		TextureRegion.Clip(FIntRect(0, 0, AllocatedVT->GetWidthInPixels(), AllocatedVT->GetHeightInPixels()));
	}

	if (InMipLevel >= 0)
	{
		FScopeLock Lock(&RequestedTilesLock);
		RequestTilesForRegionInternal(AllocatedVT, TextureRegion, InMipLevel);
	}
	else
	{
		const uint32 vMaxLevel = AllocatedVT->GetMaxLevel();
		const float vLevel = ComputeMipLevel(AllocatedVT, InScreenSpaceSize);
		const int32 vMipLevelDown = FMath::Clamp((int32)FMath::FloorToInt(vLevel), 0, (int32)vMaxLevel);

		FScopeLock Lock(&RequestedTilesLock);
		RequestTilesForRegionInternal(AllocatedVT, TextureRegion, vMipLevelDown);
		if (vMipLevelDown + 1u <= vMaxLevel)
		{
			// Need to fetch 2 levels to support trilinear filtering
			RequestTilesForRegionInternal(AllocatedVT, TextureRegion, vMipLevelDown + 1u);
		}
	}
}

void FVirtualTextureSystem::LoadPendingTiles(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());

	TArray<uint32> PackedTiles;
	if (RequestedPackedTiles.Num() > 0)
	{
		FScopeLock Lock(&RequestedTilesLock);
		PackedTiles = MoveTemp(RequestedPackedTiles);
		RequestedPackedTiles.Reset();
	}

	if (PackedTiles.Num() > 0)
	{
		FMemStack& MemStack = FMemStack::Get();
		FUniquePageList* UniquePageList = new(MemStack) FUniquePageList;
		UniquePageList->Initialize();
		for (uint32 Tile : PackedTiles)
		{
			UniquePageList->Add(Tile, 0xffff);
		}

		FUniqueRequestList* RequestList = new(MemStack) FUniqueRequestList(MemStack);
		RequestList->Initialize();
		GatherRequests(RequestList, UniquePageList, Frame, MemStack);
		// No need to sort requests, since we're submitting all of them here (no throttling)
		AllocateResources(GraphBuilder, FeatureLevel);
		SubmitRequests(GraphBuilder, FeatureLevel, MemStack, RequestList, false);
	}
}

void FVirtualTextureSystem::RequestTilesForRegionInternal(const IAllocatedVirtualTexture* AllocatedVT, const FIntRect& InTextureRegion, uint32 vLevel)
{
	const FIntRect TextureRegionForLevel(InTextureRegion.Min.X >> vLevel, InTextureRegion.Min.Y >> vLevel, InTextureRegion.Max.X >> vLevel, InTextureRegion.Max.Y >> vLevel);
	const FIntRect TileRegionForLevel = FIntRect::DivideAndRoundUp(TextureRegionForLevel, AllocatedVT->GetVirtualTileSize());

	// RequestedPackedTiles stores packed tiles with vPosition shifted relative to current mip level
	const uint32 vBaseTileX = FMath::ReverseMortonCode2(AllocatedVT->GetVirtualAddress()) >> vLevel;
	const uint32 vBaseTileY = FMath::ReverseMortonCode2(AllocatedVT->GetVirtualAddress() >> 1) >> vLevel;

	for (uint32 TileY = TileRegionForLevel.Min.Y; TileY < (uint32)TileRegionForLevel.Max.Y; ++TileY)
	{
		const uint32 vGlobalTileY = vBaseTileY + TileY;
		for (uint32 TileX = TileRegionForLevel.Min.X; TileX < (uint32)TileRegionForLevel.Max.X; ++TileX)
		{
			const uint32 vGlobalTileX = vBaseTileX + TileX;
			const uint32 EncodedTile = EncodePage(AllocatedVT->GetSpaceID(), vLevel, vGlobalTileX, vGlobalTileY);
			RequestedPackedTiles.Add(EncodedTile);
		}
	}
}

void FVirtualTextureSystem::FeedbackAnalysisTask(const FFeedbackAnalysisParameters& Parameters)
{
	FUniquePageList* RESTRICT RequestedPageList = Parameters.UniquePageList;
	const uint32* RESTRICT Buffer = Parameters.FeedbackBuffer;
	const uint32 BufferSize = Parameters.FeedbackSize;

	// Combine simple runs of identical requests
	uint32 LastPixel = 0xffffffff;
	uint32 LastCount = 0;

	for (uint32 Index = 0; Index < BufferSize; Index++)
	{
		const uint32 Pixel = Buffer[Index];
		if (Pixel == LastPixel)
		{
			LastCount++;
			continue;
		}

		if (LastPixel != 0xffffffff)
		{
			RequestedPageList->Add(LastPixel, LastCount);
		}

		LastPixel = Pixel;
		LastCount = 1;
	}

	if (LastPixel != 0xffffffff)
	{
		RequestedPageList->Add(LastPixel, LastCount);
	}
}

void FVirtualTextureSystem::Update(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FScene* Scene)
{
	check(IsInRenderingThread());

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(VirtualTextureSystem_Update);
	SCOPE_CYCLE_COUNTER(STAT_VirtualTextureSystem_Update);
	RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTexture);

	if (bFlushCaches)
	{
		SCOPE_CYCLE_COUNTER(STAT_FlushCache);
		INC_DWORD_STAT_BY(STAT_NumFlushCache, 1);

		for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
		{
			FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
			if (PhysicalSpace)
			{
				if (CVarVTProduceLockedTilesOnFlush.GetValueOnRenderThread())
				{
					// Collect locked pages to be produced again
					PhysicalSpace->GetPagePool().GetAllLockedPages(this, MappedTilesToProduce);
				}
				// Flush unlocked pages
				PhysicalSpace->GetPagePool().EvictAllPages(this);
			}
		}

		bFlushCaches = false;
	}

	DestroyPendingVirtualTextures();

	// Early out when no allocated VTs
	if (AllocatedVTs.Num() == 0)
	{
		MappedTilesToProduce.Reset();
		return;
	}

	// Flush any dirty runtime virtual textures for the current scene
	if (Scene != nullptr)
	{
		// Only flush if we know that there is GPU feedback available to refill the visible data this frame
		// This prevents bugs when low frame rate causes feedback buffer to stall so that the physical cache isn't filled immediately which causes visible glitching
		if (GVirtualTextureFeedback.CanMap(GraphBuilder.RHICmdList))
		{
			// Each RVT will call FVirtualTextureSystem::FlushCache()
			Scene->FlushDirtyRuntimeVirtualTextures();
		}
	}

	// Update Adaptive VTs
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_FinalizeAdaptiveRequests);
		for (uint32 ID = 0; ID < MaxSpaces; ID++)
		{
			if (AdaptiveVTs[ID])
			{
				AdaptiveVTs[ID]->UpdateAllocations(this, GraphBuilder.RHICmdList, Frame);
			}
		}
	}

	FMemStack& MemStack = FMemStack::Get();
	FUniquePageList* MergedUniquePageList = new(MemStack) FUniquePageList;
	MergedUniquePageList->Initialize();
	
	if (CVarVTEnableFeedBack.GetValueOnRenderThread())
	{
		FMemMark FeedbackMark(MemStack);

		// Fetch feedback for analysis
		FVirtualTextureFeedback::FMapResult FeedbackResult;

		{
			SCOPE_CYCLE_COUNTER(STAT_FeedbackMap);
			FeedbackResult = GVirtualTextureFeedback.Map(GraphBuilder.RHICmdList);
		}

		// Create tasks to read the feedback data
		// Give each task a section of the feedback buffer to analyze
		FFeedbackAnalysisParameters FeedbackAnalysisParameters[MaxNumTasks];

		const uint32 MaxNumFeedbackTasks = FMath::Clamp((uint32)CVarVTNumFeedbackTasks.GetValueOnRenderThread(), 1u, MaxNumTasks);
		const uint32 FeedbackSizePerTask = FMath::DivideAndRoundUp(FeedbackResult.Size, MaxNumFeedbackTasks);

		uint32 NumFeedbackTasks = 0;
		uint32 CurrentOffset = 0;
		while (CurrentOffset < FeedbackResult.Size)
		{
			const uint32 TaskIndex = NumFeedbackTasks++;
			FFeedbackAnalysisParameters& Params = FeedbackAnalysisParameters[TaskIndex];
			Params.System = this;
			if (TaskIndex == 0u)
			{
				Params.UniquePageList = MergedUniquePageList;
			}
			else
			{
				Params.UniquePageList = new(MemStack) FUniquePageList;
			}
			Params.FeedbackBuffer = FeedbackResult.Data + CurrentOffset;

			const uint32 Size = FMath::Min(FeedbackSizePerTask, FeedbackResult.Size - CurrentOffset);
			Params.FeedbackSize = Size;
			CurrentOffset += Size;
		}

		// Kick the tasks
		const bool bParallelTasks = CVarVTParallelFeedbackTasks.GetValueOnRenderThread() != 0;
		const int32 LocalFeedbackTaskCount = bParallelTasks ? 1 : NumFeedbackTasks;
		const int32 WorkerFeedbackTaskCount = NumFeedbackTasks - LocalFeedbackTaskCount;

		FGraphEventArray Tasks;
		if (WorkerFeedbackTaskCount > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_SubmitTasks);
			Tasks.Reserve(WorkerFeedbackTaskCount);
			for (uint32 TaskIndex = LocalFeedbackTaskCount; TaskIndex < NumFeedbackTasks; ++TaskIndex)
			{
				Tasks.Add(TGraphTask<FFeedbackAnalysisTask>::CreateTask().ConstructAndDispatchWhenReady(FeedbackAnalysisParameters[TaskIndex]));
			}
		}

		if (NumFeedbackTasks > 0u)
		{
			SCOPE_CYCLE_COUNTER(STAT_FeedbackAnalysis);

			for (int32 TaskIndex = 0; TaskIndex < LocalFeedbackTaskCount; ++TaskIndex)
			{
				FFeedbackAnalysisTask::DoTask(FeedbackAnalysisParameters[TaskIndex]);
			}
			if (WorkerFeedbackTaskCount > 0)
			{
				SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_WaitTasks);

				FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks, ENamedThreads::GetRenderThread_Local());
			}
		}

		if (NumFeedbackTasks > 1u)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MergePages);
			for (uint32 TaskIndex = 1u; TaskIndex < NumFeedbackTasks; ++TaskIndex)
			{
				MergedUniquePageList->MergePages(FeedbackAnalysisParameters[TaskIndex].UniquePageList);
			}
		}

		GVirtualTextureFeedback.Unmap(GraphBuilder.RHICmdList, FeedbackResult.MapHandle);
	}

	FUniqueRequestList* MergedRequestList = new(MemStack) FUniqueRequestList(MemStack);
	MergedRequestList->Initialize();

	// Collect tiles to lock
	{
		for (const FVirtualTextureLocalTile& Tile : TilesToLock)
		{
			const FVirtualTextureProducerHandle ProducerHandle = Tile.GetProducerHandle();
			const FVirtualTextureProducer* Producer = Producers.FindProducer(ProducerHandle);
			checkSlow(TileLocks.IsLocked(Tile));
			if (Producer)
			{
				uint8 ProducerLayerMaskToLoad = 0u;
				for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer->GetNumTextureLayers(); ++ProducerLayerIndex)
				{
					uint32 GroupIndex = Producer->GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex);
					FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(GroupIndex);
					FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
					const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, GroupIndex, Tile.Local_vAddress, Tile.Local_vLevel);
					if (pAddress == ~0u)
					{
						ProducerLayerMaskToLoad |= (1u << ProducerLayerIndex);
					}
					else
					{
						PagePool.Lock(pAddress);
					}
				}
				if (ProducerLayerMaskToLoad != 0u)
				{
					MergedRequestList->LockLoadRequest(FVirtualTextureLocalTile(Tile.GetProducerHandle(), Tile.Local_vAddress, Tile.Local_vLevel), ProducerLayerMaskToLoad);
				}
			}
		}

		TilesToLock.Reset();
	}

	TArray<uint32> PackedTiles;
	if(RequestedPackedTiles.Num() > 0)
	{
		FScopeLock Lock(&RequestedTilesLock);
		PackedTiles = MoveTemp(RequestedPackedTiles);
		RequestedPackedTiles.Reset();
	}

	if (PackedTiles.Num() > 0)
	{
		// Collect explicitly requested tiles
		// These tiles are generated on the current frame, so they are collected/processed in a separate list
		FUniquePageList* RequestedPageList = new(MemStack) FUniquePageList;
		RequestedPageList->Initialize();
		for (uint32 Tile : PackedTiles)
		{
			RequestedPageList->Add(Tile, 0xffff);
		}
		GatherRequests(MergedRequestList, RequestedPageList, Frame, MemStack);
	}

	// Pages from feedback buffer were generated several frames ago, so they may no longer be valid for newly allocated VTs
	static uint32 PendingFrameDelay = 3u;
	if (Frame >= PendingFrameDelay)
	{
		GatherRequests(MergedRequestList, MergedUniquePageList, Frame - PendingFrameDelay, MemStack);
	}

	if (MergedRequestList->GetNumAdaptiveAllocationRequests() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_QueueAdaptiveRequests);
		FAdaptiveVirtualTexture::QueuePackedAllocationRequests(this, &MergedRequestList->GetAdaptiveAllocationRequest(0), MergedRequestList->GetNumAdaptiveAllocationRequests(), Frame);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Sort);

		// Limit the number of uploads (account for MappedTilesToProduce this frame)
		// Are all pages equal? Should there be different limits on different types of pages?
		const int32 MaxNumUploads = VirtualTextureScalability::GetMaxUploadsPerFrame();
		const int32 MaxRequestUploads = FMath::Max(MaxNumUploads - MappedTilesToProduce.Num(), 1);

		if (MaxRequestUploads < (int32)MergedRequestList->GetNumLoadRequests())
		{
			// Dropping requests is normal but track to log here if we want to tune settings.
			if (CVarVTVerbose.GetValueOnRenderThread())
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("VT dropped %d load requests."), MergedRequestList->GetNumLoadRequests() - MaxRequestUploads);
			}
		}

		MergedRequestList->SortRequests(Producers, MemStack, MaxRequestUploads);
	}

	{
		// After sorting and clamping the load requests, if we still have unused upload bandwidth then use it to add some continous updates
		const int32 MaxNumUploads = VirtualTextureScalability::GetMaxUploadsPerFrame();
		const int32 MaxTilesToProduce = FMath::Max(MaxNumUploads - MappedTilesToProduce.Num() - (int32)MergedRequestList->GetNumLoadRequests(), 0);

		GetContinuousUpdatesToProduce(MergedRequestList, MaxTilesToProduce);
	}

	// Submit the requests to produce pages that are already mapped
	SubmitPreMappedRequests(GraphBuilder, FeatureLevel);
	// Submit the merged requests
	SubmitRequests(GraphBuilder, FeatureLevel, MemStack, MergedRequestList, true);

	UpdateCSVStats();

	ReleasePendingSpaces();
}

void FVirtualTextureSystem::GatherRequests(FUniqueRequestList* MergedRequestList, const FUniquePageList* UniquePageList, uint32 FrameRequested, FMemStack& MemStack)
{
	FMemMark GatherMark(MemStack);

	const uint32 MaxNumGatherTasks = FMath::Clamp((uint32)CVarVTNumGatherTasks.GetValueOnRenderThread(), 1u, MaxNumTasks);
	const uint32 PageUpdateFlushCount = FMath::Min<uint32>(CVarVTPageUpdateFlushCount.GetValueOnRenderThread(), FPageUpdateBuffer::PageCapacity);

	FGatherRequestsParameters GatherRequestsParameters[MaxNumTasks];
	uint32 NumGatherTasks = 0u;
	{
		const uint32 MinNumPagesPerTask = 64u;
		const uint32 NumPagesPerTask = FMath::Max(FMath::DivideAndRoundUp(UniquePageList->GetNum(), MaxNumGatherTasks), MinNumPagesPerTask);
		const uint32 NumPages = UniquePageList->GetNum();
		uint32 StartPageIndex = 0u;
		while (StartPageIndex < NumPages)
		{
			const uint32 NumPagesForTask = FMath::Min(NumPagesPerTask, NumPages - StartPageIndex);
			if (NumPagesForTask > 0u)
			{
				const uint32 TaskIndex = NumGatherTasks++;
				FGatherRequestsParameters& Params = GatherRequestsParameters[TaskIndex];
				Params.System = this;
				Params.FrameRequested = FrameRequested;
				Params.UniquePageList = UniquePageList;
				Params.PageUpdateFlushCount = PageUpdateFlushCount;
				Params.PageUpdateBuffers = new(MemStack) FPageUpdateBuffer[PhysicalSpaces.Num()];
				if (TaskIndex == 0u)
				{
					Params.RequestList = MergedRequestList;
				}
				else
				{
					Params.RequestList = new(MemStack) FUniqueRequestList(MemStack);
				}
				Params.PageStartIndex = StartPageIndex;
				Params.NumPages = NumPagesForTask;
				StartPageIndex += NumPagesForTask;
			}
		}
	}

	// Kick all of the tasks
	FGraphEventArray Tasks;
	if (NumGatherTasks > 1u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_SubmitTasks);
		Tasks.Reserve(NumGatherTasks - 1u);
		for (uint32 TaskIndex = 1u; TaskIndex < NumGatherTasks; ++TaskIndex)
		{
			Tasks.Add(TGraphTask<FGatherRequestsTask>::CreateTask().ConstructAndDispatchWhenReady(GatherRequestsParameters[TaskIndex]));
		}
	}

	if (NumGatherTasks > 0u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Gather);

		// first task can run on this thread
		GatherRequestsTask(GatherRequestsParameters[0]);

		// Wait for them to complete
		if (Tasks.Num() > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_WaitTasks);

			FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks, ENamedThreads::GetRenderThread_Local());
		}
	}

	// Merge request lists for all tasks
	if (NumGatherTasks > 1u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MergeRequests);
		for (uint32 TaskIndex = 1u; TaskIndex < NumGatherTasks; ++TaskIndex)
		{
			MergedRequestList->MergeRequests(GatherRequestsParameters[TaskIndex].RequestList, MemStack);
		}
	}
}

void FVirtualTextureSystem::AddPageUpdate(FPageUpdateBuffer* Buffers, uint32 FlushCount, uint32 PhysicalSpaceID, uint16 pAddress)
{
	FPageUpdateBuffer& RESTRICT Buffer = Buffers[PhysicalSpaceID];
	if (pAddress == Buffer.PrevPhysicalAddress)
	{
		return;
	}
	Buffer.PrevPhysicalAddress = pAddress;

	bool bLocked = false;
	if (Buffer.NumPages >= FlushCount)
	{
		// Once we've passed a certain threshold of pending pages to update, try to take the lock then apply the updates
		FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceID);
		FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
		FCriticalSection& RESTRICT Lock = PagePool.GetLock();

		if (Buffer.NumPages >= FPageUpdateBuffer::PageCapacity)
		{
			// If we've reached capacity, need to take the lock no matter what, may potentially block here
			Lock.Lock();
			bLocked = true;
		}
		else
		{
			// try to take the lock, but avoid stalling
			bLocked = Lock.TryLock();
		}

		if(bLocked)
		{
			const uint32 CurrentFrame = Frame;
			PagePool.UpdateUsage(CurrentFrame, pAddress); // Update current request now, if we manage to get the lock
			for (uint32 i = 0u; i < Buffer.NumPages; ++i)
			{
				PagePool.UpdateUsage(CurrentFrame, Buffer.PhysicalAddresses[i]);
			}
			Lock.Unlock();
			Buffer.NumPageUpdates += (Buffer.NumPages + 1u);
			Buffer.NumPages = 0u;
		}
	}

	// Only need to buffer if we didn't lock (otherwise this has already been updated)
	if (!bLocked)
	{
		check(Buffer.NumPages < FPageUpdateBuffer::PageCapacity);
		Buffer.PhysicalAddresses[Buffer.NumPages++] = pAddress;
	}
}

void FVirtualTextureSystem::GatherRequestsTask(const FGatherRequestsParameters& Parameters)
{
	const FUniquePageList* RESTRICT UniquePageList = Parameters.UniquePageList;
	FPageUpdateBuffer* RESTRICT PageUpdateBuffers = Parameters.PageUpdateBuffers;
	FUniqueRequestList* RESTRICT RequestList = Parameters.RequestList;
	const uint32 PageUpdateFlushCount = Parameters.PageUpdateFlushCount;
	const uint32 PageEndIndex = Parameters.PageStartIndex + Parameters.NumPages;

	uint32 NumRequestsPages = 0u;
	uint32 NumResidentPages = 0u;
	uint32 NumNonResidentPages = 0u;
	uint32 NumPrefetchPages = 0u;

	const bool bForceContinuousUpdate = CVarVTForceContinuousUpdate.GetValueOnRenderThread() != 0;

	for (uint32 i = Parameters.PageStartIndex; i < PageEndIndex; ++i)
	{
		const uint32 PageEncoded = UniquePageList->GetPage(i);
		const uint32 PageCount = UniquePageList->GetCount(i);

		// Decode page
		const uint32 ID = (PageEncoded >> 28);
		const FVirtualTextureSpace* RESTRICT Space = GetSpace(ID);
		if (Space == nullptr)
		{
			continue;
		}

		const uint32 vLevelPlusOne = ((PageEncoded >> 24) & 0x0f);
		const uint32 vLevel = FMath::Max(vLevelPlusOne, 1u) - 1;
		
		// vPageX/Y passed from shader are relative to the given vLevel, we shift them up so be relative to level0
		// TODO - should we just do this in the shader?
		const uint32 vPageX = (PageEncoded & 0xfff) << vLevel;
		const uint32 vPageY = ((PageEncoded >> 12) & 0xfff) << vLevel;
		
		const uint32 vAddress = FMath::MortonCode2(vPageX) | (FMath::MortonCode2(vPageY) << 1);
	
		const FAdaptiveVirtualTexture* RESTRICT AdaptiveVT = AdaptiveVTs[ID];
		if (AdaptiveVT != nullptr && vLevelPlusOne <= 1)
		{
			uint32 AdaptiveAllocationRequest = AdaptiveVT->GetPackedAllocationRequest(vAddress, vLevelPlusOne, Frame);
			if (AdaptiveAllocationRequest != 0)
			{
				RequestList->AddAdaptiveAllocationRequest(AdaptiveAllocationRequest);
			}
		}

		uint32 PageTableLayersToLoad[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0 };
		uint32 NumPageTableLayersToLoad = 0u;
		{
			const FTexturePage VirtualPage(vLevel, vAddress);
			const uint16 VirtualPageHash = MurmurFinalize32(VirtualPage.Packed);
			for (uint32 PageTableLayerIndex = 0u; PageTableLayerIndex < Space->GetNumPageTableLayers(); ++PageTableLayerIndex)
			{
				const FTexturePageMap& RESTRICT PageMap = Space->GetPageMapForPageTableLayer(PageTableLayerIndex);

				++NumRequestsPages;
				const FPhysicalSpaceIDAndAddress PhysicalSpaceIDAndAddress = PageMap.FindPagePhysicalSpaceIDAndAddress(VirtualPage, VirtualPageHash);
				if (PhysicalSpaceIDAndAddress.Packed != ~0u)
				{
#if DO_GUARD_SLOW
					const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID);
					checkSlow(PhysicalSpaceIDAndAddress.pAddress < PhysicalSpace->GetNumTiles());
#endif // DO_GUARD_SLOW

					// Page is already resident, just need to update LRU free list
					AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpaceIDAndAddress.PhysicalSpaceID, PhysicalSpaceIDAndAddress.pAddress);

					// If continuous update flag is set then add this to pages which can be potentially updated if we have spare upload bandwidth
					//todo[vt]: Would be better to test continuous update flag *per producer*, but this would require extra indirection so need to profile first
					if (bForceContinuousUpdate || GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID)->GetDescription().bContinuousUpdate)
					{
						FTexturePagePool& RESTRICT PagePool = GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID)->GetPagePool();
						const FVirtualTextureLocalTile LocalTile = PagePool.GetLocalTileFromPhysicalAddress(PhysicalSpaceIDAndAddress.pAddress);
						RequestList->AddContinuousUpdateRequest(LocalTile);
					}

					++PageUpdateBuffers[PhysicalSpaceIDAndAddress.PhysicalSpaceID].WorkingSetSize;
					++NumResidentPages;
				}
				else
				{
					// Page not resident, store for later processing
					PageTableLayersToLoad[NumPageTableLayersToLoad++] = PageTableLayerIndex;
				}
			}
		}

		if (NumPageTableLayersToLoad == 0u)
		{
			// All pages are resident and properly mapped, we're done
			// This is the fast path, as most frames should generally have the majority of tiles already mapped
			continue;
		}

		// Need to resolve AllocatedVT in order to determine which pages to load
		const FAllocatedVirtualTexture* RESTRICT AllocatedVT = Space->GetAllocator().Find(vAddress);
		if (!AllocatedVT)
		{
			if (CVarVTVerbose.GetValueOnAnyThread())
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Space %i, vAddr %i@%i is not allocated to any AllocatedVT but was still requested."), ID, vAddress, vLevel);
			}
			continue;
		}

		if (AllocatedVT->GetFrameAllocated() > Parameters.FrameRequested)
		{
			// If the VT was allocated after the frame that generated this feedback, it's no longer valid
			continue;
		}

		check(AllocatedVT->GetNumPageTableLayers() == Space->GetNumPageTableLayers());
		if (vLevel > AllocatedVT->GetMaxLevel())
		{
			// Requested level is outside the given allocated VT
			// This can happen for requests made by expanding mips, since we don't know the current allocated VT in that context
			check(NumPageTableLayersToLoad == Space->GetNumPageTableLayers()); // no pages from this request should have been resident
			check(NumRequestsPages >= Space->GetNumPageTableLayers()); // don't want to track these requests, since it turns out they're not valid
			NumRequestsPages -= Space->GetNumPageTableLayers();
			continue;
		}

		// Build producer local layer masks from physical layers that we need to load
		uint8 ProducerGroupMaskToLoad[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0u };
		uint8 ProducerTextureLayerMaskToLoad[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0u };

		const uint32 NumUniqueProducers = AllocatedVT->GetNumUniqueProducers();

		for (uint32 LoadPageTableLayerIndex = 0u; LoadPageTableLayerIndex < NumPageTableLayersToLoad; ++LoadPageTableLayerIndex)
		{
			const uint32 PageTableLayerIndex = PageTableLayersToLoad[LoadPageTableLayerIndex];
			const uint32 ProducerIndex = AllocatedVT->GetProducerIndexForPageTableLayer(PageTableLayerIndex);
			check(ProducerIndex < NumUniqueProducers);
			
			const uint32 ProducerTextureLayerMask = AllocatedVT->GetProducerTextureLayerMaskForPageTableLayer(PageTableLayerIndex);
			ProducerTextureLayerMaskToLoad[ProducerIndex] |= ProducerTextureLayerMask;
			
			const uint32 ProducerPhysicalGroupIndex = AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(PageTableLayerIndex);
			ProducerGroupMaskToLoad[ProducerIndex] |= 1 << ProducerPhysicalGroupIndex;

			const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = AllocatedVT->GetPhysicalSpaceForPageTableLayer(PageTableLayerIndex);
			if (PhysicalSpace)
			{
				++PageUpdateBuffers[PhysicalSpace->GetID()].WorkingSetSize;
			}
		}

		const uint32 vDimensions = Space->GetDimensions();
		check(vAddress >= AllocatedVT->GetVirtualAddress());

		for (uint32 ProducerIndex = 0u; ProducerIndex < NumUniqueProducers; ++ProducerIndex)
		{
			uint8 GroupMaskToLoad = ProducerGroupMaskToLoad[ProducerIndex];
			if (GroupMaskToLoad == 0u)
			{
				continue;
			}

			const FVirtualTextureProducerHandle ProducerHandle = AllocatedVT->GetUniqueProducerHandle(ProducerIndex);
			const FVirtualTextureProducer* RESTRICT Producer = Producers.FindProducer(ProducerHandle);
			if (!Producer)
			{
				continue;
			}

			const uint32 MaxLevel = FMath::Min(Producer->GetMaxLevel(), AllocatedVT->GetMaxLevel());
			const uint32 ProducerMipBias = AllocatedVT->GetUniqueProducerMipBias(ProducerIndex);

			// here vLevel is clamped against ProducerMipBias, as ProducerMipBias represents the most detailed level of this producer, relative to the allocated VT
			// used to rescale vAddress to the correct tile within the given mip level
			uint32 Mapping_vLevel = FMath::Max(vLevel, ProducerMipBias);

			// Local_vLevel is the level within the producer that we want to allocate/map
			// here we subtract ProducerMipBias (clamped to ensure we don't fall below 0),
			// which effectively matches more detailed mips of lower resolution producers with less detailed mips of higher resolution producers
			uint32 Local_vLevel = vLevel - FMath::Min(vLevel, ProducerMipBias);

			// Wrap vAddress for the given producer
			uint32 Wrapped_vAddress = vAddress;
			{
				// Scale size of producer up to be relative to size of entire allocated VT
				const uint32 ProducerScaleFactor = (1u << ProducerMipBias);
				const uint32 ProducerWidthInPages = Producer->GetWidthInTiles() * ProducerScaleFactor;
				const uint32 ProducerHeightInPages = Producer->GetHeightInTiles() * ProducerScaleFactor;
				const uint32 AllocatedPageX = AllocatedVT->GetVirtualPageX();
				const uint32 AllocatedPageY = AllocatedVT->GetVirtualPageY();

				uint32 Local_vPageX = vPageX - AllocatedPageX;
				uint32 Local_vTileY = vPageY - AllocatedPageY;
				if (Local_vPageX >= ProducerWidthInPages || Local_vTileY >= ProducerHeightInPages)
				{
					Local_vPageX %= ProducerWidthInPages;
					Local_vTileY %= ProducerHeightInPages;
					Wrapped_vAddress = FMath::MortonCode2(Local_vPageX + AllocatedPageX) | (FMath::MortonCode2(Local_vTileY + AllocatedPageY) << 1);
				}
			}

			uint32 Local_vAddress = (Wrapped_vAddress - AllocatedVT->GetVirtualAddress()) >> (Mapping_vLevel * vDimensions);

			const uint32 LocalMipBias = Producer->GetVirtualTexture()->GetLocalMipBias(Local_vLevel, Local_vAddress);
			if (LocalMipBias > 0u)
			{
				Local_vLevel += LocalMipBias;
				Local_vAddress >>= (LocalMipBias * vDimensions);
				Mapping_vLevel = FMath::Max(vLevel, LocalMipBias + ProducerMipBias);
			}

			uint8 ProducerPhysicalGroupMaskToPrefetchForLevel[16] = { 0u };
			uint32 MaxPrefetchLocal_vLevel = Local_vLevel;

			// Iterate local layers that we found unmapped
			for (uint32 ProducerGroupIndex = 0u; ProducerGroupIndex < Producer->GetNumPhysicalGroups(); ++ProducerGroupIndex)
			{
				if ((GroupMaskToLoad & (1u << ProducerGroupIndex)) == 0u)
				{
					continue;
				}

				const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerGroupIndex);
				const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();

				// Find the highest resolution tile that's currently loaded
				const uint32 pAddress = PagePool.FindNearestPageAddress(ProducerHandle, ProducerGroupIndex, Local_vAddress, Local_vLevel, MaxLevel);
				uint32 AllocatedLocal_vLevel = MaxLevel + 1u;
				if (pAddress != ~0u)
				{
					AllocatedLocal_vLevel = PagePool.GetLocalLevelForAddress(pAddress);
					check(AllocatedLocal_vLevel >= Local_vLevel);

					const uint32 Allocated_vLevel = AllocatedLocal_vLevel + ProducerMipBias;
					ensure(Allocated_vLevel <= AllocatedVT->GetMaxLevel());

					const uint32 AllocatedMapping_vLevel = FMath::Max(Allocated_vLevel, ProducerMipBias);
					const uint32 Allocated_vAddress = Wrapped_vAddress & (0xffffffff << (Allocated_vLevel * vDimensions));

					AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpace->GetID(), pAddress);

					uint32 NumMappedPages = 0u;
					for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumPageTableLayersToLoad; ++LoadLayerIndex)
					{
						const uint32 PageTableLayerIndex = PageTableLayersToLoad[LoadLayerIndex];
						if (AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(PageTableLayerIndex) == ProducerGroupIndex &&
							AllocatedVT->GetProducerIndexForPageTableLayer(PageTableLayerIndex) == ProducerIndex)
						{
							bool bPageWasMapped = false;

							// if we found a lower resolution tile than was requested, it may have already been mapped, check for that first
							const FTexturePageMap& PageMap = Space->GetPageMapForPageTableLayer(PageTableLayerIndex);
							const FPhysicalSpaceIDAndAddress PrevPhysicalSpaceIDAndAddress = PageMap.FindPagePhysicalSpaceIDAndAddress(Allocated_vLevel, Allocated_vAddress);
							if (PrevPhysicalSpaceIDAndAddress.Packed != ~0u)
							{
								// if this address was previously mapped, ensure that it was mapped by the same physical space
								ensure(PrevPhysicalSpaceIDAndAddress.PhysicalSpaceID == PhysicalSpace->GetID());
								// either it wasn't mapped, or it's mapped to the current physical address...
								// otherwise that means that the same local tile is mapped to two separate physical addresses, which is an error
								ensure(PrevPhysicalSpaceIDAndAddress.pAddress == pAddress);
								bPageWasMapped = true;
							}
							if (!bPageWasMapped)
							{
								// map the page now if it wasn't already mapped
								RequestList->AddDirectMappingRequest(Space->GetID(), PhysicalSpace->GetID(), PageTableLayerIndex, Allocated_vLevel, Allocated_vAddress, AllocatedMapping_vLevel, pAddress);
							}

							++NumMappedPages;
						}
					}
					check(NumMappedPages > 0u);
				}

				if (AllocatedLocal_vLevel == Local_vLevel)
				{
					// page at the requested level was already resident, no longer need to load
					GroupMaskToLoad &= ~(1u << ProducerGroupIndex);
					++NumResidentPages;
				}
				else
				{
					// page not resident...see if we want to prefetch a page with resolution incrementally larger than what's currently resident
					// this means we'll ultimately load more data, but these lower resolution pages should load much faster than the requested high resolution page
					// this should make popping less noticeable
					uint32 PrefetchLocal_vLevel = AllocatedLocal_vLevel - FMath::Min(2u, AllocatedLocal_vLevel);
					PrefetchLocal_vLevel = FMath::Min<uint32>(PrefetchLocal_vLevel, AllocatedVT->GetMaxLevel() - ProducerMipBias);
					if (PrefetchLocal_vLevel > Local_vLevel)
					{
						ProducerPhysicalGroupMaskToPrefetchForLevel[PrefetchLocal_vLevel] |= (1u << ProducerGroupIndex);
						MaxPrefetchLocal_vLevel = FMath::Max(MaxPrefetchLocal_vLevel, PrefetchLocal_vLevel);
						++NumPrefetchPages;
					}
					++NumNonResidentPages;
				}
			}

			// Check to see if we have any levels to prefetch
			for (uint32 PrefetchLocal_vLevel = Local_vLevel + 1u; PrefetchLocal_vLevel <= MaxPrefetchLocal_vLevel; ++PrefetchLocal_vLevel)
			{
				uint32 ProducerPhysicalGroupMaskToPrefetch = ProducerPhysicalGroupMaskToPrefetchForLevel[PrefetchLocal_vLevel];
				if (ProducerPhysicalGroupMaskToPrefetch != 0u)
				{
					const uint32 PrefetchLocal_vAddress = Local_vAddress >> ((PrefetchLocal_vLevel - Local_vLevel) * vDimensions);

					// If we want to prefetch any layers for a given level, need to ensure that we request all the layers that aren't currently loaded
					// This is required since the VT producer interface needs to be able to write data for all layers if desired, so we need to make sure that all layers are allocated
					for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer->GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
					{
						if ((ProducerPhysicalGroupMaskToPrefetch & (1u << ProducerPhysicalGroupIndex)) == 0u)
						{
							const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
							const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, PrefetchLocal_vAddress, PrefetchLocal_vLevel);
							if (pAddress == ~0u)
							{
								ProducerPhysicalGroupMaskToPrefetch |= (1u << ProducerPhysicalGroupIndex);
								++NumPrefetchPages;
							}
							else
							{
								// Need to mark the page as recently used, otherwise it may be evicted later this frame
								AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpace->GetID(), pAddress);
							}
						}
					}

					const uint16 LoadRequestIndex = RequestList->AddLoadRequest(FVirtualTextureLocalTile(ProducerHandle, PrefetchLocal_vAddress, PrefetchLocal_vLevel), ProducerPhysicalGroupMaskToPrefetch, PageCount);
					if (LoadRequestIndex != 0xffff)
					{
						const uint32 Prefetch_vLevel = PrefetchLocal_vLevel + ProducerMipBias;
						ensure(Prefetch_vLevel <= AllocatedVT->GetMaxLevel());
						const uint32 PrefetchMapping_vLevel = FMath::Max(Prefetch_vLevel, ProducerMipBias);
						const uint32 Prefetch_vAddress = Wrapped_vAddress & (0xffffffff << (Prefetch_vLevel * vDimensions));
						for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumPageTableLayersToLoad; ++LoadLayerIndex)
						{
							const uint32 LayerIndex = PageTableLayersToLoad[LoadLayerIndex];
							if (AllocatedVT->GetProducerIndexForPageTableLayer(LayerIndex) == ProducerIndex)
							{
								const uint32 ProducerPhysicalGroupIndex = AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(LayerIndex);
								if (ProducerPhysicalGroupMaskToPrefetch & (1u << ProducerPhysicalGroupIndex))
								{
									RequestList->AddMappingRequest(LoadRequestIndex, ProducerPhysicalGroupIndex, ID, LayerIndex, Prefetch_vAddress, Prefetch_vLevel, PrefetchMapping_vLevel);
								}
							}
						}
					}
				}
			}

			if (GroupMaskToLoad != 0u)
			{
				const uint16 LoadRequestIndex = RequestList->AddLoadRequest(FVirtualTextureLocalTile(ProducerHandle, Local_vAddress, Local_vLevel), GroupMaskToLoad, PageCount);
				if (LoadRequestIndex != 0xffff)
				{
					for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumPageTableLayersToLoad; ++LoadLayerIndex)
					{
						const uint32 LayerIndex = PageTableLayersToLoad[LoadLayerIndex];
						if (AllocatedVT->GetProducerIndexForPageTableLayer(LayerIndex) == ProducerIndex)
						{
							const uint32 ProducerPhysicalGroupIndex = AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(LayerIndex);
							if (GroupMaskToLoad & (1u << ProducerPhysicalGroupIndex))
							{
								RequestList->AddMappingRequest(LoadRequestIndex, ProducerPhysicalGroupIndex, ID, LayerIndex, Wrapped_vAddress, vLevel, Mapping_vLevel);
							}
						}
					}
				}
			}
		}
	}

	for (uint32 PhysicalSpaceID = 0u; PhysicalSpaceID < (uint32)PhysicalSpaces.Num(); ++PhysicalSpaceID)
	{
		if (PhysicalSpaces[PhysicalSpaceID] == nullptr)
		{
			continue;
		}

		FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceID);
		FPageUpdateBuffer& RESTRICT Buffer = PageUpdateBuffers[PhysicalSpaceID];

		if (Buffer.WorkingSetSize > 0u)
		{
			PhysicalSpace->IncrementWorkingSetSize(Buffer.WorkingSetSize);
		}

		if (Buffer.NumPages > 0u)
		{
			Buffer.NumPageUpdates += Buffer.NumPages;
			FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();

			FScopeLock Lock(&PagePool.GetLock());
			for (uint32 i = 0u; i < Buffer.NumPages; ++i)
			{
				PagePool.UpdateUsage(Frame, Buffer.PhysicalAddresses[i]);
			}
		}
		
		INC_DWORD_STAT_BY(STAT_NumPageUpdate, Buffer.NumPageUpdates);
	}

	INC_DWORD_STAT_BY(STAT_NumPageVisible, NumRequestsPages);
	INC_DWORD_STAT_BY(STAT_NumPageVisibleResident, NumResidentPages);
	INC_DWORD_STAT_BY(STAT_NumPageVisibleNotResident, NumNonResidentPages);
	INC_DWORD_STAT_BY(STAT_NumPagePrefetch, NumPrefetchPages);
}

void FVirtualTextureSystem::GetContinuousUpdatesToProduce(FUniqueRequestList const* RequestList, int32 MaxTilesToProduce)
{
	const int32 NumContinuousUpdateRequests = (int32)RequestList->GetNumContinuousUpdateRequests();
	const int32 MaxContinousUpdates = FMath::Min(VirtualTextureScalability::GetMaxContinuousUpdatesPerFrame(), NumContinuousUpdateRequests);

	int32 NumContinuousUpdates = 0;
	while (NumContinuousUpdates < MaxContinousUpdates && ContinuousUpdateTilesToProduce.Num() < MaxTilesToProduce)
	{
		// Note it's possible that we add a duplicate value to the TSet here, and so MappedTilesToProduce doesn't grow.
		// But ending up with fewer continuous updates then the maximum is OK.
		int32 RandomIndex = FMath::Rand() % NumContinuousUpdateRequests;
		ContinuousUpdateTilesToProduce.Add(RequestList->GetContinuousUpdateRequest(RandomIndex));
		NumContinuousUpdates++;
	}
}

void FVirtualTextureSystem::UpdateCSVStats() const
{
#if CSV_PROFILER
	SCOPE_CYCLE_COUNTER(STAT_UpdateStats);

	uint32 TotalPages = 0;
	uint32 CurrentPages = 0;
	const uint32 AgeTolerance = 5; // Include some tolerance/smoothing for previous frames
	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
		if (PhysicalSpace)
		{
			FTexturePagePool const& PagePool = PhysicalSpace->GetPagePool();
			TotalPages += PagePool.GetNumPages();
			CurrentPages += PagePool.GetNumVisiblePages(Frame > AgeTolerance ? Frame - AgeTolerance : 0);
		}
	}

	const float PhysicalPoolUsage = TotalPages > 0 ? (float)CurrentPages / (float)TotalPages : 0.f;
	CSV_CUSTOM_STAT_GLOBAL(VirtualTexturePageUsage, PhysicalPoolUsage, ECsvCustomStatOp::Set);
#endif
}

void FVirtualTextureSystem::SubmitRequestsFromLocalTileList(TArray<FVirtualTextureLocalTile>& OutDeferredTiles, const TSet<FVirtualTextureLocalTile>& LocalTileList, EVTProducePageFlags Flags, FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	for (const FVirtualTextureLocalTile& Tile : LocalTileList)
	{
		const FVirtualTextureProducerHandle ProducerHandle = Tile.GetProducerHandle();
		const FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);

		// Fill targets for each layer
		// Each producer can have multiple physical layers
		// If the phys layer is mapped then we get the textures it owns and map them into the producer local slots and set the flags
		uint32 LayerMask = 0;
		FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
		for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer.GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
		{
			FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
			FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
			const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, Tile.Local_vAddress, Tile.Local_vLevel);
			if (pAddress != ~0u)
			{
				int32 PhysicalLocalTextureIndex = 0;
				for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer.GetNumTextureLayers(); ++ProducerLayerIndex)
				{
					if (Producer.GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex) == ProducerPhysicalGroupIndex)
					{
						ProduceTarget[ProducerLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture(PhysicalLocalTextureIndex);
						ProduceTarget[ProducerLayerIndex].UnorderedAccessViewRHI = PhysicalSpace->GetPhysicalTextureUAV(PhysicalLocalTextureIndex);
						ProduceTarget[ProducerLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget(PhysicalLocalTextureIndex);
						ProduceTarget[ProducerLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
						LayerMask |= 1 << ProducerLayerIndex;
						PhysicalLocalTextureIndex++;
					}
				}
			}
		}

		if (LayerMask == 0)
		{
			// If we don't have anything mapped then we can ignore (since we only want to refresh existing mapped data)
			continue;
		}

		FVTRequestPageResult RequestPageResult = Producer.GetVirtualTexture()->RequestPageData(
			ProducerHandle, LayerMask, Tile.Local_vLevel, Tile.Local_vAddress, EVTRequestPagePriority::High);

		if (RequestPageResult.Status != EVTRequestPageStatus::Available)
		{
			// Keep the request for the next frame?
			OutDeferredTiles.Add(Tile);
			continue;
		}

		IVirtualTextureFinalizer* VTFinalizer = Producer.GetVirtualTexture()->ProducePageData(
			GraphBuilder.RHICmdList, FeatureLevel,
			Flags,
			ProducerHandle, LayerMask, Tile.Local_vLevel, Tile.Local_vAddress,
			RequestPageResult.Handle,
			ProduceTarget);

		if (VTFinalizer != nullptr)
		{
			// Add the finalizer here but note that we don't call Finalize until SubmitRequests()
			Finalizers.AddUnique(VTFinalizer);
		}
	}
}

void FVirtualTextureSystem::SubmitPreMappedRequests(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel)
{
	check(TransientCollectedPages.Num() == 0);

	{
		INC_DWORD_STAT_BY(STAT_NumMappedPageUpdate, MappedTilesToProduce.Num());
		SubmitRequestsFromLocalTileList(TransientCollectedPages, MappedTilesToProduce, EVTProducePageFlags::None, GraphBuilder, FeatureLevel);
		MappedTilesToProduce.Reset();
		MappedTilesToProduce.Append(TransientCollectedPages);
		TransientCollectedPages.Reset();
	}

	{
		INC_DWORD_STAT_BY(STAT_NumContinuousPageUpdate, ContinuousUpdateTilesToProduce.Num());
		SubmitRequestsFromLocalTileList(TransientCollectedPages, ContinuousUpdateTilesToProduce, EVTProducePageFlags::ContinuousUpdate, GraphBuilder, FeatureLevel);
		ContinuousUpdateTilesToProduce.Reset();
		TransientCollectedPages.Reset();
	}
}

void FVirtualTextureSystem::SubmitRequests(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FMemStack& MemStack, FUniqueRequestList* RequestList, bool bAsync)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	// Allocate space to hold the physical address we allocate for each page load (1 page per layer per request)
	uint32* RequestPhysicalAddress = new(MemStack, MEM_Oned) uint32[RequestList->GetNumLoadRequests() * VIRTUALTEXTURE_SPACE_MAXLAYERS];
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Submit);

		struct FProducePageDataPrepareTask
		{
			IVirtualTexture* VirtualTexture;
			EVTProducePageFlags Flags;
			FVirtualTextureProducerHandle ProducerHandle;
			uint8 LayerMask;
			uint8 vLevel;
			uint32 vAddress;
			uint64 RequestHandle;
			FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
		};

		TArray<FProducePageDataPrepareTask> PrepareTasks;
		PrepareTasks.Reserve(RequestList->GetNumLoadRequests());

		const uint32 MaxPagesProduced = VirtualTextureScalability::GetMaxPagesProducedPerFrame();
		uint32 NumStacksProduced = 0u;
		uint32 NumPagesProduced = 0u;
		uint32 NumPageAllocateFails = 0u;
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumLoadRequests(); ++RequestIndex)
		{
			const bool bLockTile = RequestList->IsLocked(RequestIndex);
			const bool bForceProduceTile = (bLockTile || !bAsync);
			const FVirtualTextureLocalTile TileToLoad = RequestList->GetLoadRequest(RequestIndex);
			const FVirtualTextureProducerHandle ProducerHandle = TileToLoad.GetProducerHandle();
			const FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);

			const uint32 ProducerPhysicalGroupMask = RequestList->GetGroupMask(RequestIndex);
			uint32 ProducerTextureLayerMask = 0;
			for (uint32 ProducerLayerIndex = 0; ProducerLayerIndex < Producer.GetNumTextureLayers(); ++ProducerLayerIndex)
			{
				if (ProducerPhysicalGroupMask & (1 << Producer.GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex)))
				{
					ProducerTextureLayerMask |= (1 << ProducerLayerIndex);
				}
			}

			const EVTRequestPagePriority Priority = bLockTile ? EVTRequestPagePriority::High : EVTRequestPagePriority::Normal;
			FVTRequestPageResult RequestPageResult = Producer.GetVirtualTexture()->RequestPageData(ProducerHandle, ProducerTextureLayerMask, TileToLoad.Local_vLevel, TileToLoad.Local_vAddress, Priority);
			if (RequestPageResult.Status == EVTRequestPageStatus::Pending && bForceProduceTile)
			{
				// If we're trying to lock this tile, we're OK producing data now (and possibly waiting) as long as data is pending
				// If we render a frame without all locked tiles loaded, may render garbage VT data, as there won't be low mip fallback for unloaded tiles
				RequestPageResult.Status = EVTRequestPageStatus::Available;
			}

			if (RequestPageResult.Status == EVTRequestPageStatus::Available && !bForceProduceTile && NumPagesProduced >= MaxPagesProduced)
			{
				// Don't produce non-locked pages yet, if we're over our limit
				RequestPageResult.Status = EVTRequestPageStatus::Pending;
			}

			bool bTileLoaded = false;
			if (RequestPageResult.Status == EVTRequestPageStatus::Invalid)
			{
				if (CVarVTVerbose.GetValueOnRenderThread())
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("vAddr %i@%i is not a valid request for AllocatedVT but is still requested."), TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
				}
			}
			else if (RequestPageResult.Status == EVTRequestPageStatus::Available)
			{
				FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
				uint32 Allocate_pAddress[VIRTUALTEXTURE_SPACE_MAXLAYERS];
				FMemory::Memset(Allocate_pAddress, 0xff);

				// try to allocate a page for each layer we need to load
				bool bProduceTargetValid = true;
				for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer.GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
				{
					// If mask isn't set, we must already have a physical tile allocated for this layer, don't need to allocate another one
					if (ProducerPhysicalGroupMask & (1u << ProducerPhysicalGroupIndex))
					{
						FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
						FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
						if (PagePool.AnyFreeAvailable(Frame))
						{
							const uint32 pAddress = PagePool.Alloc(this, Frame, ProducerHandle, ProducerPhysicalGroupIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel, bLockTile);
							check(pAddress != ~0u);

							int32 PhysicalLocalTextureIndex = 0;
							for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer.GetNumTextureLayers(); ++ProducerLayerIndex)
							{
								if (Producer.GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex) == ProducerPhysicalGroupIndex)
								{
									ProduceTarget[ProducerLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].UnorderedAccessViewRHI = PhysicalSpace->GetPhysicalTextureUAV(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
									
									PhysicalLocalTextureIndex++;

									Allocate_pAddress[ProducerPhysicalGroupIndex] = pAddress;
								}
							}

							++NumPagesProduced;
						}
						else
						{
							static bool bWarnedOnce = false;
							if (!bWarnedOnce)
							{
								UE_LOG(LogConsoleResponse, Display, TEXT("Failed to allocate VT page from pool %d"), PhysicalSpace->GetID());
								for (int TextureIndex = 0; TextureIndex < PhysicalSpace->GetDescription().NumLayers; ++TextureIndex)
								{
									const FPixelFormatInfo& PoolFormatInfo = GPixelFormats[PhysicalSpace->GetFormat(TextureIndex)];
									UE_LOG(LogConsoleResponse, Display, TEXT("  PF_%s"), PoolFormatInfo.Name);
								}
								bWarnedOnce = true;
							}
							bProduceTargetValid = false;
							NumPageAllocateFails++;
							break;
						}
					}
				}

				if (bProduceTargetValid)
				{
					// Successfully allocated required pages, now we can make the request
					for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer.GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
					{
						if (ProducerPhysicalGroupMask & (1u << ProducerPhysicalGroupIndex))
						{
							// Associate the addresses we allocated with this request, so they can be mapped if required
							const uint32 pAddress = Allocate_pAddress[ProducerPhysicalGroupIndex];
							check(pAddress != ~0u);
							RequestPhysicalAddress[RequestIndex * VIRTUALTEXTURE_SPACE_MAXLAYERS + ProducerPhysicalGroupIndex] = pAddress;
						}
						else
						{
							// Fill in pAddress for layers that are already resident
							const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
							const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
							checkf(pAddress != ~0u,
								TEXT("%s missing tile: LayerMask: %X, Layer %d, vAddress %06X, vLevel %d"),
								*Producer.GetName().ToString(), ProducerPhysicalGroupMask, ProducerPhysicalGroupIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
							
							int32 PhysicalLocalTextureIndex = 0;
							for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer.GetNumTextureLayers(); ++ProducerLayerIndex)
							{
								if (Producer.GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex) == ProducerPhysicalGroupIndex)
								{
									ProduceTarget[ProducerLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].UnorderedAccessViewRHI = PhysicalSpace->GetPhysicalTextureUAV(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
									PhysicalLocalTextureIndex++;
								}
							}
						}
					}

					{
						FProducePageDataPrepareTask& Task = PrepareTasks.AddDefaulted_GetRef();
						Task.VirtualTexture = Producer.GetVirtualTexture();
						Task.Flags = EVTProducePageFlags::None;
						Task.ProducerHandle = ProducerHandle;
						Task.LayerMask = ProducerTextureLayerMask;
						Task.vLevel = TileToLoad.Local_vLevel;
						Task.vAddress = TileToLoad.Local_vAddress;
						Task.RequestHandle = RequestPageResult.Handle;
						FMemory::Memcpy(Task.ProduceTarget, ProduceTarget, sizeof(ProduceTarget));
					}

					bTileLoaded = true;
					++NumStacksProduced;
				}
				else
				{
					// Failed to allocate required physical pages for the tile, free any pages we did manage to allocate
					for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer.GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
					{
						const uint32 pAddress = Allocate_pAddress[ProducerPhysicalGroupIndex];
						if (pAddress != ~0u)
						{
							FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
							FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							PagePool.Free(this, pAddress);
						}
					}
				}
			}

			if (bLockTile && !bTileLoaded)
			{
				// Want to lock this tile, but didn't manage to load it this frame, add it back to the list to try the lock again next frame
				TilesToLock.Add(TileToLoad);
			}
		}

		if (PrepareTasks.Num())
		{
			FGraphEventArray ProducePageTasks;
			ProducePageTasks.Reserve(PrepareTasks.Num());

			for (FProducePageDataPrepareTask& Task : PrepareTasks)
			{
				Task.VirtualTexture->GatherProducePageDataTasks(Task.RequestHandle, ProducePageTasks);
			}

			static bool bWaitForTasks = true;
			if (bWaitForTasks)
			{
				QUICK_SCOPE_CYCLE_COUNTER(ProcessRequests_Wait);
				FTaskGraphInterface::Get().WaitUntilTasksComplete(ProducePageTasks, ENamedThreads::GetRenderThread_Local());
			}

			for (FProducePageDataPrepareTask& Task : PrepareTasks)
			{
				IVirtualTextureFinalizer* VTFinalizer = Task.VirtualTexture->ProducePageData(GraphBuilder.RHICmdList, FeatureLevel,
					Task.Flags,
					Task.ProducerHandle, Task.LayerMask, Task.vLevel, Task.vAddress,
					Task.RequestHandle,
					Task.ProduceTarget);

				if (VTFinalizer)
				{
					Finalizers.AddUnique(VTFinalizer); // we expect the number of unique finalizers to be very limited. if this changes, we might have to do something better then gathering them every update
				}
			}
		}

		INC_DWORD_STAT_BY(STAT_NumStacksRequested, RequestList->GetNumLoadRequests());
		INC_DWORD_STAT_BY(STAT_NumStacksProduced, NumStacksProduced);
		INC_DWORD_STAT_BY(STAT_NumPageAllocateFails, NumPageAllocateFails);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Map);

		// Update page mappings that were directly requested
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumDirectMappingRequests(); ++RequestIndex)
		{
			const FDirectMappingRequest MappingRequest = RequestList->GetDirectMappingRequest(RequestIndex);
			FVirtualTextureSpace* Space = GetSpace(MappingRequest.SpaceID);
			FVirtualTexturePhysicalSpace* PhysicalSpace = GetPhysicalSpace(MappingRequest.PhysicalSpaceID);

			PhysicalSpace->GetPagePool().MapPage(Space, PhysicalSpace, MappingRequest.PageTableLayerIndex, MappingRequest.vLevel, MappingRequest.vAddress, MappingRequest.Local_vLevel, MappingRequest.pAddress);
		}

		// Update page mappings for any requested page that completed allocation this frame
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumMappingRequests(); ++RequestIndex)
		{
			const FMappingRequest MappingRequest = RequestList->GetMappingRequest(RequestIndex);
			const uint32 pAddress = RequestPhysicalAddress[MappingRequest.LoadRequestIndex * VIRTUALTEXTURE_SPACE_MAXLAYERS + MappingRequest.ProducerPhysicalGroupIndex];
			if (pAddress != ~0u)
			{
				const FVirtualTextureLocalTile& TileToLoad = RequestList->GetLoadRequest(MappingRequest.LoadRequestIndex);
				const FVirtualTextureProducerHandle ProducerHandle = TileToLoad.GetProducerHandle();
				FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);
				FVirtualTexturePhysicalSpace* PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(MappingRequest.ProducerPhysicalGroupIndex);
				FVirtualTextureSpace* Space = GetSpace(MappingRequest.SpaceID);
				check(RequestList->GetGroupMask(MappingRequest.LoadRequestIndex) & (1u << MappingRequest.ProducerPhysicalGroupIndex));

				PhysicalSpace->GetPagePool().MapPage(Space, PhysicalSpace, MappingRequest.PageTableLayerIndex, MappingRequest.vLevel, MappingRequest.vAddress, MappingRequest.Local_vLevel, pAddress);
			}
		}
	}

	// Map any resident tiles to newly allocated VTs
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MapNew);

		uint32 Index = 0u;
		while (Index < (uint32)AllocatedVTsToMap.Num())
		{
			const FAllocatedVirtualTexture* AllocatedVT = AllocatedVTsToMap[Index];
			const uint32 vDimensions = AllocatedVT->GetDimensions();
			const uint32 BaseTileX = AllocatedVT->GetVirtualPageX();
			const uint32 BaseTileY = AllocatedVT->GetVirtualPageY();
			FVirtualTextureSpace* Space = AllocatedVT->GetSpace();

			uint32 NumFullyMappedLayers = 0u;
			for (uint32 PageTableLayerIndex = 0u; PageTableLayerIndex < AllocatedVT->GetNumPageTableLayers(); ++PageTableLayerIndex)
			{
				uint32 ProducerIndex = AllocatedVT->GetProducerIndexForPageTableLayer(PageTableLayerIndex);
				const FVirtualTextureProducerHandle ProducerHandle = AllocatedVT->GetUniqueProducerHandle(ProducerIndex);
				const FVirtualTextureProducer* Producer = Producers.FindProducer(ProducerHandle);
				if (!Producer)
				{
					++NumFullyMappedLayers;
					continue;
				}

				uint32 ProducerPhysicalGroupIndex = AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(PageTableLayerIndex);

				const uint32 ProducerMipBias = AllocatedVT->GetUniqueProducerMipBias(ProducerIndex);
				const uint32 WidthInTiles = Producer->GetWidthInTiles();
				const uint32 HeightInTiles = Producer->GetHeightInTiles();
				const uint32 MaxLevel = FMath::Min(Producer->GetMaxLevel(), AllocatedVT->GetMaxLevel() - ProducerMipBias);

				FVirtualTexturePhysicalSpace* PhysicalSpace = AllocatedVT->GetPhysicalSpaceForPageTableLayer(PageTableLayerIndex);
				FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
				FTexturePageMap& PageMap = Space->GetPageMapForPageTableLayer(PageTableLayerIndex);
				
				bool bIsLayerFullyMapped = false;
				for (uint32 Local_vLevel = 0; Local_vLevel <= MaxLevel; ++Local_vLevel)
				{
					const uint32 vLevel = Local_vLevel + ProducerMipBias;
					check(vLevel <= AllocatedVT->GetMaxLevel());

					const uint32 MipScaleFactor = (1u << Local_vLevel);
					const uint32 LevelWidthInTiles = FMath::DivideAndRoundUp(WidthInTiles, MipScaleFactor);
					const uint32 LevelHeightInTiles = FMath::DivideAndRoundUp(HeightInTiles, MipScaleFactor);

					uint32 NumNonResidentPages = 0u;
					for (uint32 TileY = 0u; TileY < LevelHeightInTiles; ++TileY)
					{
						for (uint32 TileX = 0u; TileX < LevelWidthInTiles; ++TileX)
						{
							const uint32 vAddress = FMath::MortonCode2(BaseTileX + (TileX << vLevel)) | (FMath::MortonCode2(BaseTileY + (TileY << vLevel)) << 1);
							uint32 pAddress = PageMap.FindPageAddress(vLevel, vAddress);
							if (pAddress == ~0u)
							{
								const uint32 Local_vAddress = FMath::MortonCode2(TileX) | (FMath::MortonCode2(TileY) << 1);

								pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, Local_vAddress, Local_vLevel);
								if (pAddress != ~0u)
								{
									PagePool.MapPage(Space, PhysicalSpace, PageTableLayerIndex, vLevel, vAddress, vLevel, pAddress);
								}
								else
								{
									++NumNonResidentPages;
								}
							}
						}
					}

					if (NumNonResidentPages == 0u && !bIsLayerFullyMapped)
					{
						bIsLayerFullyMapped = true;
						++NumFullyMappedLayers;
					}
				}
			}

			if (NumFullyMappedLayers < AllocatedVT->GetNumPageTableLayers())
			{
				++Index;
			}
			else
			{
				// Remove from list as long as we can fully map at least one mip level of the VT....this way we guarantee all tiles at least have some valid data (even if low resolution)
				// Normally we expect to be able to at least map the least-detailed mip, since those tiles should always be locked/resident
				// It's possible during loading that they may not be available for a few frames however
				AllocatedVTsToMap.RemoveAtSwap(Index, 1, false);
			}
		}

		AllocatedVTsToMap.Shrink();
	}

	// Finalize requests
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Finalize);
		for (IVirtualTextureFinalizer* VTFinalizer : Finalizers)
		{
			VTFinalizer->Finalize(GraphBuilder);
		}
		Finalizers.Reset();
	}

	// Update page tables
	{
		SCOPE_CYCLE_COUNTER(STAT_PageTableUpdates);
		for (uint32 ID = 0; ID < MaxSpaces; ID++)
		{
			if (Spaces[ID])
			{
				Spaces[ID]->ApplyUpdates(this, GraphBuilder);
			}
		}
	}

	Frame++;
}

void FVirtualTextureSystem::AllocateResources(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);
	RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTextureAllocate);

	for (uint32 ID = 0; ID < MaxSpaces; ID++)
	{
		if (Spaces[ID])
		{
			Spaces[ID]->AllocateTextures(GraphBuilder);
		}
	}
}

void FVirtualTextureSystem::CallPendingCallbacks()
{
	Producers.CallPendingCallbacks();
}

void FVirtualTextureSystem::ReleasePendingResources()
{
	ReleasePendingSpaces();
}
