// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementAssetDataInterface.h"

#include "ObjectElementAssetDataInterface.generated.h"

UCLASS()
class ENGINE_API UObjectElementAssetDataInterface : public UTypedElementAssetDataInterface
{
	GENERATED_BODY()

public:
	virtual FAssetData GetAssetData(const FTypedElementHandle& InElementHandle) override;
};
