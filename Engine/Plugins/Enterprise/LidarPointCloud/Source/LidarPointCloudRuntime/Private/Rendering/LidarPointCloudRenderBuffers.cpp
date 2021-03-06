// Copyright Epic Games, Inc. All Rights Reserved.

#include "LidarPointCloudRenderBuffers.h"
#include "RenderResource.h"
#include "VertexFactory.h"
#include "MeshBatch.h"
#include "RenderCommandFence.h"
#include "LidarPointCloudOctree.h"
#include "LidarPointCloudSettings.h"
#include "MeshMaterialShader.h"

#if WITH_EDITOR
#include "Classes/EditorStyleSettings.h"
#endif

#define BINDPARAM(Name) Name.Bind(ParameterMap, TEXT(#Name))
#define SETPARAM(Name) if (Name.IsBound()) { ShaderBindings.Add(Name, UserData->Name); }
#define SETSRVPARAM(Name) if(UserData->Name) { SETPARAM(Name) }

//////////////////////////////////////////////////////////// Base Buffer

TGlobalResource<FLidarPointCloudIndexBuffer> GLidarPointCloudIndexBuffer;
TGlobalResource<FLidarPointCloudVertexFactory> GLidarPointCloudVertexFactory;

//////////////////////////////////////////////////////////// Index Buffer

void FLidarPointCloudIndexBuffer::Resize(const uint32 & RequestedCapacity)
{
	// This must be called from Rendering thread
	check(IsInRenderingThread());

	if (Capacity != RequestedCapacity)
	{
		ReleaseResource();
		Capacity = RequestedCapacity;
		InitResource();
	}
}

void FLidarPointCloudIndexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo(TEXT("FLidarPointCloudIndexBuffer"));
	void* Buffer = nullptr;
	uint32 Size = Capacity * 7 * sizeof(uint32);
	PointOffset = Capacity * 6;

	IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(uint32), Size, BUF_Dynamic, CreateInfo, Buffer);

	uint32* Data = (uint32*)Buffer;
	for (uint32 i = 0; i < Capacity; i++)
	{
		// Full quads
		{
			uint32 idx = i * 6;
			uint32 v = i * 4;

			Data[idx] = v;
			Data[idx + 1] = v + 1;
			Data[idx + 2] = v + 2;
			Data[idx + 3] = v;
			Data[idx + 4] = v + 2;
			Data[idx + 5] = v + 3;
		}

		// Points
		Data[PointOffset + i] = i;
	}

	RHIUnlockBuffer(IndexBufferRHI);
	Buffer = nullptr;
}

//////////////////////////////////////////////////////////// Structured Buffer

void FLidarPointCloudRenderBuffer::Resize(const uint32& RequestedCapacity)
{
	// This must be called from Rendering thread
	check(IsInRenderingThread());

	if (Capacity != RequestedCapacity)
	{
		ReleaseResource();
		Capacity = RequestedCapacity;
		InitResource();
	}
	else if (!IsInitialized())
	{
		InitResource();
	}
}

void FLidarPointCloudRenderBuffer::InitRHI()
{
	// This must be called from Rendering thread
	check(IsInRenderingThread());

	FRHIResourceCreateInfo CreateInfo(TEXT("FLidarPointCloudRenderBuffer"));
	Buffer = RHICreateVertexBuffer(sizeof(uint32) * Capacity, BUF_ShaderResource | BUF_Dynamic, CreateInfo);
	SRV = RHICreateShaderResourceView(Buffer, sizeof(uint32), PF_R32_FLOAT);
}

void FLidarPointCloudRenderBuffer::ReleaseRHI()
{
	// This must be called from Rendering thread
	check(IsInRenderingThread());

	if (Buffer)
	{
		RHIDiscardTransientResource(Buffer);
		Buffer.SafeRelease();
	}

	SRV.SafeRelease();
}

//////////////////////////////////////////////////////////// User Data

FLidarPointCloudBatchElementUserData::FLidarPointCloudBatchElementUserData()
	: SelectionColor(FVector::OneVector)
	, IndexDivisor(4)
	, SpriteSize(0)
	, bUseLODColoration(false)
	, NumClippingVolumes(0)
	, bStartClipped(false)
{
	for (int32 i = 0; i < 16; ++i)
	{
		ClippingVolume[i] = FMatrix(FPlane(FVector::ZeroVector, 0),
									FPlane(FVector::ForwardVector, FLT_MAX),
									FPlane(FVector::RightVector, FLT_MAX),
									FPlane(FVector::UpVector, FLT_MAX));
	}

#if WITH_EDITOR
	SelectionColor = FVector(GetDefault<UEditorStyleSettings>()->SelectionColor.ToFColor(true));
#endif
}

void FLidarPointCloudBatchElementUserData::SetClassificationColors(const TMap<int32, FLinearColor>& InClassificationColors)
{
	for (int32 i = 0; i < 32; ++i)
	{
		const FLinearColor* Color = InClassificationColors.Find(i);
		ClassificationColors[i] = Color ? FVector4(*Color) : FVector4(1, 1, 1);
	}
}

//////////////////////////////////////////////////////////// Vertex Factory

void FLidarPointCloudVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	BINDPARAM(DataBuffer);
	BINDPARAM(bEditorView);
	BINDPARAM(SelectionColor);
	BINDPARAM(IndexDivisor);
	BINDPARAM(LocationOffset);
	BINDPARAM(VirtualDepth);
	BINDPARAM(SpriteSize);
	BINDPARAM(bUseLODColoration);
	BINDPARAM(SpriteSizeMultiplier);
	BINDPARAM(ViewRightVector);
	BINDPARAM(ViewUpVector);
	BINDPARAM(bUseCameraFacing);
	BINDPARAM(bUseScreenSizeScaling);
	BINDPARAM(BoundsSize);
	BINDPARAM(ElevationColorBottom);
	BINDPARAM(ElevationColorTop);
	BINDPARAM(bUseCircle);
	BINDPARAM(bUseColorOverride);
	BINDPARAM(bUseElevationColor);
	BINDPARAM(Offset);
	BINDPARAM(Contrast);
	BINDPARAM(Saturation);
	BINDPARAM(Gamma);
	BINDPARAM(Tint);
	BINDPARAM(IntensityInfluence);
	BINDPARAM(bUseClassification);
	BINDPARAM(ClassificationColors);
	BINDPARAM(ClippingVolume);
	BINDPARAM(NumClippingVolumes);
	BINDPARAM(bStartClipped);
}

void FLidarPointCloudVertexFactoryShaderParameters::GetElementShaderBindings(const class FSceneInterface* Scene, const FSceneView* View, const FMeshMaterialShader* Shader, const EVertexInputStreamType InputStreamType, ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory, const FMeshBatchElement& BatchElement, class FMeshDrawSingleShaderBindings& ShaderBindings, FVertexInputStreamArray& VertexStreams) const
{
	FLidarPointCloudBatchElementUserData* UserData = (FLidarPointCloudBatchElementUserData*)BatchElement.UserData;

	SETSRVPARAM(DataBuffer);
	SETPARAM(bEditorView);
	SETPARAM(SelectionColor);
	SETPARAM(IndexDivisor);
	SETPARAM(LocationOffset);
	SETPARAM(VirtualDepth);
	SETPARAM(SpriteSize);
	SETPARAM(bUseLODColoration);
	SETPARAM(SpriteSizeMultiplier);
	SETPARAM(ViewRightVector);
	SETPARAM(ViewUpVector);
	SETPARAM(bUseCameraFacing);
	SETPARAM(bUseScreenSizeScaling);
	SETPARAM(BoundsSize);
	SETPARAM(ElevationColorBottom);
	SETPARAM(ElevationColorTop);
	SETPARAM(bUseCircle);
	SETPARAM(bUseColorOverride);
	SETPARAM(bUseElevationColor);
	SETPARAM(Offset);
	SETPARAM(Contrast);
	SETPARAM(Saturation);
	SETPARAM(Gamma);
	SETPARAM(Tint);
	SETPARAM(IntensityInfluence);
	SETPARAM(bUseClassification);
	SETPARAM(ClassificationColors);
	SETPARAM(ClippingVolume);
	SETPARAM(NumClippingVolumes);
	SETPARAM(bStartClipped);
}

bool FLidarPointCloudVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return (IsPCPlatform(Parameters.Platform) && IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
		Parameters.MaterialParameters.MaterialDomain == MD_Surface && Parameters.MaterialParameters.bIsUsedWithLidarPointCloud) || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

void FLidarPointCloudVertexFactory::InitRHI()
{
	VertexBuffer.InitResource();

	FVertexDeclarationElementList Elements;
	Elements.Add(AccessStreamComponent(FVertexStreamComponent(&VertexBuffer, 0, 0, VET_Float3), 0));
	InitDeclaration(Elements);
}

void FLidarPointCloudVertexFactory::ReleaseRHI()
{
	FVertexFactory::ReleaseRHI();
	VertexBuffer.ReleaseResource();
}

IMPLEMENT_TYPE_LAYOUT(FLidarPointCloudVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FLidarPointCloudVertexFactory, SF_Vertex, FLidarPointCloudVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FLidarPointCloudVertexFactory, "/Plugin/LidarPointCloud/Private/LidarPointCloudVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPositionOnly
);

#undef BINDPARAM
#undef SETPARAM
#undef SETSRVPARAM
