// Copyright Epic Games, Inc. All Rights Reserved.

#include "Drawing/MeshWireframeComponent.h"

#include "Engine/CollisionProfile.h"
#include "LocalVertexFactory.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "StaticMeshResources.h"
#include "IndexTypes.h"
#include "Async/ParallelFor.h"

struct FWireframeLinesMeshBatchData
{
	FWireframeLinesMeshBatchData()
		: MaterialProxy(nullptr)
	{}

	FMaterialRenderProxy* MaterialProxy;
	int32 StartIndex;
	int32 NumPrimitives;
	int32 MinVertexIndex;
	int32 MaxVertexIndex;
};





/** Class for the MeshWireframeComponent data passed to the render thread. */
class FMeshWireframeSceneProxy final : public FPrimitiveSceneProxy
{
public:

	FMeshWireframeSceneProxy(UMeshWireframeComponent* Component, const IMeshWireframeSource* WireSource)
		: FPrimitiveSceneProxy(Component),
		MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel())),
		VertexFactory(GetScene().GetFeatureLevel(), "FPointSetSceneProxy")
	{
		if (!ensure(WireSource && WireSource->IsValid())) return;

		if (WireSource->GetEdgeCount() <= 0) return;

		// count visible edges and remap so we can build in parallel below
		int32 MaxEdgeIndex = WireSource->GetMaxEdgeIndex();
		CurrentEdgeSet.Reserve(WireSource->GetEdgeCount());
		for (int32 li = 0; li < MaxEdgeIndex; ++li)
		{
			if (WireSource->IsEdge(li) == false) continue;

			int32 VertIndexA, VertIndexB;
			IMeshWireframeSource::EMeshEdgeType EdgeType;
			WireSource->GetEdge(li, VertIndexA, VertIndexB, EdgeType);

			bool bEdgeIsVisible = false;
			if (Component->bEnableWireframe)
			{
				bEdgeIsVisible = true;
			}
			else if (((int)EdgeType & (int)IMeshWireframeSource::EMeshEdgeType::MeshBoundary) != 0 && Component->bEnableBoundaryEdges)
			{
				bEdgeIsVisible = true;
			}
			else if (((int)EdgeType & (int)IMeshWireframeSource::EMeshEdgeType::UVSeam) != 0 && Component->bEnableUVSeams)
			{
				bEdgeIsVisible = true;
			}
			else if (((int)EdgeType & (int)IMeshWireframeSource::EMeshEdgeType::NormalSeam) != 0 && Component->bEnableNormalSeams)
			{
				bEdgeIsVisible = true;
			}

			if (bEdgeIsVisible)
			{
				CurrentEdgeSet.Add(FIndex4i(li, VertIndexA, VertIndexB, (int)EdgeType));
			}
		};
		int32 NumEdges = CurrentEdgeSet.Num();
		if (NumEdges == 0)
		{
			return;
		}

		const int32 NumLineVertices = NumEdges * 4;
		const int32 NumLineIndices = NumEdges * 6;
		const int32 NumTextureCoordinates = 1;

		VertexBuffers.PositionVertexBuffer.Init(NumLineVertices);
		VertexBuffers.StaticMeshVertexBuffer.Init(NumLineVertices, NumTextureCoordinates);
		VertexBuffers.ColorVertexBuffer.Init(NumLineVertices);
		IndexBuffer.Indices.SetNumUninitialized(NumLineIndices);

		MeshBatchDatas.Emplace();
		FWireframeLinesMeshBatchData& MeshBatchData = MeshBatchDatas.Last();
		MeshBatchData.MinVertexIndex = 0;
		MeshBatchData.MaxVertexIndex = 0 + NumLineVertices - 1;
		MeshBatchData.StartIndex = 0;
		MeshBatchData.NumPrimitives = NumEdges * 2;
		if (Component->GetMaterial(0) != nullptr)
		{
			MeshBatchData.MaterialProxy = Component->GetMaterial(0)->GetRenderProxy();
		}
		else
		{
			MeshBatchData.MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		}
		
		FColor RegularEdgeColor = FLinearColor::FromSRGBColor(Component->WireframeColor).ToFColor(false);
		float RegularEdgeThickness = Component->ThicknessScale * Component->WireframeThickness;
		FColor BoundaryEdgeColor = FLinearColor::FromSRGBColor(Component->BoundaryEdgeColor).ToFColor(false);
		float BoundaryEdgeThickness = Component->ThicknessScale * Component->BoundaryEdgeThickness;
		FColor UVSeamColor = FLinearColor::FromSRGBColor(Component->UVSeamColor).ToFColor(false);
		float UVSeamThickness = Component->ThicknessScale * Component->UVSeamThickness;
		FColor NormalSeamColor = FLinearColor::FromSRGBColor(Component->NormalSeamColor).ToFColor(false);
		float NormalSeamThickness = Component->ThicknessScale * Component->NormalSeamThickness;

		float LineDepthBias = Component->LineDepthBias * Component->LineDepthBiasSizeScale;

		// Initialize lines.
		// Lines are represented as two tris of zero thickness. The UV's stored at vertices are actually (lineThickness, depthBias), 
		// which the material unpacks and uses to thicken the polygons and set the pixel depth bias.
		ParallelFor(NumEdges, [&](int32 idx)
		{
			int32 VertexBufferIndex = idx * 4;
			int32 IndexBufferIndex = idx * 6;

			FIndex4i EdgeInfo = CurrentEdgeSet[idx];
			IMeshWireframeSource::EMeshEdgeType EdgeType = (IMeshWireframeSource::EMeshEdgeType)EdgeInfo.D;

			float UseThickness = RegularEdgeThickness;
			FColor UseColor = RegularEdgeColor;

			bool bIsRegularEdge = (EdgeType == IMeshWireframeSource::EMeshEdgeType::Regular);
			bool bIsBoundaryEdge = (((int)EdgeType & (int)IMeshWireframeSource::EMeshEdgeType::MeshBoundary) != 0);
			if (!bIsRegularEdge)
			{
				if (bIsBoundaryEdge && Component->bEnableBoundaryEdges)
				{
					UseThickness = BoundaryEdgeThickness;
					UseColor = BoundaryEdgeColor;
				}
				else if (((int)EdgeType & (int)IMeshWireframeSource::EMeshEdgeType::UVSeam) != 0 && Component->bEnableUVSeams)
				{
					UseThickness = (bIsBoundaryEdge) ? BoundaryEdgeThickness : UVSeamThickness;
					UseColor = UVSeamColor;
				}
				else if (((int)EdgeType & (int)IMeshWireframeSource::EMeshEdgeType::NormalSeam) != 0 && Component->bEnableNormalSeams)
				{
					UseThickness = (bIsBoundaryEdge) ? BoundaryEdgeThickness : NormalSeamThickness;
					UseColor = NormalSeamColor;
				}
			}

			const FVector A = WireSource->GetVertex(EdgeInfo.B);
			const FVector B = WireSource->GetVertex(EdgeInfo.C);
			const FVector LineDirection = (B - A).GetSafeNormal();
			const FVector2D UV(UseThickness, LineDepthBias);

			VertexBuffers.PositionVertexBuffer.VertexPosition(VertexBufferIndex + 0) = A;
			VertexBuffers.PositionVertexBuffer.VertexPosition(VertexBufferIndex + 1) = B;
			VertexBuffers.PositionVertexBuffer.VertexPosition(VertexBufferIndex + 2) = B;
			VertexBuffers.PositionVertexBuffer.VertexPosition(VertexBufferIndex + 3) = A;

			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(VertexBufferIndex + 0, FVector::ZeroVector, FVector::ZeroVector, -LineDirection);
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(VertexBufferIndex + 1, FVector::ZeroVector, FVector::ZeroVector, -LineDirection);
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(VertexBufferIndex + 2, FVector::ZeroVector, FVector::ZeroVector, LineDirection);
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(VertexBufferIndex + 3, FVector::ZeroVector, FVector::ZeroVector, LineDirection);

			VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexBufferIndex + 0, 0, UV);
			VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexBufferIndex + 1, 0, UV);
			VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexBufferIndex + 2, 0, UV);
			VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(VertexBufferIndex + 3, 0, UV);

			// The color stored in the vertices actually gets interpreted as a linear color by the material,
			// whereas it is more convenient for the user of the MeshWireframe to specify colors as sRGB. So we actually
			// have to convert it back to linear. The ToFColor(false) call just scales back into 0-255 space.
			VertexBuffers.ColorVertexBuffer.VertexColor(VertexBufferIndex + 0) = UseColor;
			VertexBuffers.ColorVertexBuffer.VertexColor(VertexBufferIndex + 1) = UseColor;
			VertexBuffers.ColorVertexBuffer.VertexColor(VertexBufferIndex + 2) = UseColor;
			VertexBuffers.ColorVertexBuffer.VertexColor(VertexBufferIndex + 3) = UseColor;

			IndexBuffer.Indices[IndexBufferIndex + 0] = VertexBufferIndex + 0;
			IndexBuffer.Indices[IndexBufferIndex + 1] = VertexBufferIndex + 1;
			IndexBuffer.Indices[IndexBufferIndex + 2] = VertexBufferIndex + 2;
			IndexBuffer.Indices[IndexBufferIndex + 3] = VertexBufferIndex + 2;
			IndexBuffer.Indices[IndexBufferIndex + 4] = VertexBufferIndex + 3;
			IndexBuffer.Indices[IndexBufferIndex + 5] = VertexBufferIndex + 0;
		});

		ENQUEUE_RENDER_COMMAND(MeshWireframeVertexBuffersInit)(
			[this](FRHICommandListImmediate& RHICmdList)
		{
			VertexBuffers.PositionVertexBuffer.InitResource();
			VertexBuffers.StaticMeshVertexBuffer.InitResource();
			VertexBuffers.ColorVertexBuffer.InitResource();

			FLocalVertexFactory::FDataType Data;
			VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(&VertexFactory, Data);
			VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(&VertexFactory, Data);
			VertexBuffers.StaticMeshVertexBuffer.BindTexCoordVertexBuffer(&VertexFactory, Data);
			VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(&VertexFactory, Data);
			VertexFactory.SetData(Data);

			VertexFactory.InitResource();
			IndexBuffer.InitResource();
		});
	}

	virtual ~FMeshWireframeSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, 
		const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_OverlaySceneProxy_GetDynamicMeshElements);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				for (const FWireframeLinesMeshBatchData& MeshBatchData : MeshBatchDatas)
				{
					const FSceneView* View = Views[ViewIndex];
					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.IndexBuffer = &IndexBuffer;
					Mesh.bWireframe = false;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = MeshBatchData.MaterialProxy;

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), false, false, DrawsVelocity(), false);
					BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					BatchElement.FirstIndex = MeshBatchData.StartIndex;
					BatchElement.NumPrimitives = MeshBatchData.NumPrimitives;
					BatchElement.MinVertexIndex = MeshBatchData.MinVertexIndex;
					BatchElement.MaxVertexIndex = MeshBatchData.MaxVertexIndex;
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }

	uint32 GetAllocatedSize() const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

	virtual SIZE_T GetTypeHash() const override
	{
		static SIZE_T UniquePointer;
		return reinterpret_cast<SIZE_T>(&UniquePointer);
	}

private:
	TArray<FWireframeLinesMeshBatchData> MeshBatchDatas;
	FMaterialRelevance MaterialRelevance;
	FLocalVertexFactory VertexFactory;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;

	TArray<FIndex4i> CurrentEdgeSet;
};


UMeshWireframeComponent::UMeshWireframeComponent()
{
	CastShadow = false;
	bSelectable = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void UMeshWireframeComponent::SetWireframeSourceProvider(TSharedPtr<IMeshWireframeSourceProvider> Provider)
{
	SourceProvider = Provider;
	if (SourceProvider)
	{
		SourceProvider->AccessMesh([&](const IMeshWireframeSource& Source)
		{
			this->LocalBounds = Source.GetBounds();
		});
	}
	MarkRenderStateDirty();
}

void UMeshWireframeComponent::SetLineMaterial(UMaterialInterface* InLineMaterial)
{
	LineMaterial = InLineMaterial;
	SetMaterial(0, InLineMaterial);
}



FPrimitiveSceneProxy* UMeshWireframeComponent::CreateSceneProxy()
{
	if (SourceProvider)
	{
		FMeshWireframeSceneProxy* NewProxy = nullptr;
		SourceProvider->AccessMesh([this, &NewProxy](const IMeshWireframeSource& Source)
		{
			NewProxy = new FMeshWireframeSceneProxy(this, &Source);
		});
		return NewProxy;
	}
	return nullptr;
}

int32 UMeshWireframeComponent::GetNumMaterials() const
{
	return 1;
}

FBoxSphereBounds UMeshWireframeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return LocalBounds.TransformBy(LocalToWorld);
}

