// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AssetImportTask.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAssetImportTask, Log, All);

class UFactory;

/**
 * Contains data for a group of assets to import
 */ 
UCLASS(Transient, BlueprintType)
class UNREALED_API UAssetImportTask : public UObject
{
	GENERATED_BODY()

public:
	UAssetImportTask();

public:
	/** Filename to import */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	FString Filename;

	/** Path where asset will be imported to */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	FString DestinationPath;

	/** Optional custom name to import as */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	FString DestinationName;

	/** Overwrite existing assets */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	bool bReplaceExisting;

	/** Replace existing settings when overwriting existing assets  */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	bool bReplaceExistingSettings;

	/** Avoid dialogs */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	bool bAutomated;

	/** Save after importing */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	bool bSave;

	/** Optional factory to use */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	TObjectPtr<UFactory> Factory;

	/** Import options specific to the type of asset */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	TObjectPtr<UObject> Options;

	/** Paths to objects created or updated after import */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	TArray<FString> ImportedObjectPaths;

	/** Imported objects */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	TArray<TObjectPtr<UObject>> Result;
};

