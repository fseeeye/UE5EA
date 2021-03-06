// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundArrayShuffleNode.h"
#include "MetasoundDataTypeRegistrationMacro.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
	namespace ArrayNodeShuffleVertexNames
	{
		const FString& GetInputTriggerNextName()
		{
			static const FString Name = TEXT("Next");
			return Name;
		}

		const FString& GetInputTriggerShuffleName()
		{
			static const FString Name = TEXT("Shuffle");
			return Name;
		}

		const FString& GetInputTriggerResetName()
		{
			static const FString Name = TEXT("Reset Seed");
			return Name;
		}

		const FString& GetInputShuffleArrayName()
		{
			static const FString Name = TEXT("In Array");
			return Name;
		}

		const FString& GetInputSeedName()
		{
			static const FString Name = TEXT("Seed");
			return Name;
		}

		const FString& GetInputAutoShuffleName()
		{
			static const FString Name = TEXT("Auto Shuffle");
			return Name;
		}

		const FString& GetInputEnableSharedStateName()
		{
			static const FString Name = TEXT("Enable Shared State");
			return Name;
		}

		const FString& GetOutputTriggerOnNextName()
		{
			static const FString Name = TEXT("On Next");
			return Name;
		}

		const FString& GetOutputTriggerOnShuffleName()
		{
			static const FString Name = TEXT("On Shuffle");
			return Name;
		}

		const FString& GetOutputTriggerOnResetName()
		{
			static const FString Name = TEXT("On Reset Seed");
			return Name;
		}

		const FString& GetOutputValueName()
		{
			static const FString Name = TEXT("Value");
			return Name;
		}
	}

	/**
	* FArrayIndexShuffler
	*/

	FArrayIndexShuffler::FArrayIndexShuffler(int32 InSeed, int32 InMaxIndices)
	{
		Init(InSeed, InMaxIndices);
	}

	void FArrayIndexShuffler::Init(int32 InSeed, int32 InMaxIndices)
	{
		SetSeed(InSeed);
		if (InMaxIndices > 0)
		{
			ShuffleIndices.AddUninitialized(InMaxIndices);
			for (int32 i = 0; i < ShuffleIndices.Num(); ++i)
			{
				ShuffleIndices[i] = i;
			}
			ShuffleArray();
		}
	}

	void FArrayIndexShuffler::SetSeed(int32 InSeed)
	{
		if (InSeed == INDEX_NONE)
		{
			RandomStream.Initialize(FPlatformTime::Cycles());
		}
		else
		{
			RandomStream.Initialize(InSeed);
		}

		ResetSeed();
	}

	void FArrayIndexShuffler::ResetSeed()
	{
		RandomStream.Reset();
	}

	bool FArrayIndexShuffler::NextValue(bool bAutoShuffle, int32& OutIndex)
	{
		bool bShuffled = false;
		if (CurrentIndex == ShuffleIndices.Num())
		{
			if (bAutoShuffle)
			{
				ShuffleArray();
				bShuffled = true;
			}
			else
			{
				CurrentIndex = 0;
			}
		}

		check(CurrentIndex < ShuffleIndices.Num());
		PrevValue = ShuffleIndices[CurrentIndex];
		OutIndex = PrevValue;
		++CurrentIndex;

		return bShuffled;
	}

	// Shuffle the array with the given max indices
	void FArrayIndexShuffler::ShuffleArray()
	{
		// Randomize the shuffled array by randomly swapping indicies
		for (int32 i = 0; i < ShuffleIndices.Num(); ++i)
		{
			RandomSwap(i, 0, ShuffleIndices.Num() - 1);
		}

		// Reset the current index back to 0
		CurrentIndex = 0;

		// Fix up the new current index if the value is our previous value and we have an array larger than 1
		if (ShuffleIndices.Num() > 1 && ShuffleIndices[CurrentIndex] == PrevValue)
		{
			RandomSwap(0, 1, ShuffleIndices.Num() - 1);
		}
	}

	void FArrayIndexShuffler::RandomSwap(int32 InCurrentIndex, int32 InStartIndex, int32 InEndIndex)
	{
		int32 ShuffleIndex = RandomStream.RandRange(InStartIndex, InEndIndex);
		int32 Temp = ShuffleIndices[ShuffleIndex];
		ShuffleIndices[ShuffleIndex] = ShuffleIndices[InCurrentIndex];
		ShuffleIndices[InCurrentIndex] = Temp;
	}

	namespace ArrayNodeGetGlobalArrayKeyVertexNames
	{
		const FString& GetInputNamespaceName()
		{
			static const FString Name = TEXT("Namespace");
			return Name;		
		}

		const FString& GetInputArraySizeName()
		{
			static const FString Name = TEXT("Array Size");
			return Name;
		}

		const FString& GetInputSeedName()
		{
			static const FString Name = TEXT("Seed");
			return Name;
		}

		const FString& GetOutputArrayKeyName()
		{
			static const FString Name = TEXT("Global Array Key");
			return Name;
		}
	}

}

#undef LOCTEXT_NAMESPACE
