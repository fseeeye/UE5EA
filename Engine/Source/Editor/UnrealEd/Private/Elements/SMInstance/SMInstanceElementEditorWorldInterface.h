// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/SMInstance/SMInstanceElementWorldInterface.h"
#include "SMInstanceElementEditorWorldInterface.generated.h"

UCLASS()
class USMInstanceElementEditorWorldInterface : public USMInstanceElementWorldInterface
{
	GENERATED_BODY()

public:
	virtual bool CanDeleteElement(const FTypedElementHandle& InElementHandle) override;
	virtual bool DeleteElement(const FTypedElementHandle& InElementHandle, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions) override;
	virtual bool DeleteElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions) override;
	virtual bool CanDuplicateElement(const FTypedElementHandle& InElementHandle) override;
	virtual FTypedElementHandle DuplicateElement(const FTypedElementHandle& InElementHandle, UWorld* InWorld, const FVector& InLocationOffset) override;
	virtual void DuplicateElements(TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements) override;
};
