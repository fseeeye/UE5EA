// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraRendererProperties.h"
#include "StaticMeshResources.h"
#include "NiagaraCommon.h"
#include "NiagaraMeshRendererProperties.generated.h"

class FNiagaraEmitterInstance;
class FAssetThumbnailPool;
class SWidget;

/** This enum decides how a mesh particle will orient its "facing" axis relative to camera. Must keep these in sync with NiagaraMeshVertexFactory.ush*/
UENUM()
enum class ENiagaraMeshFacingMode : uint8
{
	/** Ignores the camera altogether. The mesh aligns its local-space X-axis with the particles' local-space X-axis, after transforming by the Particles.Transform vector (if it exists).*/
	Default = 0,
	/** The mesh aligns it's local-space X-axis with the particle's Particles.Velocity vector.*/
	Velocity,
	/** Has the mesh local-space X-axis point towards the camera's position.*/
	CameraPosition, 
	/** Has the mesh local-space X-axis point towards the closest point on the camera view plane.*/
	CameraPlane
};

UENUM()
enum class ENiagaraMeshPivotOffsetSpace : uint8 {
	/** The pivot offset is in the mesh's local space (default) */
	Mesh,
	/** The pivot offset is in the emitter's local space if the emitter is marked as local-space, or in world space otherwise */
	Simulation,
	/** The pivot offset is in world space */
	World,
	/** The pivot offset is in the emitter's local space */
	Local
};

UENUM()
enum class ENiagaraMeshLockedAxisSpace : uint8 {
	/** The locked axis is in the emitter's local space if the emitter is marked as local-space, or in world space otherwise */
	Simulation,
	/** The locked axis is in world space */
	World,
	/** The locked axis is in the emitter's local space */
	Local
};

USTRUCT()
struct NIAGARA_API FNiagaraMeshMaterialOverride 
{
	GENERATED_USTRUCT_BODY()
public:	
	FNiagaraMeshMaterialOverride();
	
	/** Used to upgrade a serialized FNiagaraParameterStore property to our own struct */
	bool SerializeFromMismatchedTag(const struct FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

	/** Use this UMaterialInterface if set to a valid value. This will be subordinate to UserParamBinding if it is set to a valid user variable.*/
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering")
	TObjectPtr<UMaterialInterface> ExplicitMat;

	/** Use the UMaterialInterface bound to this user variable if it is set to a valid value. If this is bound to a valid value and ExplicitMat is also set, UserParamBinding wins.*/
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering")
	FNiagaraUserParameterBinding UserParamBinding;
};

template<>
struct TStructOpsTypeTraits<FNiagaraMeshMaterialOverride> : public TStructOpsTypeTraitsBase2<FNiagaraMeshMaterialOverride>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

namespace ENiagaraMeshVFLayout
{
	enum Type
	{
		Position,
		Velocity,
		Color,
		Scale,
		Transform,
		MaterialRandom,
		NormalizedAge,
		CustomSorting,
		SubImage,
		DynamicParam0,
		DynamicParam1,
		DynamicParam2,
		DynamicParam3,
		CameraOffset,

		Num,
	};
};

USTRUCT()
struct NIAGARA_API FNiagaraMeshRendererMeshProperties
{
	GENERATED_BODY()

	FNiagaraMeshRendererMeshProperties();

	/** The mesh to use when rendering this slot */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	TObjectPtr<UStaticMesh> Mesh;

	/** Scale of the mesh */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	FVector Scale;

	/** Offset of the mesh pivot */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	FVector PivotOffset;

	/** What space is the pivot offset in? */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	ENiagaraMeshPivotOffsetSpace PivotOffsetSpace;
};

UCLASS(editinlinenew, meta = (DisplayName = "Mesh Renderer"))
class NIAGARA_API UNiagaraMeshRendererProperties : public UNiagaraRendererProperties
{
public:
	GENERATED_BODY()

	UNiagaraMeshRendererProperties();

	//UObject Interface
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITORONLY_DATA
	virtual void BeginDestroy() override;
	virtual void PreEditChange(class FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif// WITH_EDITORONLY_DATA
	//UObject Interface END

	static void InitCDOPropertiesAfterModuleStartup();

	//~ UNiagaraRendererProperties interface
	virtual FNiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, const FNiagaraEmitterInstance* Emitter, const UNiagaraComponent* InComponent) override;
	virtual class FNiagaraBoundsCalculator* CreateBoundsCalculator() override;
	virtual void GetUsedMaterials(const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials) const override;
	virtual bool IsSimTargetSupported(ENiagaraSimTarget InSimTarget) const override { return true; };

#if WITH_EDITORONLY_DATA
	virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) override;
	virtual void FixMaterial(UMaterial* Material) override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
	virtual	void GetRendererWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const override;
	virtual	void GetRendererTooltipWidgets(const FNiagaraEmitterInstance* InEmitter, TArray<TSharedPtr<SWidget>>& OutWidgets, TSharedPtr<FAssetThumbnailPool> InThumbnailPool) const override;
	virtual void GetRendererFeedback(const UNiagaraEmitter* InEmitter, TArray<FText>& OutErrors, TArray<FText>& OutWarnings, TArray<FText>& OutInfo) const override;
	void OnMeshChanged();
	void OnMeshPostBuild(UStaticMesh*);
	void OnAssetReimported(UObject*);
	void CheckMaterialUsage();
#endif // WITH_EDITORONLY_DATA
	virtual void CacheFromCompiledData(const FNiagaraDataSetCompiledData* CompiledData) override;
	//UNiagaraRendererProperties Interface END

	void GetUsedMeshMaterials(int32 MeshIndex, const FNiagaraEmitterInstance* Emitter, TArray<UMaterialInterface*>& OutMaterials) const;

	/**
	 * The static mesh(es) to be instanced when rendering mesh particles.
	 * 
	 * NOTES:
	 * - If "Override Material" is not specified, the mesh's material is used. Override materials must have the Niagara Mesh Particles flag checked.
	 * - If "Enable Mesh Flipbook" is specified, this mesh is assumed to be the first frame of the flipbook.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering", meta = (EditCondition = "!bEnableMeshFlipbook"))
	TArray<FNiagaraMeshRendererMeshProperties> Meshes;

	/** Determines how we sort the particles prior to rendering.*/
	UPROPERTY(EditAnywhere, Category = "Sorting")
	ENiagaraSortMode SortMode;

	/** Whether or not to use the OverrideMaterials array instead of the mesh's existing materials.*/
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering", meta = (InlineEditConditionToggle))
	uint32 bOverrideMaterials : 1;

	/** If true, the particles are only sorted when using a translucent material. */
	UPROPERTY(EditAnywhere, Category = "Sorting")
	uint32 bSortOnlyWhenTranslucent : 1;

	/** If true, blends the sub-image UV lookup with its next adjacent member using the fractional part of the SubImageIndex float value as the linear interpolation factor.*/
	UPROPERTY(EditAnywhere, Category = "SubUV", meta = (DisplayName = "Sub UV Blending Enabled"))
	uint32 bSubImageBlend : 1;
	
	/** Enables frustum culling of individual mesh particles */
	UPROPERTY(EditAnywhere, Category = "Visibility")
	uint32 bEnableFrustumCulling : 1;

	/** Enables frustum culling of individual mesh particles */
	UPROPERTY(EditAnywhere, Category = "Visibility")
	uint32 bEnableCameraDistanceCulling : 1;

	/** When checked, will treat 'ParticleMesh' as the first frame of the flipbook, and will use the other mesh flipbook options to find the other frames */
	UPROPERTY(EditAnywhere, Category = "Mesh Flipbook")
	uint32 bEnableMeshFlipbook : 1;

	/** The materials to be used instead of the StaticMesh's materials. Note that each material must have the Niagara Mesh Particles flag checked. If the ParticleMesh 
	requires more materials than exist in this array or any entry in this array is set to None, we will use the ParticleMesh's existing Material instead.*/
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering", meta = (EditCondition = "bOverrideMaterials"))
	TArray<FNiagaraMeshMaterialOverride> OverrideMaterials;

	/** When using SubImage lookups for particles, this variable contains the number of columns in X and the number of rows in Y.*/
	UPROPERTY(EditAnywhere, Category = "SubUV")
	FVector2D SubImageSize;

	/** Determines how the mesh orients itself relative to the camera. */
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering")
	ENiagaraMeshFacingMode FacingMode;

	/** If true and in a non-default facing mode, will lock facing direction to an arbitrary plane of rotation */
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering")
	uint32 bLockedAxisEnable : 1;

	/** Arbitrary axis by which to lock facing rotations */
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering", meta = (EditCondition = "bLockedAxisEnable"))
	FVector LockedAxis;

	/** Specifies what space the locked axis is in */
	UPROPERTY(EditAnywhere, Category = "Mesh Rendering", meta = (EditCondition = "bLockedAxisEnable"))
	ENiagaraMeshLockedAxisSpace LockedAxisSpace;
	
	UPROPERTY(EditAnywhere, Category = "Visibility", meta = (EditCondition = "bEnableCameraDistanceCulling", ClampMin = 0.0f))
	float MinCameraDistance;
	
	UPROPERTY(EditAnywhere, Category = "Visibility", meta = (EditCondition = "bEnableCameraDistanceCulling", ClampMin = 0.0f))
	float MaxCameraDistance = 1000.0f;

	/** If a render visibility tag is present, particles whose tag matches this value will be visible in this renderer. */
	UPROPERTY(EditAnywhere, Category = "Visibility")
	uint32 RendererVisibility = 0;
	
	/** Which attribute should we use for position when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding PositionBinding;

	/** Which attribute should we use for color when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding ColorBinding;

	/** Which attribute should we use for velocity when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding VelocityBinding;

	/** Which attribute should we use for orienting meshes when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding MeshOrientationBinding;

	/** Which attribute should we use for scale when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding ScaleBinding;
	
	/** Which attribute should we use for sprite sub-image indexing when generating sprites?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding SubImageIndexBinding;

	/** Which attribute should we use for dynamic material parameters when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterialBinding;

	/** Which attribute should we use for dynamic material parameters when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterial1Binding;

	/** Which attribute should we use for dynamic material parameters when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterial2Binding;
	
	/** Which attribute should we use for dynamic material parameters when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding DynamicMaterial3Binding;

	/** Which attribute should we use for material randoms when generating instanced meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding MaterialRandomBinding;

	/** Which attribute should we use custom sorting of particles in this emitter. */
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding CustomSortingBinding;

	/** Which attribute should we use for Normalized Age? */
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding NormalizedAgeBinding;

	/** Which attribute should we use for camera offset when rendering meshes?*/
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding CameraOffsetBinding;

	/** Which attribute should we use for the renderer visibility tag? */
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding RendererVisibilityTagBinding;

	/** Which attribute should we use to pick the element in the mesh array on the mesh renderer? */
	UPROPERTY(EditAnywhere, Category = "Bindings")
	FNiagaraVariableAttributeBinding MeshIndexBinding;
	
#if WITH_EDITORONLY_DATA
	/** 
	 * The static mesh to use for the first frame of the flipbook. Its name will also be used to find subsequent frames of a similar name.
	 * NOTE: The subsequent frames are expected to exist in the same content directory as the first frame of the flipbook, otherwise they
	 * will not be found or used.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh Flipbook", meta = (EditCondition = "bEnableMeshFlipbook"))
	TObjectPtr<UStaticMesh> FirstFlipbookFrame;

	/**
	 * Provides the format of the suffix of the names of the static meshes when searching for flipbook frames. "{frame_number}" is used to mark
	 * where the frame number should appear in the suffix. If "Particle Mesh" contains this suffix, the number in its name will be treated as
	 * the starting frame index. Otherwise, it will assume "Particle Mesh" is frame number 0, and that subsequent frames follow this format,
	 * starting with frame number 1.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh Flipbook", meta = (EditCondition = "bEnableMeshFlipbook && FirstFlipbookFrame != nullptr"))
	FString FlipbookSuffixFormat;

	/**
	* The number of digits to expect in the frame number of the flipbook page. A value of 1 will expect no leading zeros in the package names,
	* and can also be used for names with frame numbers that extend to 10 and beyond (Example: Frame_1, Frame_2, ..., Frame_10, Frame_11, etc.)
	*/
	UPROPERTY(EditAnywhere, Category = "Mesh Flipbook", meta = (EditCondition = "bEnableMeshFlipbook && FirstFlipbookFrame != nullptr", ClampMin = 1, ClampMax = 10, NoSpinbox = true))
	uint32 FlipbookSuffixNumDigits;

	/** The number of frames (static meshes) to be included in the flipbook. */
	UPROPERTY(EditAnywhere, Category = "Mesh Flipbook", meta = (EditCondition = "bEnableMeshFlipbook && FirstFlipbookFrame != nullptr", ClampMin = 1, NoSpinbox = true))
	uint32 NumFlipbookFrames;
#endif	

	uint32 MaterialParamValidMask = 0;
	FNiagaraRendererLayout RendererLayoutWithCustomSorting;
	FNiagaraRendererLayout RendererLayoutWithoutCustomSorting;

protected:
	bool FindBinding(const FNiagaraUserParameterBinding& InBinding, const FNiagaraEmitterInstance* InEmitter, TArray<UMaterialInterface*>& OutMaterials);
	void InitBindings();

#if WITH_EDITORONLY_DATA
	bool ChangeRequiresMeshListRebuild(const FProperty* Property);
	void RebuildMeshList();
#endif

private:
	static TArray<TWeakObjectPtr<UNiagaraMeshRendererProperties>> MeshRendererPropertiesToDeferredInit;

	// These properties are deprecated and moved to FNiagaraMeshRendererMeshProperties
	UPROPERTY()
	TObjectPtr<UStaticMesh> ParticleMesh_DEPRECATED;

	UPROPERTY()
	FVector PivotOffset_DEPRECATED;

	UPROPERTY()
	ENiagaraMeshPivotOffsetSpace PivotOffsetSpace_DEPRECATED;


};
