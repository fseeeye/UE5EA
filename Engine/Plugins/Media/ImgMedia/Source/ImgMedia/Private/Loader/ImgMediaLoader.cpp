// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaLoader.h"
#include "ImgMediaGlobalCache.h"
#include "ImgMediaPrivate.h"

#include "Algo/Reverse.h"
#include "Misc/FrameRate.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/QueuedThreadPool.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#include "GenericImgMediaReader.h"
#include "IImageWrapperModule.h"
#include "IImgMediaReader.h"
#include "ImgMediaLoaderWork.h"
#include "ImgMediaScheduler.h"
#include "ImgMediaTextureSample.h"

#if IMGMEDIA_EXR_SUPPORTED_PLATFORM
	#include "ExrImgMediaReader.h"
#endif


/** Time spent loading a new image sequence. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Loader Load Sequence"), STAT_ImgMedia_LoaderLoadSequence, STATGROUP_Media);

/** Time spent releasing cache in image loader destructor. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Loader Release Cache"), STAT_ImgMedia_LoaderReleaseCache, STATGROUP_Media);


/* FImgMediaLoader structors
 *****************************************************************************/

FImgMediaLoader::FImgMediaLoader(const TSharedRef<FImgMediaScheduler, ESPMode::ThreadSafe>& InScheduler,
	const TSharedRef<FImgMediaGlobalCache, ESPMode::ThreadSafe>& InGlobalCache)
	: Frames(1)
	, ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper"))
	, Initialized(false)
	, NumLoadAhead(0)
	, NumLoadBehind(0)
	, Scheduler(InScheduler)
	, GlobalCache(InGlobalCache)
	, SequenceDim(FIntPoint::ZeroValue)
	, SequenceDuration(FTimespan::Zero())
	, SequenceFrameRate(0, 0)
	, LastRequestedFrame(INDEX_NONE)
	, UseGlobalCache(false)
{
	ResetFetchLogic();
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Created"), this);
}


FImgMediaLoader::~FImgMediaLoader()
{
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Destroyed"), this);

	// clean up work item pool
	for (auto Work : WorkPool)
	{
		delete Work;
	}

	WorkPool.Empty();

	// release cache
	{
		SCOPE_CYCLE_COUNTER(STAT_ImgMedia_LoaderReleaseCache);

		Frames.Empty();
		PendingFrameNumbers.Empty();
	}
}


/* FImgMediaLoader interface
 *****************************************************************************/

uint64 FImgMediaLoader::GetBitRate() const
{
	FScopeLock Lock(&CriticalSection);
	return SequenceDim.X * SequenceDim.Y * sizeof(uint16) * 8 * SequenceFrameRate.AsDecimal();
}


void FImgMediaLoader::GetBusyTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);
	FrameNumbersToTimeRanges(QueuedFrameNumbers, OutRangeSet);
}


void FImgMediaLoader::GetCompletedTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);

	TArray<int32> CompletedFrames;
	if (UseGlobalCache)
	{
		GlobalCache->GetIndices(SequenceName, CompletedFrames);
	}
	else
	{
		Frames.GetKeys(CompletedFrames);
	}
	FrameNumbersToTimeRanges(CompletedFrames, OutRangeSet);
}


//note: use with V1 player version only!
TSharedPtr<FImgMediaTextureSample, ESPMode::ThreadSafe> FImgMediaLoader::GetFrameSample(FTimespan Time)
{
	const uint32 FrameIndex = TimeToFrameNumber(Time);

	if (FrameIndex == INDEX_NONE)
	{
		return nullptr;
	}

	FScopeLock ScopeLock(&CriticalSection);

	const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame;
	if (UseGlobalCache)
	{
		Frame = GlobalCache->FindAndTouch(SequenceName, FrameIndex);
	}
	else
	{
		Frame = Frames.FindAndTouch(FrameIndex);
	}


	if (Frame == nullptr)
	{
		return nullptr;
	}

	const FTimespan FrameStartTime = FrameNumberToTime(FrameIndex);
	const FTimespan NextStartTime = FrameNumberToTime(FrameIndex + 1);

	auto Sample = MakeShared<FImgMediaTextureSample, ESPMode::ThreadSafe>();
	
	if (!Sample->Initialize(*Frame->Get(), SequenceDim, FMediaTimeStamp(FrameStartTime, 0), NextStartTime - FrameStartTime))
	{
		return nullptr;
	}

	return Sample;
}


void FImgMediaLoader::ResetFetchLogic()
{
	QueuedSampleFetch.LastFrameIndex = INDEX_NONE;
	// note: we can reset the sequence index here as this will be called when MFW does flush any queues - so we can start from scratch with no issues
	QueuedSampleFetch.CurrentSequenceIndex = 0;
}


float FImgMediaLoader::FindMaxOverlapInRange(int32 StartIndex, int32 EndIndex, FTimespan StartTime, FTimespan EndTime, int32 & MaxIdx) const
{
	int32 IdxInc = 0;
	if (StartIndex < EndIndex)
	{
		IdxInc = 1;
	}
	else
	{
		IdxInc = -1;
	}

	// Find index that overlaps the most and is furthest along the timeline...
	MaxIdx = -1;
	float MaxOverlap = 0.0f;
	for (uint32 Idx = StartIndex; Idx != (EndIndex + IdxInc); Idx+=IdxInc)
	{
		float Overlap = GetFrameOverlap(Idx, StartTime, EndTime);
		if (MaxOverlap < Overlap)
		{
			MaxOverlap = Overlap;
			MaxIdx = Idx;
		}
	}
	return MaxOverlap;
}


const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* FImgMediaLoader::GetFrameForBestIndex(int32 & MaxIdx, int32 LastIndex)
{
	const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame = nullptr;
	int32 IdxInc = (MaxIdx > LastIndex) ? -1 : 1;

	while (MaxIdx != (LastIndex + IdxInc))
	{
		Frame = UseGlobalCache ? GlobalCache->FindAndTouch(SequenceName, MaxIdx) : Frames.FindAndTouch(MaxIdx);
		if (Frame)
		{
			break;
		}
		MaxIdx += IdxInc;
	}
	return Frame;
}


IMediaSamples::EFetchBestSampleResult FImgMediaLoader::FetchBestVideoSampleForTimeRange(const TRange<FMediaTimeStamp>& TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample, bool bIsLoopingEnabled, float PlayRate, bool bPlaybackIsBlocking)
{
	if (IsInitialized() && TimeRange.HasLowerBound() && TimeRange.HasUpperBound())
	{
		FTimespan StartTime = TimeRange.GetLowerBoundValue().Time;
		FTimespan EndTime = TimeRange.GetUpperBoundValue().Time;

		if (bIsLoopingEnabled)
		{
			// Modulo with sequence duration to take care of looping.
			StartTime = ModuloTime(StartTime);
			EndTime = ModuloTime(EndTime);
		}

		// Get start and end frame indices for this time range.
		int32 StartIndex = TimeToFrameNumber(StartTime);
		int32 EndIndex = TimeToFrameNumber(EndTime);

		// Sanity checks on returned indices...
		if ((uint32)StartIndex == INDEX_NONE && (uint32)EndIndex == INDEX_NONE)
		{
			return IMediaSamples::EFetchBestSampleResult::NoSample;
		}

		if ((uint32)StartIndex == INDEX_NONE)
		{
			StartIndex = 0;
		}
		else if ((uint32)EndIndex == INDEX_NONE)
		{
			EndIndex = ImagePaths.Num() - 1;
		}

		// Find the frame that overlaps the most with the given range & is furthest along on the timeline
		int32 MaxIdx = -1;
		if (PlayRate >= 0.0f)
		{
			// Forward...
			if (StartIndex > EndIndex)
			{
				int32 MaxIdx1, MaxIdx2;
				float MaxOverlap1 = FindMaxOverlapInRange(StartIndex, ImagePaths.Num()-1, StartTime, FrameNumberToTime(ImagePaths.Num()), MaxIdx1);
				float MaxOverlap2 = FindMaxOverlapInRange(0, EndIndex, FTimespan::Zero(), EndTime, MaxIdx2);
				MaxIdx = (MaxOverlap2 >= MaxOverlap1) ? MaxIdx2 : MaxIdx1;
			}
			else
			{
				FindMaxOverlapInRange(StartIndex, EndIndex, StartTime, EndTime, MaxIdx);
			}
		}
		else
		{
			// Backward...
			if (StartIndex > EndIndex)
			{
				int32 MaxIdx1, MaxIdx2;
				float MaxOverlap1 = FindMaxOverlapInRange(EndIndex, 0, FTimespan::Zero(), EndTime, MaxIdx1);
				float MaxOverlap2 = FindMaxOverlapInRange(ImagePaths.Num()-1, StartIndex,  StartTime, FrameNumberToTime(ImagePaths.Num()), MaxIdx2);
				MaxIdx = (MaxOverlap2 >= MaxOverlap1) ? MaxIdx2 : MaxIdx1;
			}
			else
			{
				FindMaxOverlapInRange(EndIndex, StartIndex, StartTime, EndTime, MaxIdx);
			}
		}

		// Anything?
		if (MaxIdx >= 0)
		{
			const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame;

			// Request data for the frame we would like... (in case it's not in, yet)
			RequestFrame(FrameNumberToTime(MaxIdx), PlayRate, bIsLoopingEnabled);

			// If playback is not blocking, we expect less expectancy of precision on the users side, but more need for speedy return of "some ok frame"
			// So: if we detect non-blocking playback we return a "as good sample as we can", but not always the "perfect" one we calculated
			// (still we adhere to a rough emulation of a classic output pipeline as other players have)
			if (!bPlaybackIsBlocking)
			{
				// Check what data we actually have in the cache already & attempt to go further backwards on the time line
				// to get a less then optimal frame that is still on screen and available...
				if (PlayRate >= 0.0f)
				{
					// Forward...
					if (StartIndex > EndIndex)
					{
						if (MaxIdx < StartIndex)
						{
							Frame = GetFrameForBestIndex(MaxIdx, 0);
							if (!Frame)
							{
								MaxIdx = ImagePaths.Num() - 1;
								Frame = GetFrameForBestIndex(MaxIdx, StartIndex);
							}

						}
						else
						{
							Frame = GetFrameForBestIndex(MaxIdx, StartIndex);
						}
					}
					else
					{
						Frame = GetFrameForBestIndex(MaxIdx, StartIndex);
					}
				}
				else
				{
					// Backward...
					if (StartIndex > EndIndex)
					{
						if (MaxIdx > EndIndex)
						{
							Frame = GetFrameForBestIndex(MaxIdx, ImagePaths.Num() - 1);
							if (!Frame)
							{
								MaxIdx = 0;
								Frame = GetFrameForBestIndex(MaxIdx, EndIndex);
							}

						}
						else
						{
							Frame = GetFrameForBestIndex(MaxIdx, EndIndex);
						}
					}
					else
					{
						Frame = GetFrameForBestIndex(MaxIdx, EndIndex);
					}
				}
			}
			else
			{
				// Get a frame if we have one available right now...
				Frame = UseGlobalCache ? GlobalCache->FindAndTouch(SequenceName, MaxIdx) : Frames.FindAndTouch(MaxIdx);
			}

			// Got a potential frame?
			if (Frame)
			{
				// Yes. First time (after flush)?
				int32 NewSequenceIndex = QueuedSampleFetch.CurrentSequenceIndex;
				if (QueuedSampleFetch.LastFrameIndex != INDEX_NONE)
				{
					// No. Check if we looped and need to start a new sequence...
					if (PlayRate >= 0.0f && QueuedSampleFetch.LastFrameIndex > MaxIdx)
					{
						++NewSequenceIndex;
					}
					else if (PlayRate < 0.0f && QueuedSampleFetch.LastFrameIndex < MaxIdx)
					{
						--NewSequenceIndex;
					}
					else if (ImagePaths.Num() == 1)
					{
						NewSequenceIndex = TimeRange.GetLowerBoundValue().SequenceIndex;
					}
				}

				// Different from the last one we returned?
				if ((QueuedSampleFetch.LastFrameIndex != MaxIdx) || (QueuedSampleFetch.CurrentSequenceIndex != NewSequenceIndex) || (ImagePaths.Num() == 1))
				{
					QueuedSampleFetch.LastFrameIndex = MaxIdx;
					QueuedSampleFetch.CurrentSequenceIndex = NewSequenceIndex;

					// We are clear to return it as new result... Make a sample & initialize it...
					auto Sample = MakeShared<FImgMediaTextureSample, ESPMode::ThreadSafe>();
					auto Duration = Frame->Get()->Info.FrameRate.AsInterval();
					if (Sample->Initialize(*Frame->Get(), SequenceDim, FMediaTimeStamp(FrameNumberToTime(MaxIdx), QueuedSampleFetch.CurrentSequenceIndex), FTimespan::FromSeconds(Duration)))
					{
						OutSample = Sample;
						return IMediaSamples::EFetchBestSampleResult::Ok;
					}
				}
			}
		}
	}
	return IMediaSamples::EFetchBestSampleResult::NoSample;
}


bool FImgMediaLoader::PeekVideoSampleTime(FMediaTimeStamp &TimeStamp, bool bIsLoopingEnabled, float PlayRate, const FTimespan& CurrentTime)
{
	if (IsInitialized())
	{
		int32 Idx;
		bool bNewSeq = false;

		// Do we know which index we handed out last?
		if (QueuedSampleFetch.LastFrameIndex != INDEX_NONE)
		{
			// Yes. A queue would now yield the next frame (independent of rate). See which index that would be...
			Idx = int32(QueuedSampleFetch.LastFrameIndex + FMath::Sign(PlayRate));
			int32 NumFrames = ImagePaths.Num();
			if (bIsLoopingEnabled)
			{
				if (Idx < 0)
				{
					Idx = NumFrames - 1;
					bNewSeq = true;
				}
				else if (Idx >= NumFrames)
				{
					Idx = 0;
					bNewSeq = true;
				}
			}
			else
			{
				// If we reach either end of the sequence with no looping, we have no more frames to offer
				if (Idx < 0 || Idx >= NumFrames)
				{
					return false;
				}
			}
		}
		else
		{
			// No, we don't have an index. Just compute things based on the current time given...
			Idx = TimeToFrameNumber(CurrentTime);
		}

		// If possible, fetch any existing frame data...
		// (just to see if we have any)
		const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame = ((uint32)Idx != INDEX_NONE) ? (UseGlobalCache ? GlobalCache->FindAndTouch(SequenceName, Idx) : Frames.FindAndTouch(Idx)) : nullptr;

		// Start time of this frame
		FTimespan FrameStart = FrameNumberToTime(Idx);

		// Data is present?
		if (Frame)
		{
			// Yes. Return the timing information...
			TimeStamp.Time = FrameStart;
			TimeStamp.SequenceIndex = bNewSeq ? (QueuedSampleFetch.CurrentSequenceIndex + 1) : QueuedSampleFetch.CurrentSequenceIndex;
			return true;
		}
		else
		{
			// No data. We will request it (so, like other players do) we fill our (virtual) queue at the current location automatically
			RequestFrame(FrameStart, PlayRate, bIsLoopingEnabled);
		}
	}
	return false;
}


void FImgMediaLoader::GetPendingTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);
	FrameNumbersToTimeRanges(PendingFrameNumbers, OutRangeSet);
}


IQueuedWork* FImgMediaLoader::GetWork()
{
	FScopeLock Lock(&CriticalSection);

	if (PendingFrameNumbers.Num() == 0)
	{
		return nullptr;
	}

	int32 FrameNumber = PendingFrameNumbers.Pop(false);
	FImgMediaLoaderWork* Work = (WorkPool.Num() > 0) ? WorkPool.Pop() : new FImgMediaLoaderWork(AsShared(), Reader.ToSharedRef());
	
	Work->Initialize(FrameNumber, ImagePaths[FrameNumber]);
	QueuedFrameNumbers.Add(FrameNumber);

	return Work;
}


void FImgMediaLoader::Initialize(const FString& SequencePath, const FFrameRate& FrameRateOverride, bool Loop)
{
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Initializing with %s (FrameRateOverride = %s, Loop = %i)"),
		this,
		*SequencePath,
		*FrameRateOverride.ToPrettyText().ToString(),
		Loop
	);

	check(!Initialized); // reinitialization not allowed for now

	LoadSequence(SequencePath, FrameRateOverride, Loop);
	FPlatformMisc::MemoryBarrier();

	Initialized = true;
}


bool FImgMediaLoader::RequestFrame(FTimespan Time, float PlayRate, bool Loop)
{
	const int32 FrameNumber = TimeToFrameNumber(Time);

	if ((FrameNumber == INDEX_NONE) || (FrameNumber == LastRequestedFrame))
	{
		// Make sure we call the reader even if we do no update - just in case it does anything
		Reader->OnTick();

		UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Skipping frame %i for time %s"), this, FrameNumber, *Time.ToString(TEXT("%h:%m:%s.%t")));
		return false;
	}

	UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Requesting frame %i for time %s"), this, FrameNumber, *Time.ToString(TEXT("%h:%m:%s.%t")));

	Update(FrameNumber, PlayRate, Loop);
	LastRequestedFrame = FrameNumber;

	return true;
}


/* FImgMediaLoader implementation
 *****************************************************************************/

void FImgMediaLoader::FrameNumbersToTimeRanges(const TArray<int32>& FrameNumbers, TRangeSet<FTimespan>& OutRangeSet) const
{
	if (!SequenceFrameRate.IsValid() || (SequenceFrameRate.Numerator <= 0))
	{
		return;
	}

	for (const auto FrameNumber : FrameNumbers)
	{
		const FTimespan FrameStartTime = FrameNumberToTime(FrameNumber);
		const FTimespan NextStartTime = FrameNumberToTime(FrameNumber + 1);

		OutRangeSet.Add(TRange<FTimespan>(FrameStartTime, NextStartTime));
	}
}


FTimespan FImgMediaLoader::FrameNumberToTime(uint32 FrameNumber) const
{
	return FTimespan(FMath::DivideAndRoundNearest(
		FrameNumber * SequenceFrameRate.Denominator * ETimespan::TicksPerSecond,
		(int64)SequenceFrameRate.Numerator
	));
}


void FImgMediaLoader::LoadSequence(const FString& SequencePath, const FFrameRate& FrameRateOverride, bool Loop)
{
	SCOPE_CYCLE_COUNTER(STAT_ImgMedia_LoaderLoadSequence);

	if (SequencePath.IsEmpty())
	{
		return;
	}

	// locate image sequence files
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *SequencePath, TEXT("*"));

	if (FoundFiles.Num() == 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The directory %s does not contain any image files"), *SequencePath);
		return;
	}

	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Found %i image files in %s"), this, FoundFiles.Num(), *SequencePath);

	FoundFiles.Sort();

	for (const auto& File : FoundFiles)
	{
		ImagePaths.Add(FPaths::Combine(SequencePath, File));
	}

	FScopeLock Lock(&CriticalSection);

	// create image reader
	const FString FirstExtension = FPaths::GetExtension(ImagePaths[0]);

	if (FirstExtension == TEXT("exr"))
	{
#if IMGMEDIA_EXR_SUPPORTED_PLATFORM
		// Differentiate between Uncompressed exr and the rest.
		Reader = FExrImgMediaReader::GetReader(ImagePaths[0]);
#else
		UE_LOG(LogImgMedia, Error, TEXT("EXR image sequences are currently supported on macOS and Windows only"));
		return;
#endif
	}
	else
	{
		Reader = MakeShareable(new FGenericImgMediaReader(ImageWrapperModule));
	}
	if (Reader.IsValid() == false)
	{
		UE_LOG(LogImgMedia, Error, TEXT("Reader is not valid for file %s."), *ImagePaths[0]);
		return;
	}

	const UImgMediaSettings* Settings = GetDefault<UImgMediaSettings>();
	UseGlobalCache = Settings->UseGlobalCache;
	SequenceName = FName(*SequencePath);

	// fetch sequence attributes from first image
	FImgMediaFrameInfo FirstFrameInfo;
	{
		// Try and get frame from the global cache.
		const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame = nullptr;
		if (UseGlobalCache)
		{
			Frame = GlobalCache->FindAndTouch(SequenceName, 0);
		}

		if (Frame)
		{
			FirstFrameInfo = Frame->Get()->Info;
		}
		else if (!Reader->GetFrameInfo(ImagePaths[0], FirstFrameInfo))
		{
			UE_LOG(LogImgMedia, Error, TEXT("Failed to get frame information from first image in %s"), *SequencePath);
			return;
		}
	}
	if (FirstFrameInfo.UncompressedSize == 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The first image in sequence %s does not have a valid frame size"), *SequencePath);
		return;
	}

	if (FirstFrameInfo.Dim.GetMin() <= 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The first image in sequence %s does not have a valid dimension"), *SequencePath);
		return;
	}

	SequenceDim = FirstFrameInfo.Dim;

	if (FrameRateOverride.IsValid() && (FrameRateOverride.Numerator > 0))
	{
		SequenceFrameRate = FrameRateOverride;
	}
	else
	{
		SequenceFrameRate = FirstFrameInfo.FrameRate;
	}

	SequenceDuration = FrameNumberToTime(ImagePaths.Num());

	// initialize loader
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	const SIZE_T DesiredCacheSize = Settings->CacheSizeGB * 1024 * 1024 * 1024;
	const SIZE_T CacheSize = FMath::Clamp(DesiredCacheSize, (SIZE_T)0, (SIZE_T)Stats.AvailablePhysical);

	const int32 MaxFramesToLoad = (int32)(CacheSize / FirstFrameInfo.UncompressedSize);
	const int32 NumFramesToLoad = FMath::Clamp(MaxFramesToLoad, 0, ImagePaths.Num());
	const float LoadBehindScale = FMath::Clamp(Settings->CacheBehindPercentage, 0.0f, 100.0f) / 100.0f;

	NumLoadBehind = (int32)(LoadBehindScale * NumFramesToLoad);
	NumLoadAhead = NumFramesToLoad - NumLoadBehind;

	// Giving our reader a chance to handle RAM allocation.
	// Not all readers use this, only those that need to handle large files 
	// or need to be as efficient as possible.
	Reader->PreAllocateMemoryPool(NumLoadAhead + NumLoadBehind, FirstFrameInfo.UncompressedSize);

	Frames.Empty(NumFramesToLoad);

	Update(0, 0.0f, Loop);

	// update info
	Info = TEXT("Image Sequence\n");
	Info += FString::Printf(TEXT("    Dimension: %i x %i\n"), SequenceDim.X, SequenceDim.Y);
	Info += FString::Printf(TEXT("    Format: %s\n"), *FirstFrameInfo.FormatName);
	Info += FString::Printf(TEXT("    Compression: %s\n"), *FirstFrameInfo.CompressionName);
	Info += FString::Printf(TEXT("    Frames: %i\n"), ImagePaths.Num());
	Info += FString::Printf(TEXT("    Frame Rate: %.2f (%i/%i)\n"), SequenceFrameRate.AsDecimal(), SequenceFrameRate.Numerator, SequenceFrameRate.Denominator);
}


uint32 FImgMediaLoader::TimeToFrameNumber(FTimespan Time) const
{
	if ((Time < FTimespan::Zero()) || (Time >= SequenceDuration))
	{
		return INDEX_NONE;
	}

	const double FrameTimeErrorTollerance = 0.0001;

	// note: we snap to the next best whole frame index if the compute result is with in FrameTimeErrorTollerance * FrameDuration to avoid
	// incorrect frame selection if the value passed in is just ever so slightly off
	double FrameDuration = (double)SequenceFrameRate.Numerator / SequenceFrameRate.Denominator;
	double Frame = Time.GetTotalSeconds() * FrameDuration;
	double Epsilon = FrameTimeErrorTollerance * FrameDuration;

	return uint32(Frame + Epsilon);
}


void FImgMediaLoader::Update(int32 PlayHeadFrame, float PlayRate, bool Loop)
{
	// In case reader needs to do something once per frame.
	// As an example return buffers back to the pool in ExrImgMediaReaderGpu.
	Reader->OnTick();

	// @todo gmp: ImgMedia: take PlayRate and DeltaTime into account when determining frames to load
	
	// determine frame numbers to be loaded
	TArray<int32> FramesToLoad;
	{
		const int32 NumImagePaths = ImagePaths.Num();

		FramesToLoad.Empty(NumLoadAhead + NumLoadBehind);

		int32 FrameOffset = (PlayRate >= 0.0f) ? 1 : -1;

		int32 LoadAheadCount = NumLoadAhead;
		int32 LoadAheadIndex = PlayHeadFrame;

		int32 LoadBehindCount = NumLoadBehind;
		int32 LoadBehindIndex = PlayHeadFrame - FrameOffset;

		// alternate between look ahead and look behind
		while ((LoadAheadCount > 0) || (LoadBehindCount > 0))
		{
			if (LoadAheadCount > 0)
			{
				if (LoadAheadIndex < 0)
				{
					if (Loop)
					{
						LoadAheadIndex += NumImagePaths;
					}
					else
					{
						LoadAheadCount = 0;
					}
				}
				else if (LoadAheadIndex >= NumImagePaths)
				{
					if (Loop)
					{
						LoadAheadIndex -= NumImagePaths;
					}
					else
					{
						LoadAheadCount = 0;
					}
				}

				if (LoadAheadCount > 0)
				{
					FramesToLoad.Add(LoadAheadIndex);
				}

				LoadAheadIndex += FrameOffset;
				--LoadAheadCount;
			}

			if (LoadBehindCount > 0)
			{
				if (LoadBehindIndex < 0)
				{
					if (Loop)
					{
						LoadBehindIndex += NumImagePaths;
					}
					else
					{
						LoadBehindCount = 0;
					}
				}
				else if (LoadBehindIndex >= NumImagePaths)
				{
					if (Loop)
					{
						LoadBehindIndex -= NumImagePaths;
					}
					else
					{
						LoadBehindCount = 0;
					}
				}

				if (LoadBehindCount > 0)
				{
					FramesToLoad.Add(LoadBehindIndex);
				}

				LoadBehindIndex -= FrameOffset;
				--LoadBehindCount;
			}
		}
	}

	FScopeLock ScopeLock(&CriticalSection);

	// determine queued frame numbers that can be discarded
	for (int32 Idx = QueuedFrameNumbers.Num() - 1; Idx >= 0; --Idx)
	{
		const int32 FrameNumber = QueuedFrameNumbers[Idx];

		if (!FramesToLoad.Contains(FrameNumber))
		{
			UE_LOG(LogImgMedia, Warning, TEXT("Loader %p: Removed Frame %i"), this, FrameNumber);
			QueuedFrameNumbers.RemoveAtSwap(Idx);
			Reader->CancelFrame(FrameNumber);
		}
	}

	// determine frame numbers that need to be cached
	PendingFrameNumbers.Empty();

	for (int32 FrameNumber : FramesToLoad)
	{
		bool NeedFrame = false;
		if (UseGlobalCache)
		{
			NeedFrame = GlobalCache->FindAndTouch(SequenceName, FrameNumber) == nullptr;
		}
		else
		{
			NeedFrame = Frames.FindAndTouch(FrameNumber) == nullptr;
		}
		
		if ((NeedFrame) && !QueuedFrameNumbers.Contains(FrameNumber))
		{
			PendingFrameNumbers.Add(FrameNumber);
		}
	}
	Algo::Reverse(PendingFrameNumbers);
}


/* IImgMediaLoader interface
 *****************************************************************************/

void FImgMediaLoader::NotifyWorkComplete(FImgMediaLoaderWork& CompletedWork, int32 FrameNumber, const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame)
{
	FScopeLock Lock(&CriticalSection);

	// if frame is still needed, add it to the cache
	if (QueuedFrameNumbers.Remove(FrameNumber) > 0)
	{
		if (Frame.IsValid())
		{
			UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Loaded frame %i"), this, FrameNumber);
			if (UseGlobalCache)
			{
				GlobalCache->AddFrame(ImagePaths[FrameNumber], SequenceName, FrameNumber, Frame);
			}
			else
			{
				Frames.Add(FrameNumber, Frame);
			}
		}
	}

	WorkPool.Push(&CompletedWork);
}


FTimespan FImgMediaLoader::ModuloTime(FTimespan Time)
{
	bool IsNegative = Time < FTimespan::Zero();
	
	FTimespan NewTime = Time % SequenceDuration;
	if (IsNegative)
	{
		NewTime = SequenceDuration + NewTime;
	}

	return NewTime;
}


float FImgMediaLoader::GetFrameOverlap(uint32 FrameIndex, FTimespan StartTime, FTimespan EndTime) const
{
	check(StartTime <= EndTime);
	if (StartTime == EndTime)
	{
		return 0.0f;
	}

	float Overlap = 0.0f;

	// Set up ranges.
	FTimespan FrameStartTime = FrameNumberToTime(FrameIndex);
	FTimespan FrameEndTime = FrameStartTime + FrameNumberToTime(1);

	TRange<FTimespan> FrameRange(FrameStartTime, FrameEndTime);
	TRange<FTimespan> TimeRange(StartTime, EndTime);
	TRange<FTimespan> OverlapRange = TRange<FTimespan>::Intersection(FrameRange, TimeRange);

	// Get overlap size.
	FTimespan OverlapTimespan = OverlapRange.Size<FTimespan>();
	Overlap = OverlapTimespan.GetTotalSeconds();

	return Overlap;
}
