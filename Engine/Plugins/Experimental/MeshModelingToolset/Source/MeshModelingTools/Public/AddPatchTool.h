// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "UObject/NoExportTypes.h"
#include "InteractiveToolBuilder.h"
#include "BaseTools/SingleClickTool.h"
#include "PreviewMesh.h"
#include "Properties/MeshMaterialProperties.h"
#include "AddPatchTool.generated.h"

class IAssetGenerationAPI;

class FDynamicMesh3;

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UAddPatchToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	IAssetGenerationAPI* AssetAPI;

	UAddPatchToolBuilder() 
	{
		AssetAPI = nullptr;
	}

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};



UCLASS()
class MESHMODELINGTOOLS_API UAddPatchToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UAddPatchToolProperties();

	/** Width of Shape */
	UPROPERTY(EditAnywhere, Category = PatchSettings, meta = (DisplayName = "Width", UIMin = "1.0", UIMax = "1000.0", ClampMin = "0.0001", ClampMax = "1000000.0"))
	float Width;

	/** Rotation around up axis */
	UPROPERTY(EditAnywhere, Category = PatchSettings, meta = (DisplayName = "Rotation", UIMin = "0.0", UIMax = "360.0"))
	float Rotation;

	/** Subdivisions */
	UPROPERTY(EditAnywhere, Category = PatchSettings, meta = (DisplayName = "Subdivisions", UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "4000"))
	int Subdivisions;

	/** Rotation around up axis */
	UPROPERTY(EditAnywhere, Category = PatchSettings, meta = (DisplayName = "Shift", UIMin = "-1000", UIMax = "1000"))
	float Shift;
};







/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UAddPatchTool : public USingleClickTool, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	virtual void SetWorld(UWorld* World);
	virtual void SetAssetAPI(IAssetGenerationAPI* AssetAPI);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;


	// IHoverBehaviorTarget interface
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;


protected:
	UPROPERTY()
	UAddPatchToolProperties* ShapeSettings;

	UPROPERTY()
	UNewMeshMaterialProperties* MaterialProperties;



	UPROPERTY()
	UPreviewMesh* PreviewMesh;

protected:
	UWorld* TargetWorld;
	IAssetGenerationAPI* AssetAPI;

	FBox WorldBounds;

	FFrame3f ShapeFrame;
	bool bPreviewValid = true;

	void UpdatePreviewPosition(const FInputDeviceRay& ClickPos);
	void UpdatePreviewMesh();

	TUniquePtr<FDynamicMesh3> BaseMesh;
	void GeneratePreviewBaseMesh();

	void GeneratePlane(FDynamicMesh3* OutMesh);
};
