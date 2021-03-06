// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InteractiveToolBuilder.h"
#include "DynamicMeshBrushTool.h"
#include "ShapeSprayTool.generated.h"

class IAssetGenerationAPI;

/**
 * UMeshSurfacePointToolBuilder override for UShapeSprayTool
 */
UCLASS(Transient)
class MESHMODELINGTOOLSEDITORONLY_API UShapeSprayToolBuilder : public UMeshSurfacePointToolBuilder
{
	GENERATED_BODY()

public:
	IAssetGenerationAPI* AssetAPI = nullptr;

	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};


/**
 * Settings UObject for UShapeSprayTool. 
 */
UCLASS(Transient)
class MESHMODELINGTOOLSEDITORONLY_API UShapeSprayToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UShapeSprayToolProperties();

	UPROPERTY(EditAnywhere, Category = Options)
	FLinearColor Color;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bRandomColor;

	UPROPERTY(EditAnywhere, Category = Options, meta = (DisplayName = "Speed", UIMin = "0.01", UIMax = "1.0"))
	float DropSpeed;

	UPROPERTY(EditAnywhere, Category = Options, meta = (DisplayName = "Shape Size", UIMin = "1.0", UIMax = "30.0"))
	float ObjectSize;

	UPROPERTY(EditAnywhere, Category = Options, meta = (DisplayName = "Repeat Per Stamp"))
	int NumSplats;

	UPROPERTY(EditAnywhere, Category = Options)
	UMaterialInterface* Material = nullptr;
};



/**
 * UShapeSprayTool is a brush-based tool that generates random points on the
 * target surface within the brush radius, and then creates small meshes
 * at those points. The accumulated meshes are appended and can
 * be emitted as a new StaticMeshComponent on Accept.
 */
UCLASS(Transient)
class MESHMODELINGTOOLSEDITORONLY_API UShapeSprayTool : public UDynamicMeshBrushTool
{
	GENERATED_BODY()

public:
	UShapeSprayTool();

	virtual void SetWorld(UWorld* World);
	virtual void SetAssetAPI(IAssetGenerationAPI* AssetAPI);

	// UInteractiveTool API

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// UMeshSurfacePointTool API
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;

protected:
	UPROPERTY()
	UShapeSprayToolProperties* Settings;

	// small meshes are accumulated here
	UPROPERTY()
	USimpleDynamicMeshComponent* AccumMeshComponent;


protected:
	UWorld* TargetWorld;
	IAssetGenerationAPI* AssetAPI;

	FDynamicMesh3 ShapeMesh;
	void UpdateShapeMesh();
	void SplatShape(const FFrame3d& LocalFrame, double Scale, FDynamicMesh3* TargetMesh);
	TArray<int> VertexMap;

	FRandomStream Random;

	virtual void EmitResult();
};
